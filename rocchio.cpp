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
#include <limits>

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

static class_w_cats_list all_unique_docs; /* Contains all w in the input */
static class_w_cats_list Th_estimation_docs; /* Contains all w in
					      * W_VECTORS_SET_FOR_TH_ESTIMATION
					      * file
					      */

#ifdef DONT_FOLLOW_ROI
typedef pair<pair<class_sparse_vector /* sum of w in C */,
		  class_sparse_vector /* adjustment for multicats docs */>,
	     unsigned int /* |C| */> class_W_construction;
#else
typedef pair<class_sparse_vector /* sum of w in C */,
	     unsigned int /* |C| */> class_W_construction;
#endif
typedef pair<class_W_construction, class_classifier> class_cat_profile;
typedef pair<string /* cat name */,
	     class_cat_profile /* classifier of this cat */
	     > class_cat_profile_list_entry;
typedef vector<class_cat_profile_list_entry> class_cat_profile_list;

#ifdef DONT_FOLLOW_ROI
typedef unordered_map<class_sparse_vector *,
		      unsigned int /* |GS(d)| - 1 */> class_multicats_docs;
typedef pair<pair<pair<class_cat_doc_list,
		       class_multicats_docs>,
		  unsigned int /* unique doc count (i.e., M) */>,
	     class_cat_profile_list> class_W_construction_material;
#else
typedef pair<pair<class_cat_doc_list,
		  unsigned int /* unique doc count (i.e., M) */>,
	     class_cat_profile_list> class_W_construction_material;
#endif

typedef pair<class_unique_docs_for_estimating_Th,
	     class_W_construction_material> class_data;

static inline unsigned int &unique_doc_count(class_data &data)
{
  return data.second.first.second;
}
static inline class_cat_doc_list &cat_doc_list(class_data &data)
{
#ifdef DONT_FOLLOW_ROI
  return data.second.first.first.first;
#else
  return data.second.first.first;
#endif
}
#ifdef DONT_FOLLOW_ROI
static inline class_multicats_docs &multicats_docs_list(class_data &data)
{
  return data.second.first.first.second;
}
#endif
static inline class_unique_docs_for_estimating_Th &unique_docs(class_data &data)
{
  return data.first;
}
static inline class_cat_profile_list &cat_profile_list(class_data &data)
{
  return data.second.second;
}

static class_doc_cat_list gold_standard;
static class_data LS; /* All w in the input are cached here for constructing
		       * the classifiers
		       */
static class_data Th_estimation_set; /* All w in W_VECTORS_SET_FOR_TH_ESTIMATION
				      * are cached here for Th estimation
				      */
static inline void doc_cat_fn(const string &doc_name, const string &cat_name)
{
  if (doc_name.empty()) {
    fatal_error("DOC_CAT file must not contain an empty document name");
  }
  if (cat_name.empty()) { // Empty cat is used to indicate excluded cat
    fatal_error("DOC_CAT file must not contain an empty category name");
  }

  gold_standard[doc_name].insert(cat_name);

  class_cat_doc_list::iterator i = cat_doc_list(LS).find(cat_name);
  if (i == cat_doc_list(LS).end()) {
    cat_doc_list(LS)[cat_name];
  }
}

static unsigned int Th_vector_size;
static unsigned int vector_size;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
  if (vector_size != Th_vector_size) {
    fatal_error("Size of w vectors in input (%u) != in Th estimation set (%u)",
		vector_size, Th_vector_size);
  }
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
  unique_docs(LS).push_back(D_ptr);
  /* Step 2 completed */

  /* Step 3 */
#ifdef BE_VERBOSE
  w_to_doc_name[&D_ptr->first] = word;
#endif

  unique_doc_count(LS)++;

  class_doc_cat_list::iterator GS_entry = gold_standard.find(word);
  if (GS_entry == gold_standard.end()) { // Doc is in excluded categories

    D_ptr->second = NULL;

    /* Step 4 */
#ifdef BE_VERBOSE
    cat_doc_list(LS)[""].insert(&D_ptr->first);
#else
    cat_doc_list(LS)[""].push_back(&D_ptr->first);
#endif
    /* Step 4 completed */
  } else {

    class_set_of_cats &doc_GS = GS_entry->second;
    D_ptr->second = &doc_GS;

#ifdef DONT_FOLLOW_ROI
    unsigned int doc_GS_cardinality = doc_GS.size();
    if (doc_GS_cardinality > 1) {
      multicats_docs_list(LS)[&D_ptr->first] = doc_GS_cardinality - 1;
    }
#endif

    /* Step 4 */
    for (class_set_of_cats::iterator i = doc_GS.begin(); i != doc_GS.end(); i++)
      {
#ifdef BE_VERBOSE
	cat_doc_list(LS)[*i].insert(&D_ptr->first);
#else
	cat_doc_list(LS)[*i].push_back(&D_ptr->first);
#endif
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

static inline void Th_vector_size_fn(unsigned int size)
{
  Th_vector_size = size;
}

static inline void Th_string_partial_fn(char *str)
{
  word.append(str);
}

static inline void Th_string_complete_fn(void)
{
  /* 1. Push an entry D into Th_estimation_docs.
   * 2. Set the pointer D_ptr to D to fill in the vector w and cache it in
   *    Th_estimation_set.
   * 3. Set the gold standard of D based on the DOC_CAT file while
   *    incrementing variable unique_doc_count of Th_estimation_set.
   */

  /* Step 1 */
  Th_estimation_docs.push_back(class_w_cats());
  /* Step 1 completed */

  /* Step 2 */
  D_ptr = &Th_estimation_docs.back();
  unique_docs(Th_estimation_set).push_back(D_ptr);
  /* Step 2 completed */

  /* Step 3 */
#ifdef BE_VERBOSE
  w_to_doc_name[&D_ptr->first] = word;
#endif

  unique_doc_count(Th_estimation_set)++;

  class_doc_cat_list::iterator GS_entry = gold_standard.find(word);
  if (GS_entry == gold_standard.end()) { // Doc is in excluded categories

    D_ptr->second = NULL;

  } else {

    class_set_of_cats &doc_GS = GS_entry->second;
    D_ptr->second = &doc_GS;

  }
  /* Step 3 completed */

  word.clear();
}

static inline void Th_offset_count_fn(unsigned int count)
{
}

static inline void Th_double_fn(unsigned int index, double value)
{
  D_ptr->first[index] = value;
}

static inline void Th_end_of_vector_fn(void)
{
}

static inline void output_classifiers(const class_cat_profile_list
				      &cat_profile_list)
{
  size_t block_write = fwrite(&vector_size, sizeof(vector_size), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }

  unsigned int i_cnt = 0;
  for (class_cat_profile_list::const_iterator i = cat_profile_list.begin();
       i != cat_profile_list.end();
       i++)
    {
      if (i->first.empty() // Don't consider excluded categories
	  || (i->second.first.second == 0)) { /* No document in the input stream
					       * has this category
					       */
	continue;
      }

      block_write = fwrite(i->first.c_str(),
			   i->first.length() + 1, 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write category name #%u to output stream",
		       i_cnt + 1);
      }

      const class_sparse_vector &W = i->second.second.second;

      unsigned int Q = W.size() + 3;
      block_write = fwrite(&Q, sizeof(Q), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output offset count of vector #%u", i_cnt + 1);
      }

      struct sparse_vector_entry e;
      e.offset = vector_size;
      e.value = i->second.second.first.threshold;
      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output threshold of vector #%u", i_cnt + 1);
      }
      e.offset++;
      e.value = i->second.second.first.P_avg;
      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output P of vector #%u", i_cnt + 1);
      }
      e.offset++;
      e.value = i->second.second.first.BEP;
      block_write = fwrite(&e, sizeof(e), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output BEP of vector #%u", i_cnt + 1);
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
	    fatal_syserror("Cannot output offset #%u of vector #%u",
			   k_cnt + 1, i_cnt + 1);
	  }
      }

      i_cnt++;
    }
}

/* Given a cat doc list, construct a cat profile list containing all categories
 * in the given cat doc list.
 */
static inline void construct_cat_profile_list(class_cat_profile_list &list,
					      const class_cat_doc_list &cat_doc)
{
  for (class_cat_doc_list::const_iterator i = cat_doc.begin();
       i != cat_doc.end();
       i++)
    {
      list.push_back(class_cat_profile_list_entry(i->first,
						  class_cat_profile()));
    }
}

#ifdef DONT_FOLLOW_ROI
/* For each cat profile in the cat profile list, find a set of docs that
 * belongs to the cat profile in cat doc list, sum the w vectors of those
 * documents registering the adjustment necessary for documents categorized into
 * more than one category, and store the result in the cat profile for later
 * construction of W vector.
 */
static inline void prepare_Ws(class_cat_profile_list &cat_profile_list,
			      const class_cat_doc_list &cat_doc_list,
			      const class_multicats_docs &multicats_docs)
#else
/* For each cat profile in the cat profile list, find a set of docs that
 * belongs to the cat profile in cat doc list, sum the w vectors of those
 * documents and store the result in the cat profile for later construction of
 * W vector.
 */
static inline void prepare_Ws(class_cat_profile_list &cat_profile_list,
			      const class_cat_doc_list &cat_doc_list)
#endif
{
  for (class_cat_profile_list::iterator i = cat_profile_list.begin();
       i != cat_profile_list.end();
       i++)
    {
      const string &cat_name = i->first;
      class_W_construction &cat_W_construction = i->second.first;

      class_cat_doc_list::const_iterator e = cat_doc_list.find(cat_name);
      if (e == cat_doc_list.end() || e->second.empty()) {

	cat_W_construction.second = 0; // |C|
#ifdef DONT_FOLLOW_ROI
	cat_W_construction.first.first.clear(); // no doc, sum of w vectors is 0
	cat_W_construction.first.second.clear(); // no doc, no adjustment needed
#else
	cat_W_construction.first.clear(); // Has no doc, sum of w vectors is 0
#endif
	continue;
      }

      const class_docs &cat_docs = e->second;

      cat_W_construction.second = cat_docs.size(); // |C|
      for (class_docs::const_iterator j = cat_docs.begin(); // sum all w
	   j != cat_docs.end();
	   j++)
	{
#ifdef DONT_FOLLOW_ROI
	  add_sparse_vector(cat_W_construction.first.first, **j);

	  class_multicats_docs::const_iterator d = multicats_docs.find(*j);
	  if (d != multicats_docs.end()) {
	    add_weighted_sparse_vector(cat_W_construction.first.second, **j,
				       d->second);
	  }
#else
	  add_sparse_vector(cat_W_construction.first, **j);
#endif
	}
    }
}

/* If P is set to -1, the value of P_avg stored in each cat profile will be
 * used to construct the corresponding W vector.
 */
static inline void construct_Ws(class_cat_profile_list &cat_profile_list,
				const unsigned int unique_doc_count,
				double P)
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

  for (class_cat_profile_list::iterator i = cat_profile_list.begin();
       i != cat_profile_list.end();
       i++)
    {
      if (i->first.empty()) { // Don't consider excluded categories
	continue;
      }

      class_cat_profile &cat_profile = i->second;

      unsigned int &C_cardinality = cat_profile.first.second;
      unsigned int not_C_cardinality = unique_doc_count - C_cardinality;

      class_sparse_vector &W = cat_profile.second.second;

      /* The first term. If C_cardinality == 0, sum_w_in_C should be all zeros
       * (i.e., the sparse vector is empty), and so, W should be empty too  */
      if (C_cardinality == 0) {
	W.clear();
	continue; // W is already all 0. There is no point to do subtraction.
      }

      /* Adjusting P if P is < 0 */
      if (P == -1) {
	P = cat_profile.second.first.P_avg;
      }
      /* End of adjustment */

#ifdef DONT_FOLLOW_ROI
      const class_sparse_vector &sum_w_in_C = cat_profile.first.first.first;
#else
      const class_sparse_vector &sum_w_in_C = cat_profile.first.first;
#endif
      assign_weighted_sparse_vector(W, sum_w_in_C, 1.0 / C_cardinality);

      /* The second term, the penalizing part. P is assumed to be >= 0.0 */
      if (fpclassify(P) != FP_ZERO && not_C_cardinality != 0)
	{
	  double multiplier = P / static_cast<double>(not_C_cardinality);

#ifdef DONT_FOLLOW_ROI
	  const class_sparse_vector &adjustment
	    = cat_profile.first.first.second;
	  add_weighted_sparse_vector(W, adjustment, multiplier);
#endif

	  for (class_cat_profile_list::const_iterator j
		 = cat_profile_list.begin();
	       j != cat_profile_list.end();
	       j++)
	    {
	      if (i == j) {
		continue;
	      }
#ifdef DONT_FOLLOW_ROI
	      const class_sparse_vector &sum_w_not_in_C
		= j->second.first.first.first;
#else
	      const class_sparse_vector &sum_w_not_in_C
		= j->second.first.first;
#endif

	      for (class_sparse_vector::const_iterator k
		     = sum_w_not_in_C.begin();
		   k != sum_w_not_in_C.end();
		   k++)
		{
		  class_sparse_vector::iterator W_e = W.find(k->first);

		  if (W_e == W.end()) { // entry is already zero
		    continue;
		  }

		  W_e->second -= multiplier * k->second;

		  /* If W_e->second is zero, it must be removed since this is
		   * a sparse vector. And, due to floating-point error, the
		   * zero can be a bit higher than the true zero.
		   */
		  if (W_e->second < FP_COMPARISON_DELTA) // max {0, ...
		    {
		      W.erase(W_e); // since W is a sparse vector
		    }
		} /* End of walking through sum_w_not_in_C entries */

	    } /* End of walking through all the other categories */

	} /* End of the second term, the penalizing part */
    }
}

static double tuning_init = -1;
static const char *Th_estimation_file = NULL;
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
"Then, profiling is carried out using P specified by the mandatory option -B\n"
"while the thresholds are estimated on all w vectors.\n"
"This processing unit will calculate the profile vector W of the category C\n"
"using the weight vectors of documents as follows:\n"
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
"Otherwise, stdout is used to output binary data.\n",
"D:B:U:",
"-D DOC_CAT_FILE -B VALUE_OF_P -U W_VECTORS_SET_FOR_TH_ESTIMATION\n",
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

case 'U':
Th_estimation_file = optarg;
break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
  if (tuning_init < 0) {
    fatal_error("-B must be specified (-h for help)");
  }
  if (Th_estimation_file == NULL) {
    fatal_error("-U must be specified (-h for help)");
  } else {
    open_in_stream(Th_estimation_file);
    parse_vector(buffer, BUFFER_SIZE, Th_vector_size_fn, Th_string_partial_fn,
		 Th_string_complete_fn, Th_offset_count_fn, Th_double_fn,
		 Th_end_of_vector_fn);
  }
}

#ifndef NDEBUG
test_do_threshold_estimation();
test_estimate_Th();

fatal_error("This only runs the test cases of " __FILE__);
#endif /* NDEBUG */

MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, offset_count_fn, double_fn,
	       end_of_vector_fn);
}
MAIN_INPUT_END /* All w vectors are stored in LS */

construct_cat_profile_list(cat_profile_list(LS), cat_doc_list(LS));
#ifdef DONT_FOLLOW_ROI
prepare_Ws(cat_profile_list(LS), cat_doc_list(LS), multicats_docs_list(LS));
#else
prepare_Ws(cat_profile_list(LS), cat_doc_list(LS));
#endif

for (class_cat_profile_list::iterator i = cat_profile_list(LS).begin();
     i != cat_profile_list(LS).end();
     i++)
  {
    if (i->first.empty()) { // Don't consider excluded categories
      continue;
    }

    i->second.second.first.P_avg = tuning_init;
  }

construct_Ws(cat_profile_list(LS), unique_doc_count(LS), -1);

for (class_cat_profile_list::iterator i = cat_profile_list(LS).begin();
     i != cat_profile_list(LS).end();
     i++)
  {
    if (i->first.empty()) { // Don't consider excluded categories
      continue;
    }

    class_classifier &cat_classifier = i->second.second;
    class_W_property &prop = cat_classifier.first;
    prop.BEP = estimate_Th(unique_docs(Th_estimation_set), cat_doc_list(LS),
			   i->first, i->second.second);
  }

output_classifiers(cat_profile_list(LS));

MAIN_END
