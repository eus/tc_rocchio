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

#define THREADED

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
#include <pthread.h>

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

typedef pair<class_sparse_vector /* sum of w in C */,
	     unsigned int /* |C| */> class_W_construction;
typedef pair<class_W_construction, class_classifier> class_cat_profile;
typedef pair<string /* cat name */,
	     class_cat_profile /* classifier of this cat */
	     > class_cat_profile_list_entry;
typedef vector<class_cat_profile_list_entry> class_cat_profile_list;

typedef pair<pair<class_cat_doc_list,
		  unsigned int /* unique doc count (i.e., M) */>,
	     class_cat_profile_list> class_W_construction_material;

typedef pair<class_unique_docs_for_estimating_Th,
	     class_W_construction_material> class_data;

static inline unsigned int &unique_doc_count(class_data &data)
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

      cat_W_construction.second = cat_docs.size(); // |C|
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
      const class_sparse_vector &sum_w_in_C = cat_profile.first.first;

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
	P = cat_profile.second.first.P_avg;
      }
      /* End of adjustment */

      /* The second term, the penalizing part. P is assumed to be >= 0.0 */
      if (fpclassify(P) != FP_ZERO && not_C_cardinality != 0)
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

typedef vector<double /* BEP */> class_BEP_history_entry;
typedef unordered_map<string /* cat name */,
		      class_BEP_history_entry>class_BEP_history;
typedef unordered_set<string> class_BEP_history_filter;
static inline void output_BEP_history(FILE *out_stream,
				      const class_BEP_history &history,
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

typedef unordered_map<string /* cat name */, double> class_best_P_per_cat;
static inline void tune_parameter(unsigned int ES_index,
				  unsigned int ES_percentage,
				  unsigned int PERCENTAGE_MULTIPLIER,
				  double tuning_init,
				  double tuning_max,
				  double tuning_inc,
				  const char *BEP_h_file,
				  const class_BEP_history_filter &BEP_h_filter,
				  int BEP_h_filter_inverted,
				  /* const */class_w_cats_list &all_unique_docs,
				  class_best_P_per_cat &best_P_per_cat)
{
#ifdef BE_VERBOSE
  verbose_msg("*ES #%u:\n", ES_index);
#endif

  class_BEP_history BEP_history;
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

	unique_doc_count(LS_min_ES)++;

	if (in_excluded_categories) {

#ifdef BE_VERBOSE
	  cat_doc_list(LS_min_ES)[""].insert(&j->first);
#else
	  cat_doc_list(LS_min_ES)[""].push_back(&j->first);
#endif
	} else {

	  const class_set_of_cats &GS = *(j->second);

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
		   unique_doc_count(LS_min_ES),
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

	  double BEP = estimate_Th(unique_docs(ES), cat_doc_list(ES),
				   cat_name, cat_classifier);
	  prop.update_BEP_max(BEP, P);

	  if (BEP_h_file != NULL) {
	    BEP_history[cat_name].push_back(BEP);
	  }
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

      best_P_per_cat[j->first] += prop.P_max;
    }

  if (BEP_h_file != NULL) {
    char int2str[32];
    snprintf(int2str, sizeof(int2str), "%u", ES_index);
    string filename(BEP_h_file);
    filename.append(".").append(int2str).append(".m");
    FILE *local_out_stream = open_local_out_stream(filename.c_str());

    output_BEP_history(local_out_stream, BEP_history, BEP_h_filter,
		       BEP_h_filter_inverted, tuning_init, tuning_inc,
		       tuning_max);
    fprintf(local_out_stream, "print('-landscape', '-dsvg', '%s.svg');\n",
	    filename.c_str());

    close_local_out_stream(local_out_stream, filename.c_str());
  }
}

static unsigned int ES_percentage_set = 0;
static unsigned int ES_percentage;
static const unsigned int PERCENTAGE_MULTIPLIER = 1000000;
static unsigned int ES_count_set = 0;
static unsigned int ES_count;
static unsigned int ES_rseed_set = 0;
static unsigned int ES_rseed;
static unsigned int tuner_threads_count = 0;
static double tuning_init = -1;
static double tuning_inc = -1;
static double tuning_max = -1;
static char *BEP_history_file = NULL;
static class_BEP_history_filter BEP_history_filter;
static int BEP_history_filter_inverted = 0;

class parameter_tuner_args {
public:
  unsigned int ES_index;
  unsigned int ES_percentage;
  unsigned int PERCENTAGE_MULTIPLIER;
  double tuning_init;
  double tuning_max;
  double tuning_inc;
  const char *BEP_history_file;
  const class_BEP_history_filter *BEP_history_filter;
  int BEP_history_filter_inverted;
  /* const */class_w_cats_list *all_unique_docs;
  class_best_P_per_cat best_P_per_cat;
  int tuner_exit_status;

  parameter_tuner_args(void)
  {
    this->ES_index = numeric_limits<unsigned int>::max();
    this->ES_percentage = ::ES_percentage;
    this->PERCENTAGE_MULTIPLIER = ::PERCENTAGE_MULTIPLIER;
    this->tuning_init = ::tuning_init;
    this->tuning_max = ::tuning_max;
    this->tuning_inc = ::tuning_inc;
    this->BEP_history_file = ::BEP_history_file;
    this->BEP_history_filter = &::BEP_history_filter;
    this->BEP_history_filter_inverted = ::BEP_history_filter_inverted;
    this->all_unique_docs = &::all_unique_docs;
    this->tuner_exit_status = EXIT_SUCCESS;
  }
  parameter_tuner_args(unsigned int ES_index)
  {
    parameter_tuner_args();
    this->ES_index = ES_index;
  }
};
static void *run_parameter_tuner(void *args)
{
  struct parameter_tuner_args *data
    = static_cast<struct parameter_tuner_args *>(args);

  if (data->ES_index == numeric_limits<unsigned int>::max()) {
    fatal_error("Programming error: ES_index is not set");
  }

  tune_parameter(data->ES_index, data->ES_percentage,
		 data->PERCENTAGE_MULTIPLIER,
		 data->tuning_init, data->tuning_max, data->tuning_inc,
		 data->BEP_history_file,
		 *data->BEP_history_filter, data->BEP_history_filter_inverted,
		 *data->all_unique_docs, data->best_P_per_cat);

  return &data->tuner_exit_status;
}

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
"Since the processing of each ES is independent from the other, multiple\n"
"threads can be used to speed up the parameter tuning process. The number of\n"
"ES processing threads is specified using the mandatory option -J that must\n"
"be at least 1.\n"
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
"D:B:I:M:E:P:S:H:F:J:",
"-D DOC_CAT_FILE -B INIT_VALUE_OF_P -I INCREMENT_OF_P -M MAX_OF_P\n"
" -E ESTIMATION_SETS_COUNT -P ES_PERCENTAGE_OF_DOC_IN_[0.000...100.000]\n"
" -S RANDOM_SEED -J PARAMETER_TUNING_THREAD_COUNT\n"
" [-H BEP_HISTORY_FILE [-F LIST_OF_CAT_NAMES_TO_PLOT]]\n",
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

case 'J': {
  long int num = (long int) strtoul(optarg, NULL, 10);
  if (num < 1) {
    fatal_error("PARAMETER_TUNING_THREAD_COUNT must be >= 1");
  }
  tuner_threads_count = num;
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
  if (tuner_threads_count == 0) {
    fatal_error("-J must be specified (-h for help)");
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
typedef vector<class parameter_tuner_args> class_args_per_tuner_list;
class_args_per_tuner_list args_per_tuner_list(ES_count);

typedef vector<pthread_t> class_tuner_list;
class_tuner_list tuner_list(tuner_threads_count);

unsigned int ES_index = 0;
while (ES_index < ES_count) {
  for (class_tuner_list::iterator tuner = tuner_list.begin();
       tuner != tuner_list.end();
       tuner++)
    {
      if (ES_index == ES_count) {
	break;
      }

      args_per_tuner_list[ES_index].ES_index = ES_index;
      if (pthread_create(&*tuner, NULL, run_parameter_tuner,
			 &args_per_tuner_list[ES_index]) != 0) {
	fatal_syserror("Cannot create tuner thread at ES_index = %u", ES_index);
      }

      ES_index++;
    }

  /* Sync point (A faster way would be to create a new thread when an
   * existing thread completes instead of waiting for all existing threads to
   * complete; but that is more complicated)
   */
  for (class_tuner_list::const_iterator tuner = tuner_list.begin();
       tuner != tuner_list.end();
       tuner++)
    {
      if (pthread_join(*tuner, NULL) != 0) {
	fatal_syserror("Cannot wait for thread %u",
		       static_cast<unsigned int>(*tuner));
      }
#ifdef BE_VERBOSE
      verbose_msg("Tuner of ES #%u finishes\n", tuner - tuner_list.begin());
#endif
    }
  /* End of synchronization */
 }

/* P_avg initialization */
class_best_P_per_cat P_avg_list;
for (class_args_per_tuner_list::const_iterator tuner_arg
       = args_per_tuner_list.begin();
     tuner_arg != args_per_tuner_list.end();
     tuner_arg++)
  {
    for (class_best_P_per_cat::const_iterator cat_best_P
	   = tuner_arg->best_P_per_cat.begin();
	 cat_best_P != tuner_arg->best_P_per_cat.end();
	 cat_best_P++)
      {
	P_avg_list[cat_best_P->first] += cat_best_P->second;
      }
  }
/* End of P_avg initialization */

for (class_cat_profile_list::iterator i = cat_profile_list(LS).begin();
     i != cat_profile_list(LS).end();
     i++)
  {
    if (i->first.empty()) { // Don't consider excluded categories
      continue;
    }

    i->second.second.first.P_avg = (P_avg_list[i->first]
				    / static_cast<double>(ES_count));

#ifdef BE_VERBOSE
    verbose_msg("Best Ps of %s:\n", i->first.c_str());
    for (unsigned int ES_index = 0; ES_index < ES_count; ES_index++) {
      verbose_msg("@ES #%u: %f\n", ES_index,
		  args_per_tuner_list[ES_index].best_P_per_cat[i->first]);
    }    
    verbose_msg("Sum of Ps = %f; P_avg = %f\n",
		P_avg_list[i->first], i->second.second.first.P_avg);
#endif
  }
/* End of parameter tuning */

construct_Ws(cat_profile_list(LS), unique_doc_count(LS),
	     ((ES_count == 0) ? tuning_init : -1));

#ifdef BE_VERBOSE
verbose_msg("*LS:\n");
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
