#include <stdio.h>
#include "utility.h"

CLEANUP_BEGIN
CLEANUP_END

static int show_normal = 0;
MAIN_BEGIN(
"reader_vec",
"Display the binary content of input file(s) having the following sparse\n"
"vector structure whose endianness expected to be that of the host machine:\n"
"+-------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes) |\n"
"+-----------------------------+---+-------+-----+-----+-------+-----+\n"
"| NULL-terminated string      | Q | off_1 | w_1 | ... | off_Q | w_Q |\n"
"+-----------------------------+---+-------+-----+-----+-------+-----+\n"
"|                                ...                                |\n"
"+-------------------------------------------------------------------+\n"
"in the following human-readable form of the sparse vector:\n"
"INPUT_FILE_NAME: (STRING( INT_NUMBER FLOATING_POINT_NUMBER)*)?\\n\n"
"If -n (i.e., normal) is given, the following human-readable form of the\n"
"normal vector representation is used instead:\n"
"INPUT_FILE_NAME: (STRING( FLOATING_POINT_NUMBER)*)?\\n\n"
"If there is only one file, `INPUT_FILE_NAME: ' is not output.\n",
"n",
"-n",
1,
case 'n':
show_normal = 1;
break;
)
  int use_file_name = 1;
  if (argv[optind] == NULL || argv[optind + 1] == NULL) { // Only one input file
    use_file_name = 0;
  }
MAIN_INPUT_START
{
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
  unsigned int record_count = 0;

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

    unsigned int Q;
    block_read = fread(&Q, 1, sizeof(Q), in_stream);
    if (block_read == 0) {
      fatal_error("Input file %s has malformed structure: "
		  "record #%u is corrupted", in_stream_name, record_count + 1);
    } else if (block_read != sizeof(Q)) {
      fatal_error("Input file %s has malformed structure: "
		  "corrupted offset count at record #%u",
		  in_stream_name, record_count + 1);
    }
    
    unsigned int prev_offset = 0;
    unsigned int count = 0;
    struct sparse_vector_entry e;
    while (count < Q) {
      block_read = fread(&e, 1, sizeof(e), in_stream);
      if (block_read == 0) {
	fatal_error("Input file %s has malformed structure: "
		    "record #%u misses entry at offset #%u",
		    in_stream_name, record_count + 1, count + 1);
      } else if (block_read == sizeof(e)) {
	if (show_normal) {
	  for (; prev_offset < e.offset; prev_offset++) {
	    fprintf(out_stream, " %f", 0.0);
	  }
	  prev_offset++;
	  fprintf(out_stream, " %f", e.value);
	} else {
	  fprintf(out_stream, " %u %f", e.offset, e.value);
	}
      } else {
	fatal_error("Input file %s has malformed structure: "
		    "record #%u is corrupted at entry #%u",
		    in_stream_name, record_count + 1, count + 1);
      }
      count++;
    }
    if (show_normal) {
      for (; prev_offset < M; prev_offset++) {
	fprintf(out_stream, " %f", 0.0);
      }
    }
    fputc('\n', out_stream);
    record_count++;
  }
}
MAIN_INPUT_END
MAIN_END
