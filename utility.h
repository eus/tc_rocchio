#ifndef UTILITY_H
#define UTILITY_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NO_MORE_CASE default:					\
  fatal_error("%c is invalid option (-h for help)", optopt);	\

#define MAIN_BEGIN(name, usage_note, more_opt, more_opt_help,		\
		   is_multiple_input, cases)				\
  int main(int argc, char **argv, char **envp) {			\
    int optchar;							\
    prog_name = name;							\
    in_stream = stdin;							\
    out_stream = stdout;						\
    multiple_input = is_multiple_input;					\
    if (atexit(cleanup) != 0) {						\
      fatal_error("Cannot register clean-up handler at exit");		\
    }									\
    /* Parse command line arguments */					\
    opterr = 0;								\
    while (1) {								\
      optchar = getopt(argc, argv, ":o:h" more_opt);			\
      if (optchar == -1) {						\
	break;								\
      } else if (optchar == ':') {					\
	fatal_error("%c needs an argument (-h for help)", optopt);	\
      } else if (optchar == '?') {					\
	fatal_error("%c is invalid option (-h for help)", optopt);	\
      } else {								\
	switch (optchar) {						\
	case 'o':							\
	  if (out_stream != stdout) {					\
	    if (fclose(out_stream) != 0) {				\
	      fatal_syserror("Cannot close output %s", out_stream_name); \
	    }								\
	  }								\
	  out_stream = fopen(optarg, "w");				\
	  if (out_stream == NULL) {					\
	    fatal_syserror("Cannot open output %s for writing", optarg); \
	  }								\
	  out_stream_name = optarg;					\
	  break;							\
	case 'h':							\
	  fprintf(stderr, "Usage: %s " more_opt_help			\
		  " [-o OUTPUT_FILE] [INPUT_FILE]%s\n" usage_note,	\
		  prog_name,						\
		  is_multiple_input ? " ..." : "");			\
	  exit(EXIT_SUCCESS);						\
	  break;							\
	cases								\
	}								\
      }									\
    }									\
    /* End of parsing */

#define MAIN_INPUT_START						\
  do {									\
    if (argv[optind] != NULL) {						\
      if (in_stream != stdin) {						\
	if (fclose(in_stream) != 0) {					\
	  fatal_syserror("Cannot close input %s", in_stream_name);	\
	}								\
      }									\
      in_stream = fopen(argv[optind], "r");				\
      if (in_stream == NULL) {						\
	fatal_syserror("Cannot open input %s for reading", argv[optind]); \
      }									\
      in_stream_name = argv[optind];					\
      optind++;								\
    }
 
#define MAIN_INPUT_END } while (multiple_input && argv[optind] != NULL);

#define MAIN_END exit(EXIT_SUCCESS); }

#define CLEANUP_BEGIN void cleanup(void) {				\
  if (in_stream != NULL && in_stream != stdin) {			\
    if (fclose(in_stream) != 0) {					\
      print_syserror("Cannot close input %s", in_stream_name);		\
    }									\
  }									\
  if (out_stream != NULL && out_stream != stdout) {			\
    if (fclose(out_stream) != 0) {					\
      print_syserror("Cannot close output %s", out_stream_name);	\
    }									\
  }

#define CLEANUP_END }

#define fatal_error_hdr() fprintf(stderr, "%s[%lu]: ",			\
				  prog_name, (unsigned long) getpid())

#define fatal_error(msg, ...)			\
  do {						\
    fatal_error_hdr();				\
    fprintf(stderr, msg "\n", ## __VA_ARGS__);	\
    exit (EXIT_FAILURE);			\
  } while (0)

#define print_error(msg, ...)					\
  do {								\
    fatal_error_hdr();						\
    fprintf(stderr, msg "\n", ## __VA_ARGS__);			\
  } while (0)

#define fatal_syserror(msg, ...)				\
  do {								\
    err_msg = strerror(errno);					\
    fatal_error_hdr();						\
    fprintf(stderr, msg " (%s)\n", ## __VA_ARGS__, err_msg);	\
    exit (EXIT_FAILURE);					\
  } while (0)

#define print_syserror(msg, ...)				\
  do {								\
    err_msg = strerror(errno);					\
    fatal_error_hdr();						\
    fprintf(stderr, msg " (%s)\n", ## __VA_ARGS__, err_msg);	\
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

static const char *prog_name;
static FILE *out_stream = NULL;
static const char *out_stream_name = NULL;
static FILE *in_stream = NULL;
static const char *in_stream_name = NULL;
static char *err_msg = NULL;
static int multiple_input = 0;

/**
 * @return zero if nothing is read from input stream or non-zero otherwise
 */
static inline int load_next_text(char *buffer, size_t buffer_size)
{
  size_t block_read;

  memset(buffer, 0, buffer_size); // Clear tokenizing buffer
  block_read = fread(buffer, buffer_size - 1, 1, in_stream); // Read next chars
  if (ferror(in_stream)) {
    fatal_error("Error reading input stream");
  }

  return (block_read || buffer[0] != '\0');
}

/**
 * Tokenize the input stream and call the callback functions for each token.
 * The callback function is free to modify the passed token as long as the
 * function does not modify the passed token beyond its original length.
 * The passed token will be modified once the callback function goes out of
 * scope. So, if the callback function wants to store the token, the function
 * must copy the token to its own memory.
 *
 * Test case:
 * Set buffer_size to 3 and feed when delimiter is " \n":
 * - " 12 3 4\n" [Test Case (TC) 1]
 * - " 12 34 \n" [Test Case (TC) 2]
 * - "12  3" [Test Case (TC) 3]
 *
 * @param delimiter the token delimiter
 * @param buffer the tokenizing buffer allocated by the caller
 * @param buffer_size the size of the tokenizing buffer
 * @param partial_fn is called to process a token that can be incomplete
 * @param complete_fn is called to indicate that one or more tokens passed to
 * partial_fn forms one whole token.
 *
 * @return zero if the input stream is empty or non-zero otherwise.
 */
static inline int tokenizer(const char *delimiter,
			    char *buffer, size_t buffer_size,
			    void (*partial_fn)(char *),
			    void (*complete_fn)(void))
{
  char *f;
  int has_text;

  has_text = load_next_text(buffer, buffer_size); // Read text from input stream
  if (!has_text) { // Empty input
    return 0;
  }
  
  do {
    int token_exists = 0; /* Last word exists although it may or may not be
			     truncated so that at least it makes sense to
			     output \n. If no word exists, it is totally wrong
			     to output \n. [TC 2] */
    int not_truncated = 0; /* Last word in tokenizing buffer is not truncated
			      [TC 1] */

    if (strchr(delimiter, buffer[strlen(buffer) - 1]) != NULL) {
      not_truncated = 1; /* Need to be obtained first because strtok() will
			    mess the buffer up */
    }

    /* Tokenize the text in the tokenizing buffer */
    f = strtok(buffer, delimiter);
    while (f != NULL) {
      partial_fn(f);

      f = strtok(NULL, delimiter);
      if (f != NULL) { // Avoid truncating word at the end of tokenizing buffer
	complete_fn();
      } else {
	if (not_truncated) {
	  complete_fn();
	} else {
	  token_exists = 1;
	}
      }
    }
    /* End of tokenizing */

    has_text = load_next_text(buffer, buffer_size); // Read next text

    /* Fix truncated word at the end of tokenizing buffer */
    if (token_exists && strchr(delimiter, buffer[0]) != NULL) {
      complete_fn();
    }
    /* Finish fixing */

  } while (has_text); // block_read is zero even for partial input [TC 3]

  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_H */
