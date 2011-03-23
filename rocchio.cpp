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
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef BE_VERBOSE
#include <unordered_set>
#endif

#include "utility.h"
#include "utility.hpp"
#include "utility_vector.hpp"
#include "utility_doc_cat_list.hpp"
#include "utility_classifier.hpp"
#include "utility_threshold_estimation.hpp"
#include "rocchio.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

/* Data structures and accessors specific to this file */
typedef pair<class_docs /* unique docs in C */,
	     class_sparse_vector /* sum of w in C */> class_unique_docs_in_C;
typedef pair<class_unique_docs_in_C,
	     class_docs /* unique docs not in C */> class_W_material;
typedef pair<class_classifier, class_W_material> class_classifier_material;
typedef unordered_map<string /* cat name */,
		      class_classifier_material> class_classifier_list;
typedef pair<class_unique_docs_for_estimating_Th,
	     class_classifier_list> class_data;
static inline class_unique_docs_for_estimating_Th &unique_docs_all(class_data
								   &data)
{
  return data.first;
}
static inline unsigned int unique_doc_count(class_data &data)
{
  return unique_docs_all(data).size();
}
static inline class_classifier_list &classifiers(class_data &data)
{
  return data.second;
}
static inline class_docs &unique_docs_in_C(class_classifier_material &C)
{
  return C.second.first.first;
}
static inline class_sparse_vector &get_sum_w_in_C(class_classifier_material &C)
{
  return C.second.first.second;
}
static inline class_docs &unique_docs_not_in_C(class_classifier_material &C)
{
  return C.second.second;
}
static inline class_classifier &binary_classifier(class_classifier_material &C)
{
  return C.first;
}
static inline class_sparse_vector &W_vector(class_classifier_material &C)
{
  return binary_classifier(C).second;
}
static inline class_W_property &W_property(class_classifier_material &C)
{
  return binary_classifier(C).first;
}
/* End of data structures specific to this file */

/* Processing DOC_CAT file */
static class_doc_cat_list gold_standard;

static inline void doc_cat_fn(const string &doc_name, const string &cat_name)
{
  if (doc_name.empty()) {
    fatal_error("DOC_CAT file must not contain an empty document name");
  }
  if (cat_name.empty()) { // Empty cat is used to indicate excluded cat
    fatal_error("DOC_CAT file must not contain an empty category name");
  }

  gold_standard[doc_name].insert(cat_name);
}
/* End of processing DOC_CAT file */

/* Main data */
static class_w_cats_list all_unique_docs; /* Contains all w in the input
					   * (i.e., in the LS)
					   */

static class_data LS; /* All w in the input are cached here for final Th
		       * estimation and all category partial profiles are
		       * stored here to calculate the final profile vectors once
		       * the corresponding P_avg has been decided.
		       */
/* End of main data */

/* Processing w vectors of all unique documents in the training set LS */
static unsigned int vector_size;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
}

static string word;
static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static class_w_cats_ptr D_ptr = NULL;
static inline void string_complete_fn(void)
{
  /* 1. Push an entry D into all_unique_docs.
   * 2. Set the pointer D_ptr to D to fill in the vector w and cache it in LS.
   * 3. Set the gold standard of D based on the DOC_CAT file while
   *    incrementing variable unique_doc_count of LS.
   * 4. Copy D_ptr to the categories in the gold standard of D.
   */

  /* Step 1 */
  all_unique_docs.push_back(class_w_cats());
  /* Step 1 completed */

  /* Step 2 */
  D_ptr = &all_unique_docs.back();
  unique_docs_all(LS).push_back(D_ptr);
  /* Step 2 completed */

  /* Step 3 */
#ifdef BE_VERBOSE
  w_to_doc_name[&D_ptr->first] = word;
#endif

  class_doc_cat_list::iterator GS_entry = gold_standard.find(word);
  if (GS_entry == gold_standard.end()) { // Doc is in excluded categories

    D_ptr->second = NULL;

    // No need to build the classifier of excluded categories.

  } else {

    class_set_of_cats &doc_GS = GS_entry->second;
    D_ptr->second = &doc_GS;

    /* Step 4 */
    for (class_set_of_cats::iterator i = doc_GS.begin(); i != doc_GS.end(); i++)
      {
	class_docs_insert(&D_ptr->first, unique_docs_in_C(classifiers(LS)[*i]));
      }
    /* Step 4 completed */
  }
  /* Step 3 completed */

  word.clear();
}

static inline void offset_count_fn(unsigned int count)
{
}

static inline void double_fn(unsigned int index, double value)
{
  D_ptr->first[index] = value;
}

static inline void end_of_vector_fn(void)
{
}
/* End of processing w vectors of all unique documents in the training set LS */

static inline void output_classifiers(class_classifier_list &classifier_list)
{
  size_t block_write = fwrite(&vector_size, sizeof(vector_size), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }

  unsigned int C_cnt = 0;
  for (class_classifier_list::iterator C = classifier_list.begin();
       C != classifier_list.end();
       C++)
    {
      const string &cat_name = C->first;
      class_classifier_material &cat_material = C->second;

      block_write = fwrite(cat_name.c_str(),
			   cat_name.length() + 1, 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write category name #%u to output stream",
		       C_cnt + 1);
      }

      const class_sparse_vector &W = W_vector(cat_material);

      unsigned int Q = W.size() + 3;
      block_write = fwrite(&Q, sizeof(Q), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output offset count of vector #%u", C_cnt + 1);
      }

      struct sparse_vector_entry e;
      e.offset = vector_size;
      e.value = W_property(cat_material).threshold;
      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output threshold of vector #%u", C_cnt + 1);
      }
      e.offset++;
      e.value = W_property(cat_material).P_avg;
      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output P of vector #%u", C_cnt + 1);
      }
      e.offset++;
      e.value = W_property(cat_material).BEP;
      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output BEP of vector #%u", C_cnt + 1);
      }

      unsigned int W_f_cnt = 0;
      for (class_sparse_vector::const_iterator W_f = W.begin();
	   W_f != W.end();
	   W_f++, W_f_cnt++)
	{
	  e.offset = W_f->first;
	  e.value = W_f->second;

	  block_write = fwrite(&e, sizeof(e), 1, out_stream);
	  if (block_write == 0) {
	    fatal_syserror("Cannot output offset #%u of vector #%u",
			   W_f_cnt + 1, C_cnt + 1);
	  }
      }

      C_cnt++;
    }
}

/* If classifiers are to be constructed on the given class_data, you must
 * initialize class_unique_docs_for_estimating_Th and, for each category C, its
 * class_unique_docs_in_C. This function then takes care of initializing
 * class_unique_docs_not_in_C of each category C.
 */
static inline void construct_unique_docs_not_in_C(class_data &data)
{
  for (class_unique_docs_for_estimating_Th::const_iterator d
	 = unique_docs_all(data).begin();
       d != unique_docs_all(data).end();
       d++)
    {
      class_sparse_vector *w = &((*d)->first);
      class_set_of_cats *GS_of_d = (*d)->second;

      for (class_classifier_list::iterator C = classifiers(data).begin();
	   C != classifiers(data).end();
	   C++)
	{
	  const string &cat_name = C->first;
	  class_classifier_material &C_material = C->second;

	  if (GS_of_d == NULL /* d is in excluded categories */
	      || GS_of_d->find(cat_name) == GS_of_d->end()) {
	    
	    class_docs_insert(w, unique_docs_not_in_C(C_material));
	    
	  }
	}
    }
}

/* For each classifier C in the classifier list, initialize its
 * sum of w vectors for use in the W vector construction.
 */
static inline void sum_w_in_C(class_classifier_list &classifier_list)
{
  for (class_classifier_list::iterator C = classifier_list.begin();
       C != classifier_list.end();
       C++)
    {
      class_docs &C_unique_docs = unique_docs_in_C(C->second);
      class_sparse_vector &sum_w = get_sum_w_in_C(C->second);

      sum_w.clear();

      if (C_unique_docs.empty()) {
	continue;
      }

      for (class_docs::const_iterator w = C_unique_docs.begin();
	   w != C_unique_docs.end();
	   w++)
	{
	  add_sparse_vector(sum_w, **w);
	}
    }
}

static inline void subtract_w_from_W(const class_sparse_vector &w,
				     class_sparse_vector &W, double multiplier)
{
  /* For each given w, process only elements in w that are in W. */

  typedef vector<class_sparse_vector::const_iterator> class_erased_elements;
  class_erased_elements erased_elements;
  for (class_sparse_vector::iterator W_f = W.begin();
       W_f != W.end();
       W_f++)
    {
      class_sparse_vector::const_iterator w_f = w.find(W_f->first);

      if (w_f == w.end()) { // w_f is zero does not change W_f
	continue;
      }

      W_f->second -= multiplier * w_f->second;

      /* If W_f->second is zero, it must be removed since this is
       * a sparse vector. And, due to floating-point error, the
       * zero can be a bit higher than the true zero. However,
       * the removal cannot be carried out when the W vector itself
       * is being iterated. So, it is scheduled for erasure later.
       */
      if (W_f->second <= FP_COMPARISON_DELTA) // max {0, ...
	{
	  erased_elements.push_back(W_f);
	}
    } /* End of walking through elements in W */

  unsigned int erased_elements_count = erased_elements.size();
  for (unsigned int i = 0; i < erased_elements_count; i++) {
    W.erase(erased_elements[i]);
  }
}

/* If P is set to -1, the value of P_avg stored in each cat profile will be
 * used to construct the corresponding W vector.
 */
static inline void construct_Ws(class_classifier_list &classifier_list,
				const unsigned int unique_doc_count,
				double P)
{
  /* In this implementation, each category C in the classifier list has its
   * sum of w vectors, which have the category C in their gold standards.
   * So, to obtain the W vector of a category C, the sum of w vectors not in C
   * is simply subtracted from the sum of w vectors in C according to Rocchio
   * formula. Of course, the summations and subtraction involve some weightings.
   *
   * The key to make this process fast is to keep the sparsity of the vector.
   * Therefore, if each category C in addition to having its sum of w vectors
   * also carries the sum of w vectors not in the category C, although the W
   * vector can simply be obtained by multiplying the given P with the sum of
   * w vectors not in the category C, the sum of w vectors not in the category
   * C is not a sparse vector anymore. Beside eating up memory space,
   * the operation with non-sparse vector might have more page faults. In fact,
   * I have implemented this scheme and the performance is around 20s while the
   * current implementation is only 1.5s with w vectors generated from
   * Reuters-115.
   *
   * Doing the subtraction process by walking through elements in W instead of
   * walking through w vectors not in category C increases page faults and
   * cache misses although once an element in W is less than 0, there is no
   * need to continue walking through the remaining w vectors for that
   * particular element. However, the collection of w vectors not in category C
   * needs to be processed all over again for the next element in W.
   * Experimentally I tried this already and it took 1.7s using the same setup
   * as the one used to obtain the 1.5s above.
   *
   * Therefore, in this implementation, I walk through all w vectors not in
   * category C at each step of which I subtract the elements in the w vector
   * from the corresponding elements in W vector. Once a particular element in
   * W vector is less than or equal to 0, the number of elements in w vector
   * that need to be looked up in the next step decreases by one.
   */

  for (class_classifier_list::iterator C = classifier_list.begin();
       C != classifier_list.end();
       C++)
    {
      class_classifier_material &C_material = C->second;

      unsigned int C_cardinality = unique_docs_in_C(C_material).size();
      unsigned int not_C_cardinality = unique_docs_not_in_C(C_material).size();

      if (C_cardinality + not_C_cardinality != unique_doc_count) {
	fatal_error("Logic error in construct_Ws: (|C|=%u) + (|~C|=%u) != %u",
		    C_cardinality, not_C_cardinality, unique_doc_count);
      }

      class_sparse_vector &W = W_vector(C_material);
      const class_sparse_vector &sum_w_in_C = get_sum_w_in_C(C_material);

      /* The first term. If C_cardinality == 0, sum_w_in_C should be all zeros
       * (i.e., the sparse vector is empty), and so, W should be empty too  */
      if (C_cardinality == 0) {
	W.clear();
	continue; // W is already all 0. There is no point to do subtraction.
      } else {
	assign_weighted_sparse_vector(W, sum_w_in_C, 1.0 / C_cardinality);
      }

      /* Adjusting P if P is < 0 */
      if (P == -1) {
	P = W_property(C_material).P_avg;
      }
      /* End of adjustment */

      /* The second term, the penalizing part. P is assumed to be >= 0.0 */
      if (fpclassify(P) != FP_ZERO && not_C_cardinality != 0)
	{
	  double multiplier = P / static_cast<double>(not_C_cardinality);

	  for (class_docs::iterator d_not_in_C
		 = unique_docs_not_in_C(C_material).begin();
	       d_not_in_C != unique_docs_not_in_C(C_material).end();
	       d_not_in_C++)
	    {

	      subtract_w_from_W(**d_not_in_C, W, multiplier);

	    } /* End of walking through all w vectors not in C */

	} /* End of the second term, the penalizing part */
    }
}

typedef vector<double /* BEP */> class_BEP_history_entry;
typedef unordered_map<string /* cat name */,
		      class_BEP_history_entry>class_BEP_history;
typedef unordered_set<string> class_BEP_history_filter;
static inline void output_BEP_history(const class_BEP_history &history,
				      const class_BEP_history_filter &filter,
				      int inverted_filter,
				      double p_init, double p_inc, double p_max)
{
  char markers[] = {'+', '*', 'o', 'x', '^'};
  unsigned int marker_idx = 0;
  char colors[] = {'k', 'r', 'g', 'b', 'm', 'c'};
  unsigned int color_idx = 0;
  unsigned int vec_size = 0;

  for (class_BEP_history::const_iterator i = history.begin();
       i != history.end();
       i++)
    {
      if (i == history.begin()) { // Only once
	fprintf(out_stream, "P = [%f:%f:%f];\n", p_init, p_inc, p_max);
	vec_size = i->second.size();

	/* Activate hold on */
	fprintf(out_stream,
		"plot(P(1), 0)\n"
		"hold on\n");
	/* Hold on activated */
      }

      if ((!filter.empty()
	   && (((filter.find(i->first) == filter.end()) && !inverted_filter)
	       || ((filter.find(i->first) != filter.end()) && inverted_filter)))
	  || (filter.empty() && inverted_filter))
	{
	  continue;
	}

      /* Construct the BEP vector */
      fprintf(out_stream, "BEP = [");
      for (unsigned int j = 0; j < vec_size; j++) {
	fprintf(out_stream, "%f ", i->second[j]);
      }
      fprintf(out_stream, "]; ");
      /* End of BEP vector construction */

      /* Plotting */
      fprintf(out_stream,
	      "plot(P, BEP, '-%c%c;%s;', 'markerfacecolor', 'none'"
	      ", 'markersize', 5)\n", markers[marker_idx++],
	      colors[color_idx++], i->first.c_str());

      marker_idx %= sizeof(markers) / sizeof(markers[0]);
      color_idx %= sizeof(colors) / sizeof(colors[0]);
      /* End of plotting */
    }

  fprintf(out_stream,
	  "hold off\n"
	  "xlabel('P')\n"
	  "ylabel('BEP')\n");
}

static unsigned int ES_percentage_set = 0;
static unsigned int ES_percentage;
static const unsigned int PERCENTAGE_MULTIPLIER = 1000000;
static unsigned int ES_count_set = 0;
static unsigned int ES_count;
static unsigned int ES_rseed_set = 0;
static unsigned int ES_rseed;
static double tuning_init = -1;
static double tuning_inc = -1;
static double tuning_max = -1;
static char *BEP_history_file = NULL;
static class_BEP_history_filter BEP_history_filter;
static int BEP_history_filter_inverted = 0;

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
"The mandatory option -D specifies the name of DOC_CAT file containing the\n"
"categories of M documents in the following format:\n"
"DOC_NAME CAT_NAME\\n\n"
"It is a fatal error if either DOC_NAME or CAT_NAME or both are empty string.\n"
"To not generate the classifier of a category, omit the documents of the\n"
"category from DOC_CAT file.\n"
"If a document name in the input cannot be found in the DOC_CAT file, the\n"
"document is considered to belong to an excluded category (the document is\n"
"still taken into account to generate the classifiers of included categories)."
"\n"
"If -E is 0, then parameter tuning is not carried out and the\n"
"profiling is carried out using the value of -B as the value of P while the\n"
"thresholds are estimated on all w vectors. Otherwise, a number of estimation\n"
"sets (ES) are built. Each ES contains a percentage of randomly selected\n"
"documents from the input list.\n"
"The percentage is specified using the mandatory option -P.\n"
"The random seed must be specified using the mandatory option -S.\n"
"Next, for each ES, starting from P as specified by the mandatory option -B,\n"
"this processing unit will calculate the profile vector W of the category C\n"
"using the weight vectors of documents not in ES as follows:\n"
"W = <W_1, ..., W_N> and\n"
"             1                               P\n"
"W_i = max{0,---(S over d in C of [w^d_i]) - ----(S over e in ~C of [w^e_i])}\n"
"            |C|                             |~C|\n"
"where S means sum, C is the set of unique documents having category C and\n"
"not in ES,\n"
"~C is the set of unique documents not having category C and not in ES, and\n"
"P is the tuning parameter.\n"
"Then, to estimate the threshold (Th) of the profile vector W,\n"
"for each document in ES, this processing unit will calculate the dot\n"
"product of the document weight vector w with the profile vector W.\n"
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
"The BEP is then associated with the current P while the Th is discarded.\n"
"Then, P is incremented by an amount as specified using the mandatory\n"
"option -I. If the new P is still less than or equal to the value specified\n"
"using the mandatory option -M, this processing unit repeat the W calculation\n"
"and threshold estimation processes all over again using the new P, obtaining\n"
"another interpolated BEP value. Once P exceeds the maximum value, the value\n"
"of P associated with the maximum BEP is associated with ES while the other\n"
"values of P and BEP are discarded.\n"
"After this process has been done with all ESes, the values of P each of\n"
"which is associated with an ES are averaged to obtain P_avg.\n"
"Finally, this processing unit will use P_avg to generate the final W vector\n"
"and estimate the Th of this final W vector on the whole documents given in\n"
"the input stream. The result is a binary classifier for category C.\n"
"Because the aforementioned process is done for all K categories, the final\n"
"result that is output by this processing unit is K binary classifiers in the\n"
"form of K profile vectors and the corresponding thresholds in the following\n"
"binary format whose endianness follows that of the host machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size (N) of the sparse vector in unsigned int (4 bytes)    |\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"| cat_1_name |Q1|N|Th_1|N+1|P|N+2|BEP|off_1_1|w_1_1|...|off_(Q1-1)|w_(Q1-1)|\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"|                                       ...                                |\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"| cat_K_name |QK|N|Th_K|N+1|P|N+2|BEP|off_1_K|w_1_K|...|off_(QK-1)|w_(QK-1)|\n"
"+------------+--+-+----+---+-+---+---+-------+-----+---+----------+--------+\n"
"where N is the normal vector size of the sparse vector and cat names are all\n"
"NULL-terminated strings.\n"
"Note that the threshold Th_i of each classifier is encoded as\n"
"the first entry in the sparse vector with N as its offset.\n"
"While the value of P_i is used to estimate Th_i is encoded as the second\n"
"entry with N+1 as its offset.\n"
"Additionally, the value of BEP_i associated with Th_i is encoded as the\n"
"third entry with N+2 as its offset.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n"
"If so desired, the prefix name of a GNU Octave script, which is MATLAB\n"
"compatible, can be specified using -H option.\n"
"The script will plot the BEPs of categories listed by -F, during\n"
"parameter tuning. To negate the list, prefix the list with a caret (^).\n"
"For example, to plot only categories acq and earn, use -F acq,earn.\n"
"To plot all categories but acq and earn, use -F ^acq,earn.\n"
"For each ES, there will be one script having the following name:\n"
"BEP_HISTORY_FILE.ES_NO.m.\n",
"D:B:I:M:E:P:S:H:F:",
"-D DOC_CAT_FILE -B INIT_VALUE_OF_P -I INCREMENT_OF_P -M MAX_OF_P\n"
" -E ESTIMATION_SETS_COUNT -P ES_PERCENTAGE_OF_DOC_IN_[0.000...100.000]\n"
" -S RANDOM_SEED [-H BEP_HISTORY_FILE [-F LIST_OF_CAT_NAMES_TO_PLOT]]\n",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_doc_cat_file(buffer, BUFFER_SIZE, optarg, doc_cat_fn);
break;

case 'B':
tuning_init = strtod(optarg, NULL);
if (!isfinite(tuning_init)) {
  fatal_error("INIT_VALUE_OF_P must be finite");
} else if (tuning_init < 0.0) {
  fatal_error("INIT_VALUE_OF_P must be >= 0");
}
break;

case 'I':
tuning_inc = strtod(optarg, NULL);
if (!isfinite(tuning_inc)) {
  fatal_error("INCREMENT_OF_P must be finite");
} else if (fpclassify(tuning_inc) == FP_ZERO || tuning_inc <= 0.0) {
  fatal_error("INCREMENT_OF_P must be > 0");
}
break;

case 'M':
tuning_max = strtod(optarg, NULL);
if (!isfinite(tuning_max)) {
  fatal_error("MAX_OF_P must be finite");
} else if (fpclassify(tuning_max) == FP_ZERO || tuning_max <= 0.0) {
  fatal_error("MAX_OF_P must be > 0");
}
break;

case 'E': {
  long int num = (long int) strtoul(optarg, NULL, 10);
  if (num < 0) {
    fatal_error("ESTIMATION_SETS_COUNT must be >= 0");
  }
  ES_count = num;
  ES_count_set = 1;
}
break;

case 'P': {
  double num = strtod(optarg, NULL) / 100 * PERCENTAGE_MULTIPLIER;
  if (num < 0) {
    fatal_error("ES_PERCENTAGE_OF_DOC must be >= 0");
  }
  ES_percentage = static_cast<unsigned int>(num);
  ES_percentage_set = 1;
}
break;

case 'S': {
  long int num = (long int) strtoul(optarg, NULL, 10);
  if (num < 0) {
    fatal_error("RANDOM_SEED must be >= 0");
  }
  ES_rseed = num;
  ES_rseed_set = 1;
}
break;

case 'H': {
  BEP_history_file = optarg;
}
break;

case 'F': {
  char *ptr = optarg;

  if (ptr[0] == '^') {
    BEP_history_filter_inverted = 1;
    ptr++;
  }

  ptr = strtok(ptr, ",");
  while (ptr != NULL) {
    BEP_history_filter.insert(string(ptr));
    ptr = strtok(NULL, ",");
  }
}
break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
  if (tuning_init < 0) {
    fatal_error("-B must be specified (-h for help)");
  }
  if (tuning_inc < 0) {
    fatal_error("-I must be specified (-h for help)");
  }
  if (tuning_max < 0) {
    fatal_error("-M must be specified (-h for help)");
  }
  if (!ES_count_set) {
    fatal_error("-E must be specified (-h for help)");
  }
  if (!ES_percentage_set) {
    fatal_error("-P must be specified (-h for help)");
  }
  if (!ES_rseed_set) {
    fatal_error("-S must be specified (-h for help)");
  }
}

#ifndef NDEBUG
test_do_threshold_estimation();
test_estimate_Th();

fatal_error("This only runs the test cases of " __FILE__);
#endif /* NDEBUG */

srand(ES_rseed);

MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, offset_count_fn, double_fn,
	       end_of_vector_fn);
}
MAIN_INPUT_END /* All w vectors are stored in LS */
construct_unique_docs_not_in_C(LS);
sum_w_in_C(classifiers(LS));

/* Parameter tuning */
const char *classifier_output_filename = out_stream_name;
class_BEP_history BEP_history;
typedef unordered_map<string /* cat name */, double> class_P_avg_list;
class_P_avg_list P_avg_list;
for (unsigned int i = 0; i < ES_count; i++)
  {
#ifdef BE_VERBOSE
    fprintf(stderr, "*ES #%u:\n", i);
#endif

    class_data ES; // will only store documents for estimation
    class_data LS_min_ES; /* store everything to build W vectors but the
			   * documents needed to perform Th estimation
			   */

    /* Construct ES and LS-ES*/
    for (class_w_cats_list::iterator d = all_unique_docs.begin();
	 d != all_unique_docs.end();
	 d++)
      {
	bool in_ES = ((uniform_deviate(rand()) * PERCENTAGE_MULTIPLIER)
		      < ES_percentage);
	bool not_in_excluded_categories = (d->second != NULL);

	if (in_ES) {

	  unique_docs_all(ES).push_back(&(*d));

	  if (not_in_excluded_categories) {

	    const class_set_of_cats &GS = *(d->second);
	    for (class_set_of_cats::const_iterator k = GS.begin();
		 k != GS.end();
		 k++)
	      {
		class_docs_insert(&d->first,
				  unique_docs_in_C(classifiers(ES)[*k]));
	      }
	  }
	} else {

	  /* unique_docs_all(LS_min_ES) must be initialized to construct the
	   * profile vectors although no estimation is carried out in LS_min_ES
	   */
	  unique_docs_all(LS_min_ES).push_back(&(*d));

	  if (not_in_excluded_categories) {

	    const class_set_of_cats &GS = *(d->second);

	    for (class_set_of_cats::const_iterator k = GS.begin();
		 k != GS.end();
		 k++)
	      {
		class_docs_insert(&d->first,
				  unique_docs_in_C(classifiers(LS_min_ES)[*k]));
	      }
	  }
	}
      }
    /* End of constructions */

    /* No need for the following for ES because no classifier is built on ES */
    construct_unique_docs_not_in_C(LS_min_ES);
    sum_w_in_C(classifiers(LS_min_ES));
    /* End of the preparation to build classifiers on LS_min_ES */

    for (double P = tuning_init; P <= tuning_max; P += tuning_inc)
      {
	construct_Ws(classifiers(LS_min_ES), unique_doc_count(LS_min_ES), P);

	for (class_classifier_list::iterator C = classifiers(LS_min_ES).begin();
	     C != classifiers(LS_min_ES).end();
	     C++)
	  {
	    const string &cat_name = C->first;
	    class_classifier_material &C_material = C->second;

	    double BEP = estimate_Th(unique_docs_all(ES),
				     unique_docs_in_C(classifiers(ES)[cat_name]),
				     cat_name,
				     binary_classifier(C_material));
	    W_property(C_material).update_BEP_max(BEP, P);

	    if (BEP_history_file != NULL) {
	      BEP_history[cat_name].push_back(BEP);
	    }
	  }
      }

    for (class_classifier_list::iterator C = classifiers(LS_min_ES).begin();
	 C != classifiers(LS_min_ES).end();
	 C++)
      {
	const string &cat_name = C->first;
	class_classifier_material &C_material = C->second;

	P_avg_list[cat_name] += W_property(C_material).P_max;
      }

    if (BEP_history_file != NULL) {
      char int2str[32];
      snprintf(int2str, sizeof(int2str), "%u", i);
      string filename(BEP_history_file);
      filename.append(".").append(int2str).append(".m");
      open_out_stream(filename.c_str());

      output_BEP_history(BEP_history, BEP_history_filter,
			 BEP_history_filter_inverted, tuning_init, tuning_inc,
			 tuning_max);
      fprintf(out_stream, "print('-landscape', '-dsvg', '%s.svg');\n",
	      filename.c_str());

      BEP_history.clear();
    }
  }
for (class_classifier_list::iterator C = classifiers(LS).begin();
     C != classifiers(LS).end();
     C++)
  {
    W_property(C->second).P_avg = (P_avg_list[C->first]
				   / static_cast<double>(ES_count));
  }
/* End of parameter tuning */

construct_Ws(classifiers(LS), unique_doc_count(LS),
	     ((ES_count == 0) ? tuning_init : -1));

#ifdef BE_VERBOSE
fprintf(stderr, "*LS:\n");
#endif
for (class_classifier_list::iterator C = classifiers(LS).begin();
     C != classifiers(LS).end();
     C++)
  {
    const string &cat_name = C->first;
    class_classifier_material &C_material = C->second;

    W_property(C_material).BEP = estimate_Th(unique_docs_all(LS),
					     unique_docs_in_C(C_material),
					     cat_name,
					     binary_classifier(C_material));
  }

open_out_stream(classifier_output_filename);
output_classifiers(classifiers(LS));

MAIN_END
