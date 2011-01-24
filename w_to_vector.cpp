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
#include "utility_vector.h"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

typedef pair<unsigned int, double> class_idf_entry;
typedef unordered_map<string, class_idf_entry> class_idf_list;
static class_idf_list idf_list;

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
  int pos = word.find(' ');

  if (pos == -1) {
    fatal_error("%s is a malformed input", word.c_str());
  } else {
    w_list.push_back(class_w_entry());

    class_w_entry &e = w_list.back();
    e.first.append(word.substr(0, pos));
    e.second = strtod(word.substr(pos + 1).c_str(), NULL);
  }

  word.clear();
}

static inline void vector_size_fn(unsigned int size)
{
}

static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static inline void string_complete_fn(void)
{
}

static inline void offset_count_fn(unsigned int count)
{
}

static unsigned int vector_position = 0;
static inline void double_fn(unsigned int index, double value)
{
  if (index != 0) {
    return;
  }

  class_idf_entry &data = idf_list[word];
  data.first = vector_position++;
  data.second = value;

  word.clear(); 
}

static inline void load_idf_dic_file(const char *filename)
{
  in_stream = fopen(filename, "r");
  in_stream_name = filename;
  if (in_stream == NULL) {
    fatal_syserror("Cannot open IDF_DIC file %s", filename);
  }

  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, offset_count_fn, double_fn);
}

typedef unordered_set<string> class_processed_doc_list;
class_processed_doc_list processed_doc_list;
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
"                        TF(i, d) * IDF(i)\n"
"w^d_i = ----------------------------------------------------\n"
"        sqrt( sum from j=1 to N of [ TF(j, d) * IDF(j) ]^2 )\n"
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
"The NULL-terminated filename is of length 0 when the input is stdin.\n"
"Q is an unsigned int datum (4 bytes) expressing the number of offset-element\n"
"pairs.\n"
"off_i is an unsigned int datum (4 bytes) expressing the offset of an element"
"\n"
"within a vector.\n"
"w_j is a double datum (8 bytes) expressing the entry at position off_j.\n"
"A duplicated document name is regarded as one document assigned to more than\n"
"one category, and therefore, only the first document will be taken into\n"
"account because the duplicates are assumed to be the copy of the first\n "
"document and so will have the same vector representation w."
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
{
  class_processed_doc_list::iterator exists
    = processed_doc_list.find(in_stream_name);
  if (exists != processed_doc_list.end()) { // Skip already processed doc
    continue;
  }

  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);

  double normalizer = 0;
  for (class_w_list::iterator i = w_list.begin(); i != w_list.end(); i++) {

    class_idf_list::iterator j = idf_list.find(i->first);

    if (j == idf_list.end()) { // Word is not in the dictionary
      continue;
    } else { // Calculation of a feature's weight
      unsigned int pos = j->second.first;
      double tf_idf = i->second * j->second.second;

      valid_w_list.push_back(class_idf_entry(pos, tf_idf));
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

  processed_doc_list.insert(in_stream_name);
}
MAIN_INPUT_END
MAIN_END
