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

#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include "utility.h"
#include "utility.hpp"
#include "utility_vector.hpp"
#include "utility_classifier.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

typedef pair<string /* cat name */,
	     class_classifier> class_classifier_list_entry;
typedef vector<class_classifier_list_entry> class_classifier_list;
static class_classifier_list classifier_list;

static unsigned int vector_size;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
}

string word;
static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static class_classifier *active_classifier;
static inline void string_complete_fn(void)
{
  classifier_list.push_back(class_classifier_list_entry());

  class_classifier_list_entry &entry = classifier_list.back();
  entry.first.append(word);
  entry.second = class_classifier();
  active_classifier = &entry.second;

  word.clear();
}

static inline void offset_count_fn(unsigned int count)
{
}

static inline void double_fn(unsigned int index, double value)
{
  if (index == vector_size) {
    active_classifier->first.threshold = value;
  } else if (index < vector_size) {
    active_classifier->second[index] = value;
  }
}

static inline void end_of_vector_fn(void)
{
}

static inline void load_profile_file(char *filename)
{
  open_in_stream(filename);

  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, offset_count_fn, double_fn,
	       end_of_vector_fn);
}

static inline void vector_size_fn_dot(unsigned int size)
{
  if (size != vector_size) {
    fatal_error("w vector and W vector have different sizes");
  }
}

static inline void string_partial_fn_dot(char *str)
{
  word.append(str);
}

static inline void string_complete_fn_dot(void)
{
}

static inline void offset_count_fn_dot(unsigned int count)
{
}

static class_sparse_vector w;
static inline void double_fn_dot(unsigned int index, double value)
{
  w[index] = value;
}

static inline void end_of_vector_fn_dot(void)
{
  for (class_classifier_list::const_iterator i = classifier_list.begin();
       i != classifier_list.end();
       i++)
    {
      if ((dot_product_sparse_vector(i->second.second, w)
	   - i->second.first.threshold) >= -FP_COMPARISON_DELTA)
	{
	  fprintf(out_stream, "%s %s\n", word.c_str(), i->first.c_str());
	}
    }

  w.clear();
  word.clear();
}

static char *profile_file_name;
MAIN_BEGIN(
"classifier",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of document names and\n"
"their corresponding w vectors in the following binary format whose\n"
"endianness follows that of the host machine:\n"
"+-------------------------------------------------------------------------+\n"
"| Normal vector size (N) of the sparse vector in unsigned int (4 bytes)   |\n"
"+----------------------------+---+--------+-------+---+----------+--------+\n"
"| NULL-terminated doc_name_1 |Q1| off_1_1 | w_1_1 |...| off_Q1_1 | w_Q1_1 |\n"
"+----------------------------+---+--------+-------+---+----------+--------+\n"
"|                                    ...                                  |\n"
"+----------------------------+---+--------+-------+---+----------+--------+\n"
"| NULL-terminated doc_name_M |QM| off_1_M | w_1_M |...| off_QM_M | w_QM_M |\n"
"+----------------------------+---+--------+-------+---+----------+--------+\n"
"Qi is an unsigned int datum (4 bytes) expressing the number of\n"
"offset-element pairs of sparse vector representation of document i.\n"
"off_i_j is an unsigned int datum (4 bytes) expressing the offset of\n"
"element i within sparse vector j.\n"
"w_i_j is a double datum (8 bytes) expressing the value of element i within\n"
"sparse vector j.\n"
"Logically, the file should come from w_to_vector processing unit.\n"
"The mandatory option -D specifies the name of PROFILE file containing the\n"
"profile vectors of K categories in the following binary format whose\n"
"endianness is expected to be that of the host machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size (N) of the sparse vector in unsigned int (4 bytes)    |\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"| cat_1_name |Q1|N|Th_1|N+1|P|N+2|BEP|off_1_1|w_1_1|...|off_(Q1-1)|w_(Q1-1)|\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"|                                       ...                                |\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"| cat_K_name |QK|N|Th_K|N+1|P|N+2|BEP|off_1_K|w_1_K|...|off_(QK-1)|w_(QK-1)|\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"Logically, the file should come from rocchio processing unit.\n"
"Then, this processing unit will classify each document weight vector w using\n"
"all profile vectors. Then, following OVA (One-vs-All) approach, the document\n"
"represented by the weight vector w is classified into the categories\n"
"associated with profile vectors that accept the weight vector w.\n"
"Finally, the result will be in the following format:\n"
"DOC_NAME CAT_NAME\\n\n"
"If a document is classified into L categories, DOC_NAME will be duplicated L\n"
"times once for each category into which the document is classified.\n"
"The result is output to the given file if an output file is specified, or to\n"
"stdout otherwise.\n",
"D:",
"-D PROFILE_FILE",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }

profile_file_name = optarg;
/* End of allocation */
break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }

  load_profile_file(profile_file_name);
  if (classifier_list.size() == 0) {
    fatal_error("Cannot carry out dot product: PROFILE file is empty");
  }
}
MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn_dot, string_partial_fn_dot,
	       string_complete_fn_dot, offset_count_fn_dot, double_fn_dot,
	       end_of_vector_fn_dot);
}
MAIN_INPUT_END
MAIN_END
