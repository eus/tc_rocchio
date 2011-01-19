#include <stdio.h>
#include "utility.h"

CLEANUP_BEGIN
CLEANUP_END

MAIN_BEGIN(
"reader_vec",
"Display the binary content of input file(s) having the following structure:\n"
"+--------------------------------------------+\n"
"| 8 bytes of double value in host endianness |\n"
"+--------------------------------------------+\n"
"|                     ...                    |\n"
"+--------------------------------------------+\n"
"| 8 bytes of double value in host endianness |\n"
"+--------------------------------------------+\n"
"in the following human-readable form:\n"
"INPUT_FILE_NAME:( FLOATING_POINT_NUMBER)*\\n\n"
"If there is only one file, `INPUT_FILE_NAME:' is not output.\n",
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
  unsigned int count = 0;

  if (use_file_name) {
    fprintf(out_stream, "%s:", in_stream_name);
  }

  while (1) {
    block_read = fread(&vec_element, 1, sizeof(vec_element), in_stream);
    if (block_read == 0) {
      break;
    } else if (block_read == sizeof(vec_element)) {
      fprintf(out_stream, " %f", vec_element);
    } else {
      if (count > 0) {
	fprintf(out_stream, "\n");
      }
      fatal_syserror("Corrupted vector element #%u", count + 1);
    }
    count++;
  }

  if (count > 0) {
    fprintf(out_stream, "\n");
  }
}
MAIN_INPUT_END
MAIN_END
