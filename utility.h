/*****************************************************************************
 * Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#ifndef UTILITY_H
#define UTILITY_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FP_COMPARISON_DELTA 1e-15

#define NO_MORE_CASE default:					\
  fatal_error("%c is invalid option (-h for help)", optopt);	\

#define MAIN_BEGIN_OPT_OUT(name, usage_note, more_opt, more_opt_help,	\
			   is_multiple_input, is_no_out, cases)		\
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
      if (is_no_out) {							\
	optchar = getopt(argc, argv, ":h" more_opt);			\
      } else {								\
	optchar = getopt(argc, argv, ":o:h" more_opt);			\
      }									\
      if (optchar == -1) {						\
	break;								\
      } else if (optchar == ':') {					\
	fatal_error("%c needs an argument (-h for help)", optopt);	\
      } else if (optchar == '?') {					\
	fatal_error("%c is invalid option (-h for help)", optopt);	\
      } else {								\
	switch (optchar) {						\
	case 'h':							\
	  fprintf(stderr, "Usage: %s " more_opt_help			\
		  "%s [INPUT_FILE]%s\n" usage_note,			\
		  prog_name,						\
		  is_no_out ? "" : " [-o OUTPUT_FILE]",			\
		  is_multiple_input ? " ..." : "");			\
	  return EXIT_SUCCESS;						\
	cases								\
	}								\
      }									\
    }									\
    /* End of parsing */

#define MAIN_BEGIN(name, usage_note, more_opt, more_opt_help,	\
		   is_multiple_input, cases)			\
  MAIN_BEGIN_OPT_OUT(name, usage_note, more_opt, more_opt_help,	\
		     is_multiple_input, 0, cases		\
  case 'o':							\
		     open_out_stream(optarg);			\
		     break;					\
)

#define MAIN_INPUT_START						\
  do {									\
    recover_stdin();							\
    if (argv[optind] != NULL) {						\
      open_in_stream(argv[optind]);					\
      optind++;								\
    }
 
#define MAIN_INPUT_END } while (multiple_input && argv[optind] != NULL);

#define MAIN_END return EXIT_SUCCESS; }

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
    exit(EXIT_FAILURE);				\
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
    exit(EXIT_FAILURE);						\
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

/* Return a pointer to the file name part of the path. */
static inline const char *get_file_name(const char *path)
{
  const char *pos = strrchr(path, OS_PATH_DELIMITER);
  if (pos == NULL) {
    // The path contains no leading directories
    return path;
  } else {
    return pos + 1;
  }
}

struct sparse_vector_entry {
  unsigned int offset;
  double value;
} __attribute__((packed));

static const char *prog_name;
static FILE *out_stream = NULL;
static const char *out_stream_name = NULL;
static FILE *in_stream = NULL;
static const char *in_stream_name = NULL;
static char *err_msg = NULL;
static int multiple_input = 0;

/* When people use in_stream during options processing, they should have
 * restored it back to the original state. But, doing so manually is
 * unproductive.
 */
static inline void recover_stdin(void)
{
  if (in_stream != stdin) {
    if (fclose(in_stream) != 0) {
      fatal_syserror("Cannot close input %s", in_stream_name);
    }

    in_stream = stdin;
    in_stream_name = NULL;
  }
}

static inline void recover_stdout(void)
{
  if (out_stream != stdout) {
    if (fclose(out_stream) != 0) {
      fatal_syserror("Cannot close output %s", out_stream_name);
    }

    out_stream = stdout;
    out_stream_name = NULL;
  }
}

static inline void open_in_stream(const char *path)
{
  if (in_stream != stdin) {
    if (fclose(in_stream) != 0) {
      fatal_syserror("Cannot close input %s", in_stream_name);
    }
  }
  in_stream_name = path;
  in_stream = fopen(in_stream_name, "r");
  if (in_stream == NULL) {
    fatal_syserror("Cannot open input %s for reading", in_stream_name);
  }
}

static inline void open_out_stream(const char *path)
{
  if (out_stream != stdout) {
    if (fclose(out_stream) != 0) {
      fatal_syserror("Cannot close output %s", out_stream_name);
    }
  }
  out_stream_name = path;
  out_stream = fopen(out_stream_name, "w");
  if (out_stream == NULL) {
    fatal_syserror("Cannot open output %s for writing", out_stream_name);
  }
}

/* Taken from http://eternallyconfuzzled.com/arts/jsw_art_rand.aspx
 * in public domain (2010 January 30)
 */
static inline double uniform_deviate(int seed)
{
  return seed * (1.0 / (RAND_MAX + 1.0));
}

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
