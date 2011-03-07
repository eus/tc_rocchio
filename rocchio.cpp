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

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

typedef pair<class_sparse_vector /* w */,
	     class_set_of_cats* /* gold standard */> class_w_cats;
typedef list<class_w_cats> class_w_cats_list;
static class_w_cats_list all_unique_docs; /* Contains all w in the input */

typedef pair<class_sparse_vector /* sum of w in C */,
	     unsigned int /* |C| */> class_W_construction;
typedef pair<class_W_construction, class_classifier> class_cat_profile;
typedef pair<string /* cat name */,
	     class_cat_profile /* classifier of this cat */
	     > class_cat_profile_list_entry;
typedef vector<class_cat_profile_list_entry> class_cat_profile_list;

#ifdef BE_VERBOSE
typedef unordered_map<class_sparse_vector *, string> class_w_to_doc_name;
static class_w_to_doc_name w_to_doc_name;
typedef unordered_set<class_sparse_vector *> class_docs;
#else
typedef vector<class_sparse_vector *> class_docs;
#endif
typedef unordered_map<string, class_docs> class_cat_doc_list;

typedef pair<pair<class_cat_doc_list,
		  unsigned int /* all cats cardinality
				* (i.e, |C_1| + ... + |C_k|)
				*/>,
	     class_cat_profile_list> class_W_construction_material;

typedef class_w_cats * class_w_cats_ptr;
typedef vector<class_w_cats_ptr> class_unique_docs_for_estimating_Th;

typedef pair<class_unique_docs_for_estimating_Th,
	     class_W_construction_material> class_data;

static inline unsigned int &all_cats_cardinality(class_data &data)
{
  return data.second.first.second;
}
static inline class_cat_doc_list &cat_doc_list(class_data &data)
{
  return data.second.first.first;
}
static inline class_unique_docs_for_estimating_Th &unique_docs(class_data &data)
{
  return data.first;
}
static inline class_cat_profile_list &cat_profile_list(class_data &data)
{
  return data.second.second;
}

static class_doc_cat_list gold_standard;
static class_data LS; /* All w in the input are cached here for final Th
		       * estimation and all category partial profiles are
		       * stored here to calculate the final profile vectors once
		       * the corresponding P_avg has been decided.
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
   *    incrementing variable all_cats_cardinality of LS.
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

  class_doc_cat_list::iterator GS_entry = gold_standard.find(word);
  if (GS_entry == gold_standard.end()) { // Doc is in excluded categories

    D_ptr->second = NULL;
    all_cats_cardinality(LS)++;

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
    all_cats_cardinality(LS) += doc_GS.size();

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

/* For each cat profile in the cat profile list, find a set of docs that
 * belongs to the cat profile in cat doc list, sum the w vectors of those
 * documents and store the result in the cat profile for later construction of
 * W vector.
 */
static inline void prepare_Ws(class_cat_profile_list &cat_profile_list,
			      const class_cat_doc_list &cat_doc_list)
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
	cat_W_construction.first.clear(); // Has no doc, sum of w vectors is 0
	continue;
      }

      const class_docs &cat_docs = e->second;

      cat_W_construction.second += cat_docs.size(); // |C|
      for (class_docs::const_iterator j = cat_docs.begin(); // sum all w
	   j != cat_docs.end();
	   j++)
	{
	  add_sparse_vector(cat_W_construction.first, **j);
	}
    }
}

/* If P is set to -1, the value of P_avg stored in each cat profile will be
 * used to construct the corresponding W vector.
 */
static inline void construct_Ws(class_cat_profile_list &cat_profile_list,
				const unsigned int all_cats_cardinality,
				double P)
{
  /* In this implementation, each category in the category profile has its
   * sum of w vectors that have the category in their gold standards.
   * So, to obtain the W vector of a category C, the sum of w vectors not in C
   * is simply subtracted from the sum of w vectors in C according to Rocchio
   * formula. Of course, the summations and subtraction involve some weightings.
   *
   * The key to make this process fast is to keep the sparsity of the vector.
   * Therefore, if each category in addition to having its sum of w vectors
   * also carries the sum of w vectors not in the category, although each
   * category has all the information to obtain the W vector (i.e., there
   * is no need to obtain the sum of w vectors not in the category by inquiring
   * other categories), the sum of w vectors not in the category is not a
   * sparse vector anymore. Beside eating up memory space, the operation with
   * non-sparse vector might have more page faults. In fact, I have implemented
   * this scheme and the performance is around 20s while the current
   * implementation is only 1.5s with w vectors generated from Reuters-115.
   *
   * I also tried to do the subtraction process by walking through elements in
   * W instead of walking through categories not in the category having the W.
   * In this way, once an element in W is less than 0, there is no need to
   * continue walking through the remaining other categories. But, the
   * performance is about 1.7s. I think this is because I cannot delete the
   * element in W that is less than 0 during the walk. So, there might be some
   * overhead associated with registering the elements to be deleted later.
   * Additionally, page faults and cache misses might increase as well.
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
      unsigned int not_C_cardinality = all_cats_cardinality - C_cardinality;

      class_sparse_vector &W = cat_profile.second.second;
      const class_sparse_vector &sum_w_in_C = cat_profile.first.first;

      /* The first term. If C_cardinality == 0, sum_w_in_C should be all zeros
       * (i.e., the sparse vector is empty), and so, W should be empty too  */
      assign_weighted_sparse_vector(W, sum_w_in_C, 1.0 / C_cardinality);

      /* Adjusting P if P is < 0 */
      if (P == -1) {
	P = cat_profile.second.first.P_avg;
      }
      /* End of adjustment */

      /* The second term, the penalizing part. P is assumed to be >= 0.0 */
      if (fpclassify(P) != FP_ZERO && C_cardinality != 0
	  && not_C_cardinality != 0)
	{
	  double multiplier = P / static_cast<double>(not_C_cardinality);

	  for (class_cat_profile_list::const_iterator j
		 = cat_profile_list.begin();
	       j != cat_profile_list.end();
	       j++)
	    {
	      if (i == j) {
		continue;
	      }
	      
	      const class_sparse_vector &sum_w_not_in_C
		= j->second.first.first;

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

		  if (W_e->second < 0) // max {0, ...
		    {
		      W.erase(W_e); // since W is a sparse vector
		    }
		} /* End of walking through sum_w_not_in_C entries */

	    } /* End of walking through all the other categories */

	} /* End of the second term, the penalizing part */
    }
}

#ifdef BE_VERBOSE
class class_d_list_entry {
public:
  unsigned int first;
  unsigned int second;
  vector <string *> first_docs;
  vector <string *> second_docs;
  class_d_list_entry() {
    first = 0;
    second = 0;
  }
};
#else
typedef pair<unsigned int /*|C|*/, unsigned int /*|~C|*/> class_d_list_entry;
#endif

typedef map<double /* dot product */, class_d_list_entry> class_d_list;

#ifdef BE_VERBOSE
static inline void print_bit(const class_d_list::const_reverse_iterator &bit,
			     unsigned int b, unsigned int c)
{
  const class_d_list_entry &e = bit->second;

  fprintf(stderr, "@ %f |C|=%05u |~C|=%05u (b=%05u c=%05u)\n",
	  bit->first, e.first, e.second, b, c);

  if ((e.first != 0 || e.second != 0)
      && e.first_docs.empty() && e.second_docs.empty()) { // In unit testing
    return;
  }

  fprintf(stderr, "\t C = {");
  for (vector<string *>::const_iterator x = e.first_docs.begin();
       x != e.first_docs.end();
       x++)
    {
      fprintf(stderr, " %s", (*x)->c_str());
    }
  fprintf(stderr, " }\n");

  fprintf(stderr, "\t~C = {");
  for (vector<string *>::const_iterator x = e.second_docs.begin();
       x != e.second_docs.end();
       x++)
    {
      fprintf(stderr, " %s", (*x)->c_str());
    }
  fprintf(stderr, " }\n");
}
#endif

/* Auxiliary function of function estimate_Th.
 * cat_doc_count is the number of unique documents that belong to target cat.
 */
static inline double do_threshold_estimation(unsigned int cat_doc_count,
					     const class_d_list &bit_string,
					     double &threshold)
{
  if (bit_string.empty()) { // Nothing to be estimated
    threshold = 0;
    return 1; // precision and recall are trivially 1 when |C| = 0 and b = 0
  }

  if (cat_doc_count == 0) {
    /* Cat has no doc.
     * This corresponds to the set of cases {0, 00, 000, ...}
     * So, the action mentioned in the explanation in function estimate_Th in
     * the case where all bits are zero is applied.
     */
    threshold = 1.5 * bit_string.rbegin()->first;
    return 1; // precision and recall are trivially 1 when |C| = 0 and b = 0
  }

  unsigned int b = 0;
  unsigned int c = cat_doc_count;
#ifdef BE_VERBOSE
  fprintf(stderr, "b=%05u c=%05u\n", b, c);
#endif
  class_d_list::const_reverse_iterator prev_bit = bit_string.rend();
  for (class_d_list::const_reverse_iterator bit = bit_string.rbegin();
       bit != bit_string.rend();
       bit++)
    {
      unsigned int next_c = c - bit->second.first;
      unsigned int next_b = b + bit->second.second;

#ifdef BE_VERBOSE
      print_bit(bit, next_b, next_c);
#endif

      if (next_b > next_c) {
	double prev_diff = (get_precision(cat_doc_count - c, b)
			    - get_recall(cat_doc_count - c, c));
	double curr_diff = (get_recall(cat_doc_count - next_c, next_c)
			    - get_precision(cat_doc_count - next_c, next_b));
#ifdef BE_VERBOSE
	fprintf(stderr, "prev_diff %f, curr_diff %f\n", prev_diff, curr_diff);
#endif
	if (prev_diff > curr_diff) {
	  b = next_b;
	  c = next_c;
	  threshold = bit->first;
#ifdef BE_VERBOSE
	  fprintf(stderr, "b > c: threshold := bit (%f)\n", threshold);
#endif
	} else {
	  threshold = prev_bit->first;
#ifdef BE_VERBOSE
	  fprintf(stderr, "b > c: threshold := prev_bit (%f)\n", threshold);
#endif
	}
	break;
      }	else if (next_b == next_c) {
	b = next_b;
	c = next_c;
	threshold = bit->first;
#ifdef BE_VERBOSE
	fprintf(stderr, "b = c: threshold := bit (%f)\n", threshold);
#endif
	break;
      } else {
	c -= bit->second.first;
	b += bit->second.second;
	prev_bit = bit;
      }
    }

  /* Interpolated BEP */
  double precision = get_precision(cat_doc_count - c, b);
  double recall = get_recall(cat_doc_count - c, c);

#ifdef BE_VERBOSE
  fprintf(stderr,
	  "a = %u, b = %u, c = %u\n"
	  "precision = %f, recall = %f\n",
	  cat_doc_count - c, b, c, precision, recall);
#endif

  return 0.5 * (precision + recall);
}

#ifndef NDEBUG
/* The test cases are taken from the explanation given in estimate_Th */
static inline void test_do_threshold_estimation(void)
{
  class_d_list bit_string;
  double Th = 0;
  double deviation;

#ifdef BE_VERBOSE
  fprintf(stderr, "*test_do_threshold_estimation():\n");
#endif

  /* 1) 1101001 can be divided into 1101 and 001 where b = c = 1 and a = 3 */
  bit_string[40].first += 1;
  bit_string[27].first += 1;
  bit_string[25].second += 1;
  bit_string[17].first += 1;
  bit_string[11].second += 1;
  bit_string[10].second += 1;
  bit_string[8].first += 1;
  deviation = do_threshold_estimation(4, bit_string, Th) - 0.75;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 17;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;

  /* 2) 1 can be divided into 1 and nothing where b = c = 0 and a = 1 */
  bit_string[100].first += 1;
  deviation = do_threshold_estimation(1, bit_string, Th) - 1;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 100;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;
  
  /* 3) 0 can be divided into nothing and 0 where b = c = 0 and a = 0 */
  bit_string[1].second += 1;
  deviation = do_threshold_estimation(0, bit_string, Th) - 1;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 1.5;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;

  /* 4) nothing can be divided into nothing and nothing where b = c = 0 */
  deviation = do_threshold_estimation(0, bit_string, Th) - 1;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 0;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;

  /* 5) Duplicated values:
   * 1100101100
   * 10 1 10 01
   *  1 0 1  1
   *  0 0 0  1
   */
  bit_string[9.3].first += 1;
  bit_string[9.3].first += 1;
  bit_string[8.9].first += 1;
  bit_string[8.9].second += 1;
  bit_string[8.9].first += 1;
  bit_string[8.9].second += 1;
  bit_string[8.5].second += 1;
  bit_string[8.1].second += 1;
  bit_string[8.1].first += 1;
  bit_string[8.1].second += 1;
  bit_string[8.1].second += 1;
  bit_string[4.5].first += 1;
  bit_string[3.4].second += 1;
  bit_string[3.4].first += 1;
  bit_string[3.4].first += 1;
  bit_string[3.4].second += 1;
  bit_string[3.1].first += 1;
  bit_string[3.1].second += 1;
  bit_string[2.1].first += 1;
  bit_string[1.9].second += 1;
  bit_string[1.9].second += 1;
  bit_string[1.9].first += 1;
  bit_string[1.9].first += 1;
  bit_string[1.3].second += 1;
  bit_string[1.3].first += 1;
  deviation = (do_threshold_estimation(13, bit_string, Th)
	       - 0.5 * (6.0/12.0 + 6.0/13.0));
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 4.5;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;
}
#endif /* NDEBUG of test_do_threshold_estimation() */

/**
 * It is a programming error if cat_doc_list does not contain target_cat_name.
 *
 * @param unique_docs the docs used to estimate the threshold
 * @param cat_doc_list is used to check for category emptiness
 *
 * @return interpolated BEP associated with the estimated threshold
 */
static inline double estimate_Th(class_unique_docs_for_estimating_Th unique_docs,
				 const class_cat_doc_list &cat_doc_list,
				 const string &target_cat_name,
				 class_classifier &target_cat_classifier)
{
/*
The threshold estimation will use the BEP method so as to make the experiment results more comparable with those of the other researchers.

The goal of threshold estimation using BEP over a set S of labeled document weight vectors (each such a vector is called w) is to find the threshold (Th) of the profile vector W of a binary classifier h associated with category C such that the precision and the recall of h is equal over S. Specifically, a labeled document d associated with a w vector in S has either GS(d) = {C} if the document is labeled to be in C or GS(d) = {} if the document is labeled to be not in C. Then, for all w vectors in S, one performs a dot product between a w vector and the W vector. Next, based on the dot product values, the w vectors are grouped into two: group C and group ~C. Those that have dot product values greater than or equal to Th are in group C while those that are not are in group ~C. Note that the grouping depends on the value of Th. Once the grouping is done, any document d in group C will have h(d) = {C} while any document d in group ~C will have h(d) = {}. Having h(d) under a certain value of Th and GS(d) for any document in S, the precision and the recall can be calculated as explained in utility_doc_cat_list.hpp. The goal is then to find the value of Th that can make precision equal to recall over S. However, such a Th may not exist.

When such a Th does not exist, for example when several w vectors produce the same dot product value V with the W vector and setting Th to include V results in precision > recall while setting Th to not include V results in precision < recall, Th should be set in such a way so as to minimize the difference between precision and recall |precision - recall|. In the example, if including V results in |precision - recall| that is smaller than not including V, Th is set to include V. Otherwise, Th is set to not include V.

There are several ways to estimate the value of Th using BEP method. First, one can keep incrementing the threshold starting from 0 (i.e., from a perfect recall that means that recall = 1) up to the point where precision >= recall. This method, however, will take a long time. Let e be the value by which the threshold is incremented each time. Then, at worst it will take around (P_1 - 0) / e where P_1 is the greatest dot product value. So, a method like binary search can be employed.

A binary search between 0 and P_1 will at worst take log_2(P_1 - 0) * N where N is the cardinality of S. N is present because for each new threshold, one has to calculate the number of documents that are incorrectly classified to obtain precision and recall. Specifically, let a denotes the upper end point and b denotes the lower end point. Initially a = P_1 and b = 0. For each step, Th is set to (a + b) / 2 and precision and recall are evaluated. If precision = recall, the search halts. Otherwise before going to the next step, if precision > recall, a is set to Th while if precision < recall, b is set to Th. However, a binary search does not perform well when precision cannot be equal to recall.

Using the example on V above, sooner or later the binary search will have a = V and the distance between a and b is kept halved until the search halts due to the limited precision of double data type. In the absence of the limitation, the search will never halt since between any two real numbers that are different, there exists a set of real numbers. In other words, when precision cannot be equal to recall, a binary search will be in vain. Therefore, in order to avoid this problem and to have a better time complexity N, I implement the following search method.

My implementation will sort the dot product values uniquely. Without loss of generality, I will only consider the case of descending order sort. This will result in: P_1, ..., P_K where K the number of unique dot product values. Now let b be the number of documents d in which h(d) = {C} but GS(d) = {} and c be the number of documents d in which h(d) = {} but GS(d) = {C}. Initially b = 0 and c = |C| where |C| is the number of documents d having GS(d) = {C}. Then, my implementation walks from P_1 to P_N at each step of which b is incremented by the number of documents whose dot products result in P_1 but are incorrectly classified into C while c is decremented by the number of documents whose dot products result in P_1 and correctly classified into C. The walk stops at the dot product value P_i at which b >= c. If b = c, Th is set to P_i. Otherwise, the precisions and recalls at two points are evaluated. One point is when Th is set to P_i and the other is when Th is set to P_(i-1). Then, Th is set to the dot product value that results in the most minimal |precision - recall|.

The sole use of b and c in my method can be understood from the definitions of precision and recall that can be stated as:

  precision = recall
a / (a + b) = a / (a + c) ... definitions of precision and recall
1 / (a + b) = 1 / (a + c) ... dividing both sides by a
    (a + b) = (a + c)     ... multiplying both sides by (a + b) and (a + c)
          b = c           ... subtracting a from both sides

That is, precision = recall if and only if b = c. Therefore, when walking from P_1 to P_N, I only need to detect a flip in the value of b and c to decide at which step Th should lie. Based on the sole use of b and c, the problem of estimating Th using BEP can be formulated as the problem of counting binary bits.

First, sort the dot product values without omitting duplicated values. As before, without loss of generality I will only consider the case of descending sort. This will result in: B_1, ..., B_N where N is the cardinality of S and B_1 is the maximal dot-product value and consecutive pair-wise different elements are not necessarily unique (i.e., equivalent dot product values are simply put one after another like the twos in 1, 2, 2, 2, 3). Then, each value B_i is associated with a binary bit in which bit 1 means that the document d having the dot product value B_i has GS(d) = {C} while bit 0 means that GS(d) = {}. For example:
+----+----+----+----+
|P_1 |P_2 |P_3 |P_4 |
+----+----+----+----+
|1   |0   |1   |1   |
+----+----+----+----+
Then, the binary string can be divided into two parts such that the number of zeros in the first part, which signifies b, is equal to the number of ones in the second part, which signifies c. For example:
1) 1101001 can be divided into 1101 and 001 where b = c = 1
2) 1 can be divided into 1 and nothing where b = c = 0
3) 0 or 00 or 000 can be divided into nothing and 0 or 00 or 000 where b = c = 0
4) nothing can be divided into nothing and nothing where b = c = 0

The candidate Th then uses the dot product value of the right-most bit 1 on the left part. In Example 1, it is the dot product value Q associated with the right-most bit 1 of 1101. If Q is associated with only one document, then Th is set to Q. Otherwise, the precisions and recalls at Q and at the next larger dot product value are evaluated. Then, Th is set to the dot product value that has the most minimum |precision - recall|.

For the case where all bits are zero, Th should be a value that is larger than the largest dot product value. Regarding how larger it should be, assuming that most of the unseen bit 1s will be above bit 0s and the number of bit 1s is less than the number of bit 0s (|C| is usually less than |~C|), the right way to decide the threshold is based on the distribution of known bit 0s and use confidence interval. But, for now I will just set the threshold to 1.5 times of the largest dot product value. For example, in Example 3, the threshold is the dot product value V of bit 0 plus 0.5 * V.

If no bit exists like in Example 4, the threshold is 0.
*/

  class_cat_doc_list::const_iterator t = cat_doc_list.find(target_cat_name);

  if (t == cat_doc_list.end() || t->second.empty()) {
    /* Cat has no doc.
     * This corresponds to the set of cases {0, 00, 000, ...}
     * So, the action mentioned in the above explanation in the case where all
     * bits are zero is applied.
     */

    double max_dot_product = 0; // dot product w and W cannot be < 0
    for (class_unique_docs_for_estimating_Th::const_iterator i
	   = unique_docs.begin();
	 i != unique_docs.end();
	 i++)
      {
	double result
	  = dot_product_sparse_vector((*i)->first,
				      target_cat_classifier.second);
	if (result > max_dot_product) {
	  max_dot_product = result;
	}
      }
    
    target_cat_classifier.first.threshold = 1.5 * max_dot_product;
    return 1; // precision and recall are trivially 1 when |C| = 0 and b = 0
  }

  class_d_list d_list;

  /* Constructing the bits */
  unsigned int cat_doc_count = 0;
  for (class_unique_docs_for_estimating_Th::const_iterator i
	 = unique_docs.begin();
       i != unique_docs.end();
       i++)
    {
      double dot_prod
	= dot_product_sparse_vector((*i)->first, target_cat_classifier.second);
      class_d_list_entry &entry = d_list[dot_prod];

      class_set_of_cats *doc_GS = (*i)->second;
      int bit_belongs_to_target_cat = (doc_GS != NULL // doc is in excluded cat
				       && (doc_GS->find(target_cat_name)
					   != doc_GS->end()));

      if (bit_belongs_to_target_cat) {

	cat_doc_count++;
	entry.first++;

#ifdef BE_VERBOSE
	entry.first_docs.push_back(&w_to_doc_name[&(*i)->first]);
#endif
      } else {

	entry.second++;

#ifdef BE_VERBOSE
	entry.second_docs.push_back(&w_to_doc_name[&(*i)->first]);
#endif
      }
    }
  /* End of bits construction */

#ifdef BE_VERBOSE
  fprintf(stderr, "Threshold estimation on %s (c = %u = |C|)\n",
	  target_cat_name.c_str(), cat_doc_count);
#endif
  
  double interpolated_BEP
    = do_threshold_estimation(cat_doc_count, d_list,
			      target_cat_classifier.first.threshold);

#ifdef BE_VERBOSE
  fprintf(stderr, "Interpolated BEP = %f\n", interpolated_BEP);
#endif

  return interpolated_BEP;
}

#ifndef NDEBUG
/* The test cases are taken from the explanation given in estimate_Th */
static inline void test_estimate_Th(void)
{
  class_doc_cat_list gold_standard;
  class_w_cats_list all_docs;
  class_unique_docs_for_estimating_Th unique_docs;
  class_cat_doc_list cat_doc_list;
  string target_cat_name("X");
  class_classifier target_cat_classifier;
  double deviation;

  target_cat_classifier.second[0] = 1;

#define __make_doc(cat_name, doc_name, first_w_entry) do {	\
    class_set_of_cats &cats = gold_standard[doc_name];		\
    cats.insert(cat_name);					\
    all_docs.push_back(class_w_cats());				\
    class_w_cats &doc = all_docs.back();			\
    doc.second = &cats;						\
    doc.first[0] = first_w_entry;				\
    unique_docs.push_back(&doc);				\
  } while (0)

#define __reset_test_case() do {		\
    gold_standard.clear();			\
    cat_doc_list.clear();			\
    unique_docs.clear();			\
    all_docs.clear();				\
    target_cat_classifier.first.threshold = 0;	\
  } while (0)

#define __do_test(expected_interpolated_BEP, expected_Th) do {		\
    /* Check interpolated BEP */					\
    deviation = (estimate_Th(unique_docs,				\
			     cat_doc_list,				\
			     target_cat_name,				\
			     target_cat_classifier)			\
		 - (expected_interpolated_BEP));			\
    assert(((deviation < 0)						\
	    ? (-deviation)						\
	    : deviation) < FP_COMPARISON_DELTA);			\
    /* Check Th */							\
    deviation = target_cat_classifier.first.threshold - (expected_Th);	\
    assert(((deviation < 0)						\
	    ? (-deviation)						\
	    : deviation) < FP_COMPARISON_DELTA);			\
  } while (0)

#ifdef BE_VERBOSE

  fprintf(stderr, "*test_estimate_Th():\n");

#define make_doc(cat_name, doc_name, first_w_entry) do {	\
    __make_doc(cat_name, doc_name, first_w_entry);		\
    class_w_cats &doc = all_docs.back();			\
    cat_doc_list[cat_name].insert(&doc.first);			\
    w_to_doc_name[&doc.first] = doc_name;			\
  } while (0)
#define reset_test_case() do {			\
    __reset_test_case();			\
    w_to_doc_name.clear();			\
  } while (0)
#define do_test(test_name, expected_interpolated_BEP, expected_Th) do {	\
    fprintf(stderr, "** " test_name ":\n");				\
    __do_test(expected_interpolated_BEP, expected_Th);			\
  } while (0)

#else /* !BE_VERBOSE */

#define make_doc(cat_name, doc_name, first_w_entry) do {	\
    __make_doc(cat_name, doc_name, first_w_entry);		\
    class_w_cats &doc = all_docs.back();			\
    cat_doc_list[cat_name].push_back(&doc.first);		\
  } while (0)
#define reset_test_case() do {			\
    __reset_test_case();			\
  } while (0)
#define do_test(test_name, expected_interpolated_BEP, expected_Th) do {	\
    __do_test(expected_interpolated_BEP, expected_Th);			\
  } while (0)
#endif /* !BE_VERBOSE */

#define make_empty_cat(cat_name) do {		\
    all_docs.push_back(class_w_cats());		\
    class_w_cats &doc = all_docs.back();	\
    doc.second = NULL;				\
    cat_doc_list[cat_name];			\
  } while (0)

  /* 1) 1101001 can be divided into 1101 and 001 where b = c = 1 and a = 3 */
  make_doc("X", "d1", 7.3); // 1
  make_doc("X", "d2", 7.1); // 1
  make_doc("Y", "d3", 6.7); // 0
  make_doc("X", "d4", 5.4); // 1
  make_doc("Y", "d5", 5.1); // 0
  make_doc("Z", "d6", 4.9); // 0
  make_doc("X", "d7", 4.7); // 1
  do_test("Case 1", 0.75, 5.4);
  reset_test_case();

  /* 2) 1 can be divided into 1 and nothing where b = c = 0 and a = 1 */
  make_doc("X", "d1", 7.3); // 1
  do_test("Case 2", 1, 7.3);
  reset_test_case();

  /* 3) 0 can be divided into nothing and 0 where b = c = 0 and a = 0 */
  make_empty_cat("X"); // nothing
  make_doc("Y", "d1", 7.3);  // 0
  do_test("Case 3", 1, 1.5 * 7.3);
  reset_test_case();

  /* 4) nothing can be divided into nothing and nothing where b = c = 0 */
  make_empty_cat("X"); // nothing
  make_empty_cat("Y"); // nothing
  do_test("Case 4", 1, 0);
  reset_test_case();

  /* 5) Duplicated values:
   * 1100101100
   * 10 1 10 01
   *  1 0 1  1
   *  0 0 0  1
   */
  make_doc("X", "d1", 7.3); make_doc("X", "d2", 7.3);   // 11
  make_doc("X", "d3", 7.1); make_doc("Y", "d4", 7.1);   // 1010
  make_doc("X", "d5", 7.1); make_doc("Z", "d6", 7.1);
  make_doc("Y", "d7", 5.8);                             // 0
  make_doc("W", "d8", 5.4); make_doc("X", "d9", 5.4);   // 0100
  make_doc("Z", "d10", 5.4); make_doc("Z", "d11", 5.4);
  make_doc("X", "d12", 5.2);                            // 1
  make_doc("Z", "d13", 4.7); make_doc("X", "d14", 4.7); // 0110
  make_doc("X", "d15", 4.7); make_doc("Y", "d16", 4.7);
  make_doc("X", "d17", 4.2); make_doc("W", "d18", 4.2); // 10
  make_doc("X", "d19", 4.1);                            // 1
  make_doc("W", "d20", 3.1); make_doc("Y", "d21", 3.1); // 0011
  make_doc("X", "d22", 3.1); make_doc("X", "d23", 3.1);
  make_doc("Z", "d24", 3); make_doc("X", "d25", 3);     // 01
  do_test("Case 5", 0.5 * (6.0/12.0 + 6.0/13.0), 5.2);
  reset_test_case();

#undef reset_test_case
#undef make_doc
#undef make_empty_cat
#undef do_test
#ifdef BE_VERBOSE
  #undef __make_doc
  #undef __do_test
  #undef __reset_test_case
#endif
}
#endif /* NDEBUG of test_test_estimate_Th() */

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
"where S means sum, C is the set of documents having category C but not in ES,"
"\n"
"~C is the set of documents not having category C and not in ES, and P is the\n"
"tuning parameter.\n"
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
"Otherwise, stdout is used to output binary data.\n",
"D:B:I:M:E:P:S:",
"-D DOC_CAT_FILE -B INIT_VALUE_OF_P -I INCREMENT_OF_P -M MAX_OF_P\n"
" -E ESTIMATION_SETS_COUNT -P ES_PERCENTAGE_OF_DOC_IN_[0.000...100.000]\n"
" -S RANDOM_SEED",
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

construct_cat_profile_list(cat_profile_list(LS), cat_doc_list(LS));
prepare_Ws(cat_profile_list(LS), cat_doc_list(LS));

/* Parameter tuning */
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
    for (class_w_cats_list::iterator j = all_unique_docs.begin();
	 j != all_unique_docs.end();
	 j++)
      {
	bool in_ES = ((uniform_deviate(rand()) * PERCENTAGE_MULTIPLIER)
		      < ES_percentage);
	bool in_excluded_categories = (j->second == NULL);

	if (in_ES) {

	  unique_docs(ES).push_back(&(*j));

	  if (in_excluded_categories) {

#ifdef BE_VERBOSE
	    cat_doc_list(ES)[""].insert(&j->first);
#else
	    cat_doc_list(ES)[""].push_back(&j->first);
#endif
	  } else {

	    const class_set_of_cats &GS = *(j->second);
	    for (class_set_of_cats::const_iterator k = GS.begin();
		 k != GS.end();
		 k++)
	      {

#ifdef BE_VERBOSE
		cat_doc_list(ES)[*k].insert(&j->first);
#else
		cat_doc_list(ES)[*k].push_back(&j->first);
#endif
	      }
	  }
	} else {

	  /* unique_docs(LS_min_ES) is not updated because no estimation is
	   * carried out in LS_min_ES although cat_doc_list(LS_min_ES) needs to
	   * be constructed to build the profile vectors
	   */

	  if (in_excluded_categories) {

	    all_cats_cardinality(LS_min_ES)++;

#ifdef BE_VERBOSE
	    cat_doc_list(LS_min_ES)[""].insert(&j->first);
#else
	    cat_doc_list(LS_min_ES)[""].push_back(&j->first);
#endif
	  } else {

	    const class_set_of_cats &GS = *(j->second);

	    all_cats_cardinality(LS_min_ES) += GS.size();
	    for (class_set_of_cats::const_iterator k = GS.begin();
		 k != GS.end();
		 k++)
	      {
#ifdef BE_VERBOSE
		cat_doc_list(LS_min_ES)[*k].insert(&j->first);
#else
		cat_doc_list(LS_min_ES)[*k].push_back(&j->first);
#endif
	      }
	  }
	}
      }
    /* End of constructions */

    construct_cat_profile_list(cat_profile_list(LS_min_ES),
			       cat_doc_list(LS_min_ES));

    prepare_Ws(cat_profile_list(LS_min_ES), cat_doc_list(LS_min_ES));

    for (double P = tuning_init; P <= tuning_max; P += tuning_inc)
      {
	construct_Ws(cat_profile_list(LS_min_ES),
		     all_cats_cardinality(LS_min_ES),
		     P);

	for (class_cat_profile_list::iterator j
	       = cat_profile_list(LS_min_ES).begin();
	     j != cat_profile_list(LS_min_ES).end();
	     j++)
	  {
	    if (j->first.empty()) { // Don't consider excluded categories
	      continue;
	    }

	    const string &cat_name = j->first;
	    class_classifier &cat_classifier = j->second.second;
	    class_W_property &prop = cat_classifier.first;

	    prop.update_BEP_max(estimate_Th(unique_docs(ES), cat_doc_list(ES),
					    cat_name, cat_classifier),
				P);	   
	  }
      }

    for (class_cat_profile_list::iterator j
	   = cat_profile_list(LS_min_ES).begin();
	 j != cat_profile_list(LS_min_ES).end();
	 j++)
      {
	if (j->first.empty()) { // Don't consider excluded categories
	  continue;
	}

	class_W_property &prop = j->second.second.first;

	P_avg_list[j->first] += prop.P_max;
      }
  }
for (class_cat_profile_list::iterator i = cat_profile_list(LS).begin();
     i != cat_profile_list(LS).end();
     i++)
  {
    if (i->first.empty()) { // Don't consider excluded categories
      continue;
    }

    i->second.second.first.P_avg = (P_avg_list[i->first]
				    / static_cast<double>(ES_count));
  }
/* End of parameter tuning */

construct_Ws(cat_profile_list(LS), all_cats_cardinality(LS),
	     ((ES_count == 0) ? tuning_init : -1));

#ifdef BE_VERBOSE
fprintf(stderr, "*LS:\n");
#endif
for (class_cat_profile_list::iterator i = cat_profile_list(LS).begin();
     i != cat_profile_list(LS).end();
     i++)
  {
    if (i->first.empty()) { // Don't consider excluded categories
      continue;
    }

    class_classifier &cat_classifier = i->second.second;
    class_W_property &prop = cat_classifier.first;
    prop.BEP = estimate_Th(unique_docs(LS), cat_doc_list(LS),
			   i->first, i->second.second);
  }

output_classifiers(cat_profile_list(LS));

MAIN_END
