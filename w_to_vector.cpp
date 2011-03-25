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

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "utility.h"
#include "utility_vector.hpp"
#include "utility.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

typedef pair<string, double> class_w_entry;
typedef list<class_w_entry> class_w_list;
static class_w_list w_list;
static string word;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  unsigned int pos = word.find(' ');

  if (pos == word.npos) {
    fatal_error("%s is a malformed input", word.c_str());
  } else {
    w_list.push_back(class_w_entry());

    class_w_entry &e = w_list.back();
    e.first.append(word.substr(0, pos));
    e.second = strtod(word.substr(pos + 1).c_str(), NULL);
  }

  word.clear();
}

#include "utility_idf_dic.hpp"

MAIN_BEGIN(
"w_to_vector",
"If input file is not given, stdin is read for a list of paths of input files."
"\n"
"Otherwise, the input file is read for such a list.\n"
"Then, each of the file in the list is expected to have the following form:\n"
"WORD WORD_COUNT\\n\n"
"Logically, each file should come from a TF processing unit in which\n"
"each TF processing unit produces a list of unique words in a document.\n"
"The mandatory option -D specifies the name of the IDF_DIC file generated\n"
"by the idf_dic processing unit whose expected structure is as follows:\n"
"+----------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int  |\n"
"| (4 bytes): should be 1                                   |\n"
"+--------------------------------------+---+-------+-------+\n"
"| NULL-terminated sorted unique word 1 | Q | off_1 | IDF_1 |\n"
"+--------------------------------------+---+-------+-------+\n"
"|                             ...                          |\n"
"+--------------------------------------+---+-------+-------+\n"
"| NULL-terminated sorted unique word N | Q | off_N | IDF_N |\n"
"+--------------------------------------+---+-------+-------+\n"
"The binary data endianness is expected to be that of the host machine.\n"
"Q is an unsigned int (4 bytes) datum whose value should be 1.\n"
"off_i is an unsigned int (4 bytes) datum whose value must be 0.\n"
"IDF_i is a double (8 bytes) datum whose value should be the IDF of word i.\n"
"Then, this processing unit will calculate the weight vector w of each\n"
"document: w^d = <w^d_1, ..., w^d_N> where\n"
"                        TF'(i, d) * IDF(i)\n"
"w^d_i = ----------------------------------------------------\n"
"        sqrt( sum from j=1 to N of [ TF'(j, d) * IDF(j) ]^2 )\n"
"and TF'(i, d) = 1 + ln(TF(i, d))"
"Finally, the result will have the following binary header whose endianness\n"
"follows that of the host machine:\n"
"+--------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes)  |\n"
"+--------------------------------------------------------------------+\n"
"In continuation, for each file the following binary format of sparse vector\n"
"whose endianness follows that of the host machine is output:\n"
"+--------------------------+---+-------+-----+-----+-------+-----+\n"
"| NULL-terminated filename | Q | off_1 | w_1 | ... | off_Q | w_Q |\n"
"+--------------------------+---+-------+-----+-----+-------+-----+\n"
"Q is an unsigned int datum (4 bytes) expressing the number of offset-element\n"
"pairs.\n"
"off_i is an unsigned int datum (4 bytes) expressing the offset of an element"
"\n"
"within a vector.\n"
"w_j is a double datum (8 bytes) expressing the entry at position off_j.\n"
"A duplicated document name is regarded as one document assigned to more than\n"
"one category, and therefore, only the first document will be taken into\n"
"account because the duplicates are assumed to be the copy of the first\n "
"document and so will have the same vector representation w.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n",
"D:",
"-D IDF_DIC_FILE",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_idf_dic_file(optarg);
break;
)

  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }

  const unsigned int dic_size = idf_list.size();
  if (fwrite(&dic_size, sizeof(dic_size), 1, out_stream) == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }

  list<class_idf_entry> valid_w_list;

MAIN_INPUT_START
MAIN_LIST_OF_FILE_START
{
  const string doc_name(get_file_name(file_path->c_str()));

  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);

  double normalizer = 0;
  for (class_w_list::iterator i = w_list.begin(); i != w_list.end(); i++) {

    class_idf_list::iterator j = idf_list.find(i->first);

    if (j == idf_list.end()) { // Word is not in the dictionary
      continue;
    } else { // Calculation of a feature's weight
      unsigned int pos = j->second.first;
      double tf_idf = (1 + log(i->second)) * j->second.second;

      valid_w_list.push_back(class_idf_entry(pos, tf_idf));
      normalizer += tf_idf * tf_idf;
    }
  }
  normalizer = sqrt(normalizer);

  valid_w_list.sort();

  fprintf(out_stream, "%s%c", doc_name.c_str(), '\0');

  unsigned int offset_count = valid_w_list.size();
  if (fwrite(&offset_count, sizeof(offset_count), 1, out_stream) == 0) {
    fatal_syserror("Cannot write offset count to output stream");
  }

  struct sparse_vector_entry entry;  
  for (list<class_idf_entry>::iterator i = valid_w_list.begin();
       i != valid_w_list.end(); i++) {

    entry.offset = i->first;
    entry.value = i->second / normalizer;

    if (fwrite(&entry, sizeof(entry), 1, out_stream) == 0) {
      fatal_syserror("Cannot write weight to output stream");
    }
  }

  valid_w_list.clear();
  w_list.clear();
  word.clear();
}
MAIN_LIST_OF_FILE_END
MAIN_INPUT_END
MAIN_END
