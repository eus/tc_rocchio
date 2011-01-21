#include <stdio.h>
#include "utility.h"

CLEANUP_BEGIN
CLEANUP_END

MAIN_BEGIN(
"reader_vec",
"Display the binary content of input file(s) having the following structure\n"
"whose endianness expected to be that of the host machine:\n"
"+------------------------------------------------------------------+\n"
"| N in unsigned int (4 bytes)                                      |\n"
"+------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated string | w_1_1 in double | ... | w_N_1 in double |\n"
"+------------------------+-----------------+-----+-----------------+\n"
"|                               ...                                |\n"
"+------------------------------------------------------------------+\n"
"in the following human-readable form:\n"
"INPUT_FILE_NAME: (STRING( FLOATING_POINT_NUMBER)*)?\\n\n"
"If there is only one file, `INPUT_FILE_NAME: ' is not output.\n",
"",
"",
1,
NO_MORE_CASE
)
  int use_file_name = 1;
  if (argv[optind] == NULL || argv[optind + 1] == NULL) { // Only one input file
    use_file_name = 0;
  }
MAIN_INPUT_START
{
  double vec_element;
  size_t block_read;
  int c;
  unsigned int M;

  block_read = fread(&M, 1, sizeof(M), in_stream);
  if (block_read == 0) { // Empty input stream
    continue;
  } else if (block_read != sizeof(M)) {
    fatal_error("Input file %s has malformed structure: "
		"corrupted vector size", in_stream_name);
  }

  while (1) {

    c = fgetc(in_stream);
    if (c == EOF) {
      break;
    }

    if (use_file_name) {
      fprintf(out_stream, "%s: ", in_stream_name);
    }

    while (1) {
      fputc(c, out_stream);

      c = fgetc(in_stream);
      if (c == EOF) {
	fatal_error("Input file %s has malformed structure: corrupted string",
		    in_stream_name);
      } else if (c == '\0') {
	break;
      }
    }

    unsigned int count = 0;
    while (count < M) {
      block_read = fread(&vec_element, 1, sizeof(vec_element), in_stream);
      if (block_read == 0) {
	fatal_error("Input file %s has malformed structure: "
		    "vector stops at element #%u", in_stream_name, count + 1);
      } else if (block_read == sizeof(vec_element)) {
	fprintf(out_stream, " %f", vec_element);
      } else {
	fatal_error("Input file %s has malformed structure: "
		    "corrupted vector element #%u",
		    in_stream_name, count + 1);
      }
      count++;
    }

    fputc('\n', out_stream);
  }
}
MAIN_INPUT_END
MAIN_END
