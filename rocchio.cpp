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

#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "utility.h"
#include "utility.hpp"
#include "utility_vector.h"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

typedef unordered_set<string> class_set_of_cats;
typedef unordered_map<string, class_set_of_cats> class_doc_cat_list;
static class_doc_cat_list doc_cat_list;

typedef unsigned int class_cat_cardinality;
typedef pair<class_cat_cardinality, class_sparse_vector> class_cat_entry;
typedef unordered_map<string, class_cat_entry> class_cat_list;
static class_cat_list cat_list;

static string word;
static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  int pos = word.find(' ');

  if (pos == -1) {
    fatal_error("`%s' is a malformed DOC_CAT file entry", word.c_str());
  }

  string cat = word.substr(pos + 1);
  doc_cat_list[word.substr(0, pos)].insert(cat);

  class_cat_entry &entry = cat_list[cat];
  entry.first += 1;
  
  word.clear();
}

static inline void load_doc_cat_file(char *filename)
{
  in_stream = fopen(filename, "r");
  in_stream_name = filename;
  if (in_stream == NULL) {
    fatal_syserror("Cannot open DOC_CAT file %s", filename);
  }

  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
}

typedef class_sparse_vector * class_valid_W_vector;
typedef vector <class_valid_W_vector> class_valid_W_vectors;
static class_valid_W_vectors valid_W_vectors;
static unsigned int vector_size;
static unsigned int category_count;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
  category_count = cat_list.size();
  valid_W_vectors.reserve(category_count);
}

static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static unsigned int all_cats_cardinality = 0;
static inline void string_complete_fn(void)
{
  class_doc_cat_list::iterator i = doc_cat_list.find(word);
  if (i == doc_cat_list.end()) {
    fatal_error("Document `%s' has no category", word.c_str());
  }

  class_set_of_cats &cats = i->second;
  all_cats_cardinality += cats.size();
  valid_W_vectors.clear();
  for (class_set_of_cats::iterator j = cats.begin(); j != cats.end(); j++) {
    valid_W_vectors.push_back(&cat_list[*j].second);
  }

  word.clear();
}

static inline void offset_count_fn(unsigned int count)
{
}

/* Summation process: W += w
 * A document having more than one category is taken into account at this point
 */
static inline void double_fn(unsigned int index, double value)
{
  for (class_valid_W_vectors::iterator i = valid_W_vectors.begin();
       i != valid_W_vectors.end(); i++) {

    class_sparse_vector &W = **i;
    W[index] += value;
  }
}

static double f_selection_rate = -1;
MAIN_BEGIN(
"rocchio",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of document names and\n"
"their corresponding w vectors in the following binary format whose\n"
"endianness follows that of the host machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes)        |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated doc_name_1 |Q_1| off_1_1 | w_1_1 | ... | off_Q_1 | w_Q_1 |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"|                                    ...                                   |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated doc_name_M |Q_M| off_1_M | w_1_M | ... | off_Q_M | w_Q_M |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"Q_i is an unsigned int datum (4 bytes) expressing the number of\n"
"offset-element pairs of sparse vector representation of document i.\n"
"off_i_j is an unsigned int datum (4 bytes) expressing the offset of\n"
"element i within sparse vector j.\n"
"w_i_j is a double datum (8 bytes) expressing the value at position i within\n"
"sparse vector j.\n"
"Logically, the file should come from a w_to_vector processing unit.\n"
"The mandatory option -D specifies the name of DOC_CAT file containing the\n"
"categories of M documents in the following format:\n"
"DOC_NAME CAT_NAME\\n\n"
"If a document name in the input cannot be found in the DOC_CAT file, it is\n"
"a fatal error. However, DOC_NAME can be an empty word resulting in:\n"
" CAT_NAME\\n\n"
"to which doc_name in the form of empty string is assigned.\n"
"Then, this processing unit will calculate the profile vector W of each\n"
"category using OVA (One-vs-All) approach: W^c = <W^c_1, ..., W^c_N> where\n"
"               1                               p\n"
"W^c_i = max{0,---(S over d in c of [w^d_i]) - ---(S over e in h of [w^e_i])}\n"
"              |c|                             |h|\n"
"where S means sum, |c| is the number of documents in category c,\n"
"and |h| is the number of documents outside category c. Since OVA is\n"
"employed, h will be the set of all documents from all other categories\n"
"beside category c.\n"
"The feature selection rate p must not be negative.\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes)        |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated cat_name_1 |Q_1| off_1_1 | w_1_1 | ... | off_Q_1 | w_Q_1 |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"|                                    ...                                   |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated cat_name_K |Q_K| off_1_K | w_1_K | ... | off_Q_K | w_Q_K |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"where K is the number of categories found in DOC_CAT file.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n",
"D:p:",
"-D DOC_CAT_FILE -p FEATURE_SELECTION_RATE",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_doc_cat_file(optarg);

break;

case 'p':
f_selection_rate = strtod(optarg, NULL);
if (!isfinite(f_selection_rate)) {
  fatal_error("FEATURE_SELECTION_RATE must be finite");
} else if (fpclassify(f_selection_rate) != FP_ZERO && f_selection_rate < 0.0) {
  fatal_error("FEATURE_SELECTION_RATE must be >= 0");
}
break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
  if (f_selection_rate < 0) {
    fatal_error("-p must be specified (-h for help)");
  }
}
MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, offset_count_fn, double_fn);
}
MAIN_INPUT_END
{
  /* Beyond this point a document having more than one category is not of
   * concern anymore because everything has been taken care of during
   * parse_vector() process. Instead, we are to concern ourselves with the
   * efficient calculation of parametrized Rocchio formula.
   */

  /* Output header */
  size_t block_write = fwrite(&vector_size, sizeof(vector_size), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }
  /* End of outputting header */

  /* Profiling */
  class_sparse_vector accumulator;
  class_cat_list::iterator i_itr = cat_list.begin();
  for (unsigned int i = 0; i < category_count; i++, i_itr++)
    {

      /* The first term */
      unsigned int C_cardinality = i_itr->second.first;
      if (C_cardinality != 0)
	{

	  class_sparse_vector &W = i_itr->second.second;
	  for (class_sparse_vector::iterator k = W.begin(); k != W.end(); k++)
	    {
	      // W is already a sparse vector. So, k->second cannot be 0.
	      accumulator[k->first] = (k->second
				       / static_cast<double>(C_cardinality));
	    }

	  /* The second term, the penalizing part. This is employing OVA
	   * (One-versus-All).
	   */
	  unsigned int not_C_cardinality = all_cats_cardinality - C_cardinality;
	  if (fpclassify(f_selection_rate) != FP_ZERO && not_C_cardinality != 0)
	    {

	      double multiplier = (f_selection_rate
				   / static_cast<double>(not_C_cardinality));

	      /* Merge the first and the second terms */
	      class_cat_list::iterator j_itr = cat_list.begin();
	      for(unsigned int j = 0; j < category_count; j++, j_itr++)
		{

		  if (i == j) {
		    continue;
		  }

		  class_sparse_vector &W = j_itr->second.second;
		  for (class_sparse_vector::iterator k = W.begin();
		       k != W.end(); k++)
		    {
		      class_sparse_vector_offset W_off = k->first;
		      class_sparse_vector::iterator acc_e
			= accumulator.find(W_off);
		      if (acc_e == accumulator.end()) { // entry is already zero
			continue;
		      }

		      acc_e->second -= multiplier * k->second;

		      if (acc_e->second < 0) // max {0, ...
			{
			  /* since accumulator is a sparse vector... */
			  accumulator.erase(W_off);
			}
		    }
		}
	      /* End of merging the first and the second terms */
	    }
	  /* End of the second term, the penalizing part */
	}
      /* End of the first term */

      /* Outputting result */
      block_write = fwrite(i_itr->first.c_str(),
			   i_itr->first.length() + 1, 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write category name to output stream");
      }

      unsigned int Q = accumulator.size();
      block_write = fwrite(&Q, sizeof(Q), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write category name to output stream");
      }

      struct sparse_vector_entry e;
      for (class_sparse_vector::iterator k = accumulator.begin();
	   k != accumulator.end(); k++) {

	e.offset = k->first;
	e.value = k->second;

	block_write = fwrite(&e, sizeof(e), 1, out_stream);
	if (block_write == 0) {
	  fatal_syserror("Cannot write offset #%u to output stream", e.offset);
	}
      }
      /* End of outputting result */

      accumulator.clear();
    }
}
MAIN_END
