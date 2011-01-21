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
    w_list.push_back(class_w_entry(word.substr(0, pos),
				   strtod(word.substr(pos + 1).c_str(), NULL)));
  }

  word.clear();
}

static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static inline void string_complete_fn(void)
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

  parse_vector(buffer, BUFFER_SIZE, NULL, string_partial_fn, string_complete_fn,
	       double_fn);
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
"+-------------------------------------------------------------------+\n"
"| Vector count per record in unsigned int (4 bytes): should be >= 1 |\n"
"+--------------------------------------+----------------------------+\n"
"| NULL-terminated sorted unique word 1 | IDF in double (8 bytes)    |\n"
"+--------------------------------------+----------------------------+\n"
"|                                ...                                |\n"
"+--------------------------------------+----------------------------+\n"
"| NULL-terminated sorted unique word N | IDF in double (8 bytes)    |\n"
"+--------------------------------------+----------------------------+\n"
"The binary data endianness is expected to be that of the host machine.\n"
"Then, this processing unit will calculate the weight vector w of each\n"
"document: w^d = <w^d_1, ..., w^d_N> where\n"
"                        TF(i, d) * IDF(i)\n"
"w^d_i = ----------------------------------------------------\n"
"        sqrt( sum from j=1 to N of [ TF(j, d) * IDF(j) ]^2 )\n"
"Finally, the result will have the following binary header whose endianness\n"
"follows that of the host machine:\n"
"+-----------------------------+\n"
"| N in unsigned int (4 bytes) |\n"
"+-----------------------------+\n"
"where N is the number of words found in the IDF_DIC file (i.e., the size\n"
"of each weight vector w).\n"
"In continuation, for each file the following binary format whose endianness\n"
"follows that of the host machine is constructed:\n"
"+--------------------------+---------------+-----+---------------+\n"
"| NULL-terminated filename | w_1 in double | ... | w_N in double |\n"
"+--------------------------+---------------+-----+---------------+\n"
"The NULL-terminated filename is of length 0 when the input is stdin.\n"
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
    fatal_syserror("Cannot write N to output stream");
  }  
MAIN_INPUT_START
{
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);

  double normalizer = 0;
  list<class_idf_entry> valid_w_list;
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

  list<class_idf_entry>::iterator j = valid_w_list.begin();
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
