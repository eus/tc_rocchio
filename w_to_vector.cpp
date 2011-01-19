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

#include <unordered_map>
#include <csignal>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include "utility.h"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static unordered_map<string, pair<unsigned int, double>> idf_list;
static list<pair<string, double>> w_list;
static string word;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  int pos = word.find(' ');

  if (pos == -1) {
    fatal_error("%s is a malformed input", word.c_str());
  } else {
    w_list.push_back(pair<string, double>
		     (word.substr(0, pos),
		      strtod(word.substr(pos + 1).c_str(), NULL)));
  }

  word.clear();
}

static inline void load_idf_dic_file(const char *filename)
{
  in_stream = fopen(filename, "r");
  in_stream_name = filename;
  if (in_stream == NULL) {
    fatal_syserror("Cannot open IDF_DIC file %s", filename);
  }

  size_t block_read;
  unsigned int i = 0;
  unsigned int count;
  block_read = fread(&count, sizeof(count), 1, in_stream);
  if (block_read == 0) {
    fatal_error("Malformed input: cannot read record count");
  }
  if (count == 0) { // No record
    exit(EXIT_SUCCESS);
  }

  unsigned int delta = 0;
  double idf;
  int idf_not_truncated = 1;
  while (count > 0) {
    block_read = fread(buffer, 1, BUFFER_SIZE - 1, in_stream);
    if (block_read == 0) {
      fatal_error("Malformed input: record count is less than stated");
    }
    buffer[BUFFER_SIZE - 1] = '\0'; // Prevent buffer overflow
    unsigned int offset = 0;
    while (count > 0) {
      if (idf_not_truncated) {
	word.append(buffer + offset);
	offset += strlen(buffer + offset);
	if (offset == block_read) { // Truncated word
	  break;
	}

	offset++;

	if (offset + sizeof(idf) > block_read) { // Truncated IDF
	  delta = block_read - offset;
	  memcpy(&idf, buffer + offset, delta);
	  idf_not_truncated = 0;
	  break;
	}

	memcpy(&idf, buffer + offset, sizeof(idf));
	offset += sizeof(idf);
      } else {
	if (block_read + delta < sizeof(idf)) { // IDF is still truncated
	  memcpy(reinterpret_cast<char *>(&idf) + delta, buffer, block_read);
	  delta += block_read;
	  break;
	} else { // IDF can be completed now
	  offset = sizeof(idf) - delta;
	  memcpy(reinterpret_cast<char *>(&idf) + delta, buffer, offset);
	  idf_not_truncated = 1;
	}
      }

      pair<unsigned int, double> &data = idf_list[word];
      data.first = i;
      data.second = idf;

      i++;
      count--;
      word.clear();
    }
  }
}

MAIN_BEGIN(
"w_to_vector",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file(s) is read for input.\n"
"Then, the input stream is expected to have the following form:\n"
"WORD WORD_COUNT\\n\n"
"Logically, each file should come from a TF processing unit in which\n"
"each TF processing unit produces a list of unique words in a document.\n"
"The mandatory option -D specifies the name of the IDF_DIC file generated\n"
"by the idf_dic processing unit whose expected structure is as follows:\n"
"+----------------------------------------------------------------+\n"
"| Record count N in unsigned int (4 bytes)                       |\n"
"+--------------------------------------+-------------------------+\n"
"| NULL-terminated sorted unique word 1 | IDF in double (8 bytes) |\n"
"+--------------------------------------+-------------------------+\n"
"|                                ...                             |\n"
"+--------------------------------------+-------------------------+\n"
"| NULL-terminated sorted unique word N | IDF in double (8 bytes) |\n"
"+--------------------------------------+-------------------------+\n"
"The endianness of the binary data is expected to be that of the host machine\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+--------------------------+\n"
"| NULL-terminated filename |\n"
"+--------------------------+\n"
"| w_1 in double (8 bytes)  |\n"
"+--------------------------+\n"
"|           ...            |\n"
"+--------------------------+\n"
"| w_N in double (8 bytes)  |\n"
"+--------------------------+\n"
"where N is the number of words in the dictionary produced by the idf_dic\n"
"processing unit. The NULL-terminated filename is of length 0 when the input\n"
"is stdin.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n",
"D:",
"-D IDF_DIC_FILE",
1,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_idf_dic_file(optarg);
break;
) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
}
MAIN_INPUT_START
{
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);

  double normalizer = 0;
  list<pair<unsigned int, double>> valid_w_list;
  for (list<pair<string, double>>::iterator i = w_list.begin();
       i != w_list.end();
       ++i) {

    unordered_map<string, pair<unsigned int, double>>::iterator j
      = idf_list.find(i->first);

    if (j == idf_list.end()) {
      continue;
    } else { // Calculation of a feature's weight
      unsigned int pos = j->second.first;
      double tf_idf = i->second * j->second.second;

      valid_w_list.push_back(pair<unsigned int, double>(pos, tf_idf));
      normalizer += tf_idf * tf_idf;
    }
  }
  normalizer = sqrt(normalizer);

  valid_w_list.sort();

  if (in_stream_name == NULL) {
    fprintf(out_stream, "%c", '\0');
  } else {
    fprintf(out_stream, "%s%c", in_stream_name, '\0');
  }

  unsigned int dic_size = idf_list.size();
  list<pair<unsigned int, double>>::iterator j = valid_w_list.begin();
  for (unsigned int i = 0; i < dic_size; i++) {
    double weight = 0.0;

    if (i == j->first) { // This word exists in the input stream
      weight = j->second / normalizer;

      j++;
    }

    if (fwrite(&weight, sizeof(weight), 1, out_stream) == 0) {
      fatal_syserror("Cannot write weight to output stream");
    }
  }

  w_list.clear();
  word.clear();
}
MAIN_INPUT_END
MAIN_END
