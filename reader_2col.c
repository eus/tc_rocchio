#include <stdio.h>
#include "utility.h"

CLEANUP_BEGIN
CLEANUP_END

MAIN_BEGIN(
"reader_2col",
"Display the binary content of input file(s) having the following structure:\n"
"+-----------------------------------------------------------------------+\n"
"| Record count N as a 4 byte unsigned int in host endianness            |\n"
"+--------------------------+--------------------------------------------+\n"
"| NULL-terminated string 1 | 8 bytes of double value in host endianness |\n"
"+--------------------------+--------------------------------------------+\n"
"|                                  ...                                  |\n"
"+--------------------------+--------------------------------------------+\n"
"| NULL-terminated string N | 8 bytes of double value in host endianness |\n"
"+--------------------------+--------------------------------------------+\n"
"in the following human-readable form:\n"
"-- INPUT_FILE_NAME\\n\n"
"STRING_1 FLOATING_POINT_NUMBER\\n\n"
"STRING_N FLOATING_POINT_NUMBER\\n\n"
"If there is only one file, `-- INPUT_FILE_NAME\\n' is not output.\n",
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
  unsigned int count;
  int c;
  double idf;
  size_t block_read;
  
  block_read = fread(&count, sizeof(count), 1, in_stream);
  if (block_read == 0) {
    fatal_syserror("Cannot read count");
  }

  if (use_file_name) {
    fprintf(out_stream, "-- %s\n", in_stream_name);
  }

  while (count-- > 0) {
    while ((c = fgetc(in_stream)) != '\0' && c != EOF) {
      fputc(c, out_stream);
    }
    block_read = fread(&idf, sizeof(idf), 1, in_stream);
    if (block_read == 0) {
      fatal_syserror("Cannot read IDF #%u", count + 1);
    }
    fprintf(out_stream, " %f\n", idf);
  }

}
MAIN_INPUT_END
MAIN_END
