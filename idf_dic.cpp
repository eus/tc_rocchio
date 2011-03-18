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
#include "utility.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

typedef unordered_map<string, unsigned int> class_idf_list;
static class_idf_list idf_list;
static string word;
typedef list<string> class_word_sorter;
static class_word_sorter word_sorter;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  unsigned int pos = word.find(' ');

  if (pos != word.npos) {
    word = word.substr(0, pos);
  }

  idf_list[word] += 1;
  word_sorter.push_back(word);

  word.clear();
}

MAIN_BEGIN(
"idf_dic",
"If input file is not given, stdin is read for a list of paths of input files."
"\n"
"Otherwise, the input file is read for such a list.\n"
"Then, the each of the file is expected to have the following form:\n"
"WORD( FIELD)*\\n\n"
"in which only WORD (i.e., the first field separated by space) matters.\n"
"Logically, data should come from one or more TF processing unit in which\n"
"each TF processing unit produces a list of unique words so that the number\n"
"of duplicates of a word in the input stream can be taken as the number of\n"
"documents having that particular word.\n"
"Then, this processing unit calculates the IDF of each word f as follows:\n"
"                M + 1\n"
"IDF(f) = ln -------------\n"
"            #doc_having_f\n"
"Where M specifies the total number of paths (i.e., one document/path) in\n"
"the input stream.\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+-----------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int               |\n"
"| (4 bytes): always 2                                                   |\n"
"+-----------------------------------------------------------------------+\n"
"| NULL-terminated string with value: M |C|off_1| M                      |\n"
"+--------------------------------------+-+-----+-----+-----+------------+\n"
"| NULL-terminated sorted unique word 1 |Q|off_1|IDF_1|off_2|doc_count_1 |\n"
"+--------------------------------------+-+-----+-----+-----+------------+\n"
"|                                   ...                                 |\n"
"+--------------------------------------+-+-----+-----+-----+------------+\n"
"| NULL-terminated sorted unique word N |Q|off_1|IDF_N|off_2|doc_count_N |\n"
"+--------------------------------------+-+-----+-----+-----+------------+\n"
"C is an unsigned int (4 bytes) datum whose value is 1.\n"
"Q is an unsigned int (4 bytes) datum whose value is 2.\n"
"off_1 is an unsigned int (4 bytes) datum whose value is 0.\n"
"off_2 is an unsigned int (4 bytes) datum whose value is 1.\n"
"M is a double (8 bytes) datum whose value is the number of documents used as\n"
"the numerator in the IDF calculation.\n"
"IDF_i is a double (8 bytes) datum whose value is the number of documents\n"
"having the word i.\n"
"word_count_i is a double (8 bytes) datum whose value is the number of\n"
"occurence of word i.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, the binary output is output to stdout.\n",
"",
"",
0,
NO_MORE_CASE
)

  unsigned long M = 0;

  /* Allocating tokenizing buffer */
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }
  /* End of allocation */

MAIN_INPUT_START
MAIN_LIST_OF_FILE_START
{
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
  M++;
}
MAIN_LIST_OF_FILE_END
MAIN_INPUT_END
{
  /* If requested to write an output file, output the header */
  size_t block_write;
  unsigned int count = 2;
  block_write = fwrite(&count, sizeof(count), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }

  /* End of outputting header */

  /* Output M */
  struct output_M {
    char name[2];
    unsigned int C;
    struct sparse_vector_entry e;
  } __attribute__((packed));
  struct output_M out_M;

  strcpy(out_M.name, "M");
  out_M.C = 1;
  out_M.e.offset = 0;
  out_M.e.value = M;
  block_write = fwrite(&out_M, sizeof(out_M), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write M to output stream");
  }

  /* Calculating IDF and the word position in the vector and outputing to
   * a file if requested
   */
  word_sorter.sort();
  word_sorter.unique();

  struct output {
    unsigned int Q;
    struct sparse_vector_entry e;
    struct sparse_vector_entry doc_count;
  } __attribute__((packed));

  struct output o;
  o.Q = 2;
  o.e.offset = 0;
  o.doc_count.offset = 1;
  for (class_word_sorter::iterator i = word_sorter.begin();
       i != word_sorter.end();
       ++i) {

    block_write = fwrite(i->c_str(), i->length() + 1, 1, out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write word to output stream");
    }

    o.doc_count.value = idf_list[*i];
    o.e.value = log(static_cast<double>(M + 1) / o.doc_count.value);
    block_write = fwrite(&o, sizeof(o), 1, out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write IDF to output stream");
    }
  }
  /* End of IDF and word position in the vector calculations */
} MAIN_END
