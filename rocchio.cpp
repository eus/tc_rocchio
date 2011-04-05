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

#include <cassert>
#include <unordered_map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_set>

#include "utility.h"
#include "utility.hpp"
#include "utility_vector.hpp"
#include "utility_threshold_estimation.hpp"
#include "rocchio.hpp"

using namespace std;

typedef unordered_set<string> class_doc_names_in_C;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static inline void output_classifier(const string &cat_name,
				     unsigned int vector_size,
				     const class_sparse_vector &W,
				     double Th, double P, double BEP)
{
  size_t block_write = fwrite(&vector_size, sizeof(vector_size), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }

  block_write = fwrite(cat_name.c_str(),
		       cat_name.length() + 1, 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write category name to output stream");
  }

  unsigned int Q = W.size() + 3;
  block_write = fwrite(&Q, sizeof(Q), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot output offset count");
  }

  struct sparse_vector_entry e;
  e.offset = vector_size;
  e.value = Th;
  block_write = fwrite(&e, sizeof(e), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot output threshold");
  }
  e.offset++;
  e.value = P;
  block_write = fwrite(&e, sizeof(e), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot output P");
  }
  e.offset++;
  e.value = BEP;
  block_write = fwrite(&e, sizeof(e), 1, out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot output BEP");
  }

  unsigned int k_cnt = 0;
  for (class_sparse_vector::const_iterator k = W.begin();
       k != W.end();
       k++, k_cnt++)
    {
      e.offset = k->first;
      e.value = k->second;

      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output offset #%u", k_cnt + 1);
      }
    }
}

static inline void construct_Ws(class_sparse_vector &W,	double P,
				const class_unique_docs &LS_min_ES_in_C,
				const class_unique_docs &LS_min_ES_not_in_C)
{
  /* In this implementation, each category C in the category profile has its
   * sum of w vectors that have the category C in their gold standards.
   * So, to obtain the W vector of a category C, the sum of w vectors not in C
   * is simply subtracted from the sum of w vectors in C according to Rocchio
   * formula. Of course, the summations and subtraction involve some weightings.
   *
   * The key to make this process fast is to keep the sparsity of the vector.
   * Therefore, if each category C in addition to having its sum of w vectors
   * also carries the sum of w vectors not in the category C, although each
   * category C has all the information to obtain the W vector (i.e., there
   * is no need to obtain the sum of w vectors not in the category C by
   * inquiring other categories not in C), the sum of w vectors not in the
   * category C is not a sparse vector anymore. Beside eating up memory space,
   * the operation with non-sparse vector might have more page faults. In fact,
   * I have implemented this scheme and the performance is around 20s while the
   * current implementation is only 1.5s with w vectors generated from
   * Reuters-115.
   *
   * I also tried to do the subtraction process by walking through elements in
   * W instead of walking through categories other than the category having the
   * W. In this way, once an element in W is less than 0, there is no need to
   * continue walking through the remaining other categories. But, the
   * performance is about 1.7s. I think this is because I cannot delete the
   * element in W that is less than 0 during the walk. So, there might be some
   * overhead associated with registering the elements to be deleted later.
   * Additionally, page faults and cache misses might increase as well.
   *
   * Therefore, in this implementation, I walk through all other categories at
   * each step of which I subtract the sum of w vectors of the other category
   * from the W vector.
   */

  unsigned int C_cardinality = LS_min_ES_in_C.size();
  unsigned int not_C_cardinality = LS_min_ES_not_in_C.size();

  W.clear();

  /* The first term. If C_cardinality == 0, sum_w_in_C should be all zeros
   * (i.e., the sparse vector is empty), and so, W should be empty too  */
  if (C_cardinality == 0) {
    return; // W is already all 0. There is no point to do subtraction.
  }

  const_foreach(class_unique_docs, LS_min_ES_in_C, d) {
    add_sparse_vector(W, d->second);
  }

  assign_weighted_sparse_vector(W, W, 1.0 / C_cardinality);

  /* The second term, the penalizing part. P is assumed to be >= 0.0 */
  if (fpclassify(P) != FP_ZERO && not_C_cardinality != 0) {

    double multiplier = P / static_cast<double>(not_C_cardinality);

    const_foreach(class_unique_docs, LS_min_ES_not_in_C, d) {
      const_foreach(class_sparse_vector, d->second, vec_entry) {
	class_sparse_vector::iterator W_e = W.find(vec_entry->first);

	if (W_e == W.end()) { // entry is already zero
	  continue;
	}

	W_e->second -= multiplier * vec_entry->second;

	/* If W_e->second is zero, it must be removed since this is
	 * a sparse vector. And, due to floating-point error, the
	 * zero can be a bit higher than the true zero.
	 */
	if (W_e->second <= FP_COMPARISON_DELTA) { // max {0, ...
	  W.erase(W_e); // since W is a sparse vector
	}
      }
    }
  }
}

static class_doc_names_in_C doc_names_in_C;
static string word;
static inline void partial_fn(char *f)
{
  word.append(f);
}
static inline void complete_fn(void)
{
  doc_names_in_C.insert(word);
  word.clear();
}

static class_sparse_vector *w = NULL;

static unsigned int ES_vector_size = 0;
static class_unique_docs ES_in_C;
static class_unique_docs ES_not_in_C;
static inline void ES_vector_size_fn(unsigned int size)
{
  ES_vector_size = size;
}
static inline void ES_string_partial_fn(char *str)
{
  word.append(str);
}
static inline void ES_string_complete_fn(void)
{
  if (doc_names_in_C.find(word) == doc_names_in_C.end()) {
    w = &ES_not_in_C[word];
  } else {
    w = &ES_in_C[word];
  }
  word.clear();
}
static inline void ES_offset_count_fn(unsigned int count)
{
}
static inline void ES_double_fn(unsigned int index, double value)
{
  (*w)[index] = value;
}
static inline void ES_end_of_vector_fn(void)
{
}

static unsigned int LS_min_ES_vector_size = 0;
static class_unique_docs LS_min_ES_in_C;
static class_unique_docs LS_min_ES_not_in_C;
static inline void LS_min_ES_vector_size_fn(unsigned int size)
{
  LS_min_ES_vector_size = size;
  if (ES_vector_size != LS_min_ES_vector_size) {
    fatal_error("ES_vector_size (%u) != LS_min_ES_vector_size (%u)",
		ES_vector_size, LS_min_ES_vector_size);
  }
}
static inline void LS_min_ES_string_partial_fn(char *str)
{
  word.append(str);
}
static inline void LS_min_ES_string_complete_fn(void)
{
  if (doc_names_in_C.find(word) == doc_names_in_C.end()) {
    w = &LS_min_ES_not_in_C[word];
  } else {
    w = &LS_min_ES_in_C[word];
  }
  word.clear();
}
static inline void LS_min_ES_offset_count_fn(unsigned int count)
{
}
static inline void LS_min_ES_double_fn(unsigned int index, double value)
{
  (*w)[index] = value;
}
static inline void LS_min_ES_end_of_vector_fn(void)
{
}

static const char *cat_name = NULL;
static double tuning_init = -1;
static const char *ES_file = NULL;
MAIN_BEGIN(
"rocchio",
"If input file is not given, stdin is read for input. Otherwise, the input\n"
"file is read.\n"
"Then, the input stream is expected to have a list of document\n"
"names and their corresponding w vectors in the following binary format whose\n"
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
"Logically, the file should come from a w_to_vector processing unit.\n"
"The mandatory option -D specifies the name of DOC file containing the\n"
"name of the documents that belong to category C whose name is specified\n"
"using the mandatory option -C, in the following format:\n"
"DOC_NAME\\n\n"
"Then, profiling is carried out using P specified by the mandatory option -B\n"
"while the thresholds are estimated on w vectors contained in the file\n"
"specified using the mandatory option -U.\n"
"This processing unit will calculate the profile vector W of the category C\n"
"using the weight vectors of documents in the input stream as follows:\n"
"W = <W_1, ..., W_N> and\n"
"             1                               P\n"
"W_i = max{0,---(S over d in C of [w^d_i]) - ----(S over e in ~C of [w^e_i])}\n"
"            |C|                             |~C|\n"
"where S means sum, C is the set of documents having category C,\n"
"~C is the set of documents not having category C, and"
"P is the tuning parameter.\n"
"Then, to estimate the threshold (Th) of the profile vector W,\n"
"for each w vector in the file specified by the mandatory option -U, this\n"
"processing unit will calculate the dot product of the document weight\n"
"vector w with the profile vector W.\n"
"The values of the dot products are sorted in descending order as follows:\n"
"v_1 ... v_k where v_i is the value of some dot products (i.e., some dot\n"
"products might have the same value) and v_1 is the greatest value.\n"
"Then, setting b = 0 and c = |C| where b is the number of incorrect\n"
"classification to category C and c is the number of incorrect classification\n"
"to category ~C, this processing unit will walk from v_1 to v_k at each step\n"
"of which b is incremented by the number of documents that belongs to ~C and\n"
"whose weight vectors produce v_i, while c is decremented by the number of\n"
"documents that belongs to C and whose weight vectors produce v_i.\n"
"At the end of each step (i.e., after all weight vectors associated with this\n"
"step have been taken into account), the value of b is compared with that of c."
"\n"
"Once b is greater than or equal to c, Th is then set to v_i while the\n"
"interpolated BEP (Break-event Point) is found by letting a = |C| - c and\n"
"calculating BEP = 0.5 * (precision + recall)\n"
"                           a       a\n"
"                = 0.5 * (----- + -----)\n"
"                         a + b   a + c\n"
"This completes the construction of a binary classifier for category C.\n"
"Finally, the profile vector and the corresponding threshold is encoded in\n"
"the following binary format whose endianness follows that of the host\n"
"machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size (N) of the sparse vector in unsigned int (4 bytes)    |\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"| cat_1_name |Q1|N|Th_1|N+1|P|N+2|BEP|off_1_1|w_1_1|...|off_(Q1-1)|w_(Q1-1)|\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"where N is the normal vector size of the sparse vector and cat name is\n"
"a NULL-terminated string.\n"
"Note that the threshold Th_i of each classifier is encoded as\n"
"the first entry in the sparse vector with N as its offset.\n"
"While the value of P_i is used to estimate Th_i is encoded as the second\n"
"entry with N+1 as its offset.\n"
"Additionally, the value of BEP_i associated with Th_i is encoded as the\n"
"third entry with N+2 as its offset.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n",
"C:D:B:U:",
"-C CAT_NAME -D DOC_FILE -B VALUE_OF_P -U ES_FILE\n",
0,
case 'C':
cat_name = optarg;
break;

case 'D':
/* Allocating tokenizing buffer */
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }
/* End of allocation */
open_in_stream(optarg);
tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
break;

case 'B':
tuning_init = strtod(optarg, NULL);
if (!isfinite(tuning_init)) {
  fatal_error("INIT_VALUE_OF_P must be finite");
} else if (tuning_init < 0.0) {
  fatal_error("INIT_VALUE_OF_P must be >= 0");
}
break;

case 'U':
ES_file = optarg;
break;

) {
  if (cat_name == NULL) {
    fatal_error("-C must be specified (-h for help)");
  }
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
  if (tuning_init < 0) {
    fatal_error("-B must be specified (-h for help)");
  }
  if (ES_file == NULL) {
    fatal_error("-U must be specified (-h for help)");
  } else {
    open_in_stream(ES_file);
    parse_vector(buffer, BUFFER_SIZE, ES_vector_size_fn, ES_string_partial_fn,
		 ES_string_complete_fn, ES_offset_count_fn, ES_double_fn,
		 ES_end_of_vector_fn);
  }
}

#ifndef NDEBUG
test_do_threshold_estimation();
test_estimate_Th();

fatal_error("This only runs the test cases of " __FILE__);
#endif /* NDEBUG */

MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, LS_min_ES_vector_size_fn,
	       LS_min_ES_string_partial_fn, LS_min_ES_string_complete_fn,
	       LS_min_ES_offset_count_fn, LS_min_ES_double_fn,
	       LS_min_ES_end_of_vector_fn);
}
MAIN_INPUT_END

class_sparse_vector W;
construct_Ws(W, tuning_init, LS_min_ES_in_C, LS_min_ES_not_in_C);
double Th = numeric_limits<double>::quiet_NaN();
double BEP = estimate_Th(ES_in_C, ES_not_in_C, string(cat_name), W,
			 LS_min_ES_in_C.empty(), &Th);
output_classifier(cat_name, LS_min_ES_vector_size, W, Th, tuning_init, BEP);

MAIN_END
