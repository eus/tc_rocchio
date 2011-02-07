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

/* I design this to allow quick calculation of each category measure in
 * addition to quick indexing of category names give a document name. Below
 * is my design for quickly calculating each category measure:
 *
 * By definition given on page 83 of the book "Automatic Text Categorization:
 * From Information Retrieval To Support Vector Learning" by R. Basili and
 * A. Moschitti, let h(d) be the set of categories assigned to document d by
 * classifier h and GS(d) be the set of categories of document d.
 * Then, given a particular category C,
 * 1. num_correct of C (also called `a') is the number of documents in which
 *    C is in h(d) and C is in GS(d).
 * 2. num_wrong of C (also called `b') is the number of documents in which
 *    C is in h(d) but C is not in GS(d).
 * 3. num_not_retrieved of C (also called `c') is the number of documents in
 *    which C is not in h(d) but C is in GS(d).
 * Therefore, for each category C, I need to record a pair of boolean
 * information to be able calculate a, b and c quickly in the following way:
 *
 * First, scan the gold standard list of pairs of document and category names
 * like:
 * DOC_NAME_1 CAT_NAME_1
 * ...
 * DOC_NAME_M CAT_NAME_M
 * where DOC_NAME_i is not necessarily unique when a document is assigned into
 * multiple categories. CAT_NAME_i is usually not unique because usually one
 * category has many documents. While scanning the list, I aim to construct
 * the following logical table:
 * +-----+--------+---------+-----+-----+-----+
 * | d_i | h(d_i) | GS(d_i) | C_1 | ... | C_K |
 * +-----+--------+---------+-----+-----+-----+
 * | d_1 |        |         |     | ... |     |
 * +-----+--------+---------+-----+-----+-----+ 
 * |                     ...                  |
 * +-----+--------+---------+-----+-----+-----+
 * | d_M |        |         |     | ... |     |
 * +-----+--------+---------+-----+-----+-----+
 * For each line of the list, index d_i in the table. If d_i does not exists,
 * insert it. Then, insert a pair of boolean values (0, 1) into the column of
 * C_i. The first boolean value in the pair signifies that C_i is in h(d_i)
 * while the second boolean value signifies that C_i is in GS(d_i). Optionally,
 * C_i is appended into column GS(d_i) for human readability.
 *
 * Next, scan the list of pairs of document and category names obtained from
 * the classification process. While scanning the list, for each line, index
 * d_i in the table. If d_i cannot be found, it is an error because every
 * document must have a gold standard in supervised machine learning. Then,
 * index the pair of boolean values using C_i and set the first value in the
 * pair to 1 resulting in (1, ?). Optionally, C_i is appended into column h(d_i)
 * for human readability.
 *
 * Once the two lists have been processed, the measure of each category C_i can
 * be obtained by walking down the column of C_i incrementing:
 * - measure a_i of C_i if the pair is (1, 1),
 * - measure b_i of C_i if the pair is (1, 0), and
 * - measure c_i of C_i if the pair is (0, 1).
 *
 * From observing (0, 1) in gold standard list processing and (1, ?) in
 * classifier list processing, I can observe that the calculation can be done
 * more efficiently by initializing the measure c_i of each category C_i to the
 * number of documents whose gold standards include C_i. That is,
 * c_i := | {j | j is in {1...M}, GS(d_j) intersects {C_i} is not empty} |. This
 * can be done while processing the gold standard list of pairs of document and
 * category names.
 *
 * Then, while reading the list of pairs of document and category names obtained
 * from the classification process, for each line, if C_i is not in the gold
 * standard of d_i, increment measure b_i. Otherwise, decrement measure c_i and
 * increment measure a_i. Therefore, I can conclude that the measure a of
 * a category C is inverse proportional to the measure c of that category, and
 * that the sum of measure a and c is the total number of documents having C_i
 * in their gold standards.
 *
 * Although the measure b of a category C is unrelated to the measure a and c
 * as can be seen above, increasing the threshold of a classifier usually
 * increases the number of c (i.e., reducing recall in category C) while
 * decreasing the number of b (i.e., increasing accuracy in category C).
 */

#ifndef UTILITY_DOC_CAT_LIST_HPP
#define UTILITY_DOC_CAT_LIST_HPP

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include "utility.h"

using namespace std;

static inline double get_precision(unsigned int a, unsigned int b)
{
  /* Here the classifier is binary: in C or not in C */

  if (a == 0) {
    /* this cat has no doc (i.e., |C| = 0) or the classifier simply does
     * nothing (i.e., a = 0 but |C| >= 0)
     */

    if (b == 0) { // but the classifier makes no mistake
      return 1;
    } else { // but the classifier makes a mistake
      return 0;
    }
  }

  return static_cast<double>(a) / static_cast<double>(a + b);
}

static inline double get_recall(unsigned int a, unsigned int c)
{
  if ((a == 0) && (c == 0)) {
    return 1; // this cat has no doc, trivially all is recalled
  }

  return static_cast<double>(a) / static_cast<double>(a + c);  
}

static inline double get_f1(double precision, double recall)
{
  double denominator = precision + recall;
  if (denominator == 0) {
    return 0;
  } else {
    return 2 * precision * recall / denominator;
  }
}

static inline double get_interpolated_BEP(double precision, double recall)
{
  return (precision + recall) / 2.0;
}

class cat_stat
{
public:
  unsigned int a;
  unsigned int b;
  unsigned int c;
  double precision;
  double recall;
  double f1;
  double BEP;

  cat_stat(void)
  {
    clear();
  }

  ~cat_stat()
  {
    clear();
  }

  inline void compute(void)
  {
    precision = get_precision(a, b);
    recall = get_recall(a, c);
    f1 = get_f1(precision, recall);
    BEP = get_interpolated_BEP(precision, recall);
  }

  inline void clear(void)
  {
    a = 0;
    b = 0;
    c = 0;
    precision = 0;
    recall = 0;
    f1 = 0;
    BEP = 0;
  }
};
typedef unordered_map<string, cat_stat> class_set_of_cat_stats;

class global_cat_stat
{
public:
  unsigned int cat_count;
  unsigned int sum_a;
  unsigned int sum_b;
  unsigned int sum_c;
  double sum_precision;
  double sum_recall;

public:
  double avg_precision;
  double avg_recall;
  double u_avg_precision;
  double u_avg_recall;
  double u_avg_f1;
  double u_avg_BEP;

  global_cat_stat(void)
  {
    clear();
  }

  ~global_cat_stat()
  {
    clear();
  }

  inline void push(cat_stat &s)
  {
    cat_count++;
    sum_a += s.a;
    sum_b += s.b;
    sum_c += s.c;
    sum_precision += s.precision;
    sum_recall += s.recall;
  }

  inline void compute(void)
  {
    if (cat_count == 0) {
      return;
    }

    avg_precision = sum_precision / static_cast<double>(cat_count);
    avg_recall = sum_recall / static_cast<double>(cat_count);
    u_avg_precision = get_precision(sum_a, sum_b);
    u_avg_recall = get_recall(sum_a, sum_c);
    u_avg_f1 = get_f1(u_avg_precision, u_avg_recall);
    u_avg_BEP = get_interpolated_BEP(u_avg_precision, u_avg_recall);
  }

  inline void clear(void)
  {
    cat_count = 0;
    sum_a = 0;
    sum_b = 0;
    sum_c = 0;
    sum_precision = 0;
    sum_recall = 0;
    avg_precision = 0;
    avg_recall = 0;
    u_avg_precision = 0;
    u_avg_recall = 0;
    u_avg_f1 = 0;
    u_avg_BEP = 0;
  }
};

typedef unordered_set<string> class_set_of_cats;
typedef unordered_map<string, class_set_of_cats> class_doc_cat_list;

static void (*active_doc_cat_fn)(const string &doc_nm, const string &cat_nm);
static string doc_cat_line;

static inline void doc_cat_list_partial_fn(char *line)
{
  doc_cat_line.append(line);
}

static inline void doc_cat_list_complete_fn(void)
{
  int pos = doc_cat_line.find(' ');

  if (pos == -1) {
    fatal_error("`%s' is a malformed DOC_CAT file entry",
		doc_cat_line.c_str());
  }

  const string &doc_name = doc_cat_line.substr(0, pos);
  const string &cat_name = doc_cat_line.substr(pos + 1);

  active_doc_cat_fn(doc_name, cat_name);
  
  doc_cat_line.clear();
}

static inline void load_doc_cat_file(char *buffer, size_t buffer_size,
				     const char *filename,
				     void (*doc_cat_fn)(const string &doc_nm,
							const string &cat_nm))
{
  active_doc_cat_fn = doc_cat_fn;

  open_in_stream(filename);

  tokenizer("\n", buffer, buffer_size,
	    doc_cat_list_partial_fn, doc_cat_list_complete_fn);
}

#endif /* UTILITY_DOC_CAT_LIST_HPP */
