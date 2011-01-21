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
#include <csignal>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
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

typedef unordered_map<string, string> class_doc_cat_list;
static class_doc_cat_list doc_cat_list;
typedef vector<double> class_profile_vector;
typedef pair<unsigned int, class_profile_vector> class_cat_entry;
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
  } else {
    string cat = word.substr(pos + 1);
    doc_cat_list[word.substr(0, pos)] = cat;

    class_cat_entry &entry = cat_list[cat];
    entry.first += 1;
    entry.second = class_profile_vector();
  }
  
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

static class_profile_vector accumulator;
/* Preparing a set of profile vectors, one for each category */
static inline void vector_size_fn(unsigned int size)
{
  try {
    accumulator.reserve(size);
  } catch (length_error &e) {
    fatal_error("No memory for profile vector W of %u dimension", size);
  }

  accumulator.resize(size, 0);

  for (class_cat_list::iterator i = cat_list.begin(); i != cat_list.end(); i++)
    {
      try {
  	i->second.second.reserve(size);
      } catch (length_error &e) {
  	fatal_error("No memory for profile vector W of %u dimension", size);
      }

      i->second.second.resize(size, 0);
    }
}

static inline void string_partial_fn(char *str)
{
  word.append(str);
}

/* Selecting the correct profile vector W */
static class_profile_vector *vector_W;
static unsigned int total_doc = 0;
static inline void string_complete_fn(void)
{
  class_doc_cat_list::iterator i = doc_cat_list.find(word);
  if (i == doc_cat_list.end()) {
    fatal_error("Document `%s' has no category", word.c_str());
  }

  total_doc++;
  vector_W = &cat_list[i->second].second;

  word.clear();
}

/* Summation process: W += w */
static inline void double_fn(unsigned int index, double value)
{
  class_profile_vector &W = *vector_W;

  W[index] += value;
}

static double f_selection_rate;
MAIN_BEGIN(
"rocchio",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of document names and\n"
"their corresponding w vectors in the following binary format whose\n"
"endianness follows that of the host machine; size of `double' is 8 bytes:\n"
"+----------------------------------------------------------------------+\n"
"| N in unsigned int (4 bytes)                                          |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated doc_name_1 | w_1_1 in double | ... | w_N_1 in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"|                                 ...                                  |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated doc_name_M | w_1_1 in double | ... | w_N_M in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"Logically, the file should come from a w_to_vector processing unit.\n"
"The mandatory option -D specifies the name of DOC_CAT file specifying the\n"
"categories of M documents given as input in the following format:\n"
"DOC_NAME CAT_NAME\\n\n"
"If a document name in the input cannot be found in the DOC_CAT file, it is\n"
"a fatal error. However, DOC_NAME can be an empty word resulting in:\n"
" CAT_NAME\\n\n"
"to which doc_name in the form of empty string is assigned.\n"
"Then, this processing unit will calculate the profile vector W of each\n"
"category: W^c = <W^c_1, ..., W^c_N> where\n"
"               1                               p\n"
"W^c_i = max{0,---(S over d in c of [w^d_i]) - ---(S over e in h of [w^e_i])}\n"
"              |c|                             |h|\n"
"where S means sum, |c| is the number of documents in category c,\n"
"and |h| is the number of documents outside category c.\n"
"The feature selection rate p must not be negative.\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+----------------------------------------------------------------------+\n"
"| N in unsigned int (4 bytes)                                          |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated cat_name_1 | W_1_1 in double | ... | W_N_1 in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"|                                 ...                                  |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated cat_name_K | W_1_1 in double | ... | W_N_K in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
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
}
MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, double_fn);
}
MAIN_INPUT_END
{
  const unsigned int vector_size = accumulator.size();
  const unsigned int category_count = cat_list.size();

  /* Output header */
  size_t block_write = fwrite(&vector_size, sizeof(vector_size), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write vector size to output stream");
  }
  /* End of outputting header */

  /* Profiling */
  class_cat_list::iterator i_itr = cat_list.begin();
  for (unsigned int i = 0; i < category_count; i++, i_itr++)
    {

      /* The first term */
      unsigned int C_size = i_itr->second.first;
      if (C_size != 0)
	{

	  class_profile_vector &W = i_itr->second.second;
	  for (unsigned int k = 0; k < vector_size; k++)
	    {
	      accumulator[k] = W[k] / static_cast<double>(C_size);
	    }

	  /* The second term, the penalizing part. This is employing OVA
	   * (One-versus-All).
	   */
	  unsigned int not_C_size = total_doc - C_size;
	  if (fpclassify(f_selection_rate) != FP_ZERO && not_C_size != 0)
	    {

	      double multiplier = (f_selection_rate
				   / static_cast<double>(not_C_size));

	      /* Merge the first and the second terms */
	      class_cat_list::iterator j_itr = cat_list.begin();
	      for(unsigned int j = 0; j < category_count; j++, j_itr++)
		{

		  if (i == j) {
		    continue;
		  }

		  class_profile_vector &W = j_itr->second.second;
		  for (unsigned int k = 0; k < vector_size; k++)
		    {
		      accumulator[k] -= multiplier * W[k];
		      if (accumulator[k] < 0) // max {0, ...
			{
			  accumulator[k] = 0;

			  goto output_result;
			}
		    }
		}
	      /* End of merging the first and the second terms */
	    }
	  /* End of the second term, the penalizing part */
	}
      /* End of the first term */

    output_result:
      /* Outputting result */
      block_write = fwrite(i_itr->first.c_str(),
			   i_itr->first.length() + 1, 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write category name to output stream");
      }

      for (unsigned int k = 0; k < vector_size; k++) {
	block_write = fwrite(&accumulator[k], sizeof(double), 1, out_stream);
	if (block_write == 0) {
	  fatal_syserror("Cannot write vector element #%u to output stream",
			 k + 1);
	}
      }
      /* End of outputting result */
    }
}
MAIN_END
