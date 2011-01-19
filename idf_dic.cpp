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

#include <list>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
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
static string word;
static list<string> word_sorter;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  int pos = word.find(' ');

  if (pos != -1) {
    word = word.substr(0, pos);
  }

  idf_list[word].second += 1;
  word_sorter.push_back(word);

  word.clear();
}

static unsigned long M = 0;
MAIN_BEGIN(
"idf_dic",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file(s) is read for input.\n"
"Then, the input stream is expected to have the following form:\n"
"WORD( FIELD)*\\n\n"
"in which only WORD (i.e., the first field separated by space) matters.\n"
"Logically, data should come from one or more TF processing unit in which\n"
"each TF processing unit produces a list of unique words so that the number\n"
"of duplicates of a word in the input stream can be taken as the number of\n"
"documents having that particular word.\n"
"The mandatory option -M specifies a positive non-zero integer specifying\n"
"the total number of documents, or equivalenty, the total number of connected\n"
"TF processing units. This is needed to calculate the inverse document\n"
"frequency (IDF).\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+----------------------------------------------------------------+\n"
"| Record count N in unsigned int (4 bytes)                       |\n"
"+--------------------------------------+-------------------------+\n"
"| NULL-terminated sorted unique word 1 | IDF in double (8 bytes) |\n"
"+--------------------------------------+-------------------------+\n"
"|                                ...                             |\n"
"+--------------------------------------+-------------------------+\n"
"| NULL-terminated sorted unique word N | IDF in double (8 bytes) |\n"
"+--------------------------------------+-------------------------+\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, the binary output is output to stdout.\n",
"M:",
"-M TOTAL_DOCUMENT_COUNT",
1,
case 'M':
M = strtoul(optarg, NULL, 10);
break;
) {
  if (M == 0) { // Check that M is greater than 0
    fatal_error("%lu is invalid total document count (-h for help)", M);
  }

  /* Allocating tokenizing buffer */
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }
  /* End of allocation */
}
MAIN_INPUT_START
{
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
}
MAIN_INPUT_END
{
  /* If requested to write an output file, output the header */
  size_t block_write;
  unsigned int count = idf_list.size();
  block_write = fwrite(&count, sizeof(count), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write to output stream");
  }

  /* End of outputting header */

  /* Calculating IDF and the word position in the vector and outputing to
   * a file if requested
   */
  word_sorter.sort();
  word_sorter.unique();

  unsigned int vec_at = 0;
  for (list<string>::const_iterator i = word_sorter.begin();
       i != word_sorter.end();
       ++i, ++vec_at) {

    pair<unsigned int, double> &data = idf_list[*i];
    data.first = vec_at;
    data.second = log10(static_cast<double>(M) / data.second);

    block_write = fwrite(i->c_str(), i->length() + 1, 1, out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write word to output stream");
    }
    block_write = fwrite(&data.second, sizeof(double), 1, out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write IDF to output stream");
    }
  }
  /* End of IDF and word position in the vector calculations */
} MAIN_END
