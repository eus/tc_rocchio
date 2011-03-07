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

/* By definition given on page 83 of the book "Automatic Text Categorization:
 * From Information Retrieval To Support Vector Learning" by R. Basili and
 * A. Moschitti, let h(d) be the set of categories assigned to document d by
 * classifier h and GS(d) be the set of categories of document d.
 * Then, given a particular category C,
 * 1. num_correct of C (also called a) is the number of documents in which
 *    C is in h(d) and C is in GS(d).
 * 2. num_wrong of C (also called b) is the number of documents in which
 *    C is in h(d) and C is not in GS(d).
 * 3. num_not_retrieved of C (also called c) is the number of documents in which
 *    C is not in h(d) and C is in GS(d).
 *
 * This means that when a document d is categorized into multiple documents, the
 * classifier has to be capable of returning multiple categories as well.
 * If the classifier can only return one category, the following thing will
 * happen:
 * Let d be a particular document.
 * Let GS(d) = {C1, C2}.
 * Let h be a classifer capable of returning only one category and h(d) = {C1}.
 * Then, although the classifier is correct in classifying d into C1,
 * the recall of the classifier is penalized because the classifier cannot
 * classify the document into C2, or vice versa. In details:
 *    C1  C2
 *   +---+---+
 * a | 1 | 0 |
 * b | 0 | 0 |
 * c | 0 | 1 |
 *   +---+---+
 * where:
 *     precision recall
 *    +---------+------+
 * C1 |    1    |   1  |
 * C2 |    0    |   0  | -> we give 0 if the numerator is 0 even though the
 *    +---------+------+    denominator is also 0
 *
 * Now suppose another document d' belongs to C2. That is, h(d') = {C2} and
 * GS(d') = {C2}. Then,
 *    C1  C2
 *   +---+---+
 * a | 1 | 1 |
 * b | 0 | 0 |
 * c | 0 | 1 |
 *   +---+---+
 * where:
 *     precision recall
 *    +---------+------+
 * C1 |    1    | 1    |
 * C2 |    1    | 0.5  | -> we give 0 if the numerator is 0 even though the
 *    +---------+------+    denominator is also 0
 *
 * and the penalty in recall is clear when the classifier h can only return one
 * category.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unordered_set>
#include "utility.h"
#include "utility_doc_cat_list.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static unordered_set<string> excluded_categories;
static class_set_of_cat_stats cat_stats;
static class_doc_cat_list gold_standard;
static inline void doc_cat_fn_GS(const string &doc_name,
				 const string &cat_name)
{
  if (doc_name.empty()) {
    fatal_error("DOC_CAT file must not contain an empty document name");
  }
  if (cat_name.empty()) {
    fatal_error("DOC_CAT file must not contain an empty category name");
  }

  gold_standard[doc_name].insert(cat_name);
  cat_stats[cat_name].c++;
}

static inline void doc_cat_fn_assigned(const string &doc_name,
				       const string &cat_name)
{
  if (doc_name.empty()) {
    fatal_error("Input must not contain an empty document name");
  }
  if (cat_name.empty()) {
    fatal_error("Input must not contain an empty category name");
  }

  class_doc_cat_list::iterator entry = gold_standard.find(doc_name);
  if (entry == gold_standard.end()) {
    fatal_error("Document %s has no category", doc_name.c_str());
  }

  class_set_of_cats &cats = entry->second;
  class_set_of_cats::iterator cat = cats.find(cat_name);
  if (cat == cats.end()) { // assigned cat name is not in GS(doc_name)
    cat_stats[cat_name].b++;
  } else {
    cat_stats[cat_name].a++;
    cat_stats[cat_name].c--;
  }
}

MAIN_BEGIN(
"perf_measurer",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of document names and\n"
"their corresponding categories in the following structure:\n"
"DOC_NAME CAT_NAME\\n\n"
"Logically this should be the output of the classifier processing unit.\n"
"It is a fatal error if either DOC_NAME or CAT_NAME or both are empty string.\n"
"If a document name in the input cannot be found in the DOC_CAT file, it is\n"
"a fatal error.\n"
"The mandatory option -D specifies the name of DOC_CAT file containing the\n"
"categories of the documents in the following format:\n"
"DOC_NAME CAT_NAME\\n\n"
"It is a fatal error if either DOC_NAME or CAT_NAME or both are empty string.\n"
"To not take into account a category when measuring the global performance,\n"
"specify the category name using -X. The optional option -X can be specified\n"
"more than once to exclude several categories.\n"
"Then, this processing unit will calculate for each category:\n"
"#correct, #wrong, #not retrieved, recall, precision, F1, and BEP\n"
"Then, the unit will calculate the macro averages of recall and precision\n"
"as well as the micro averages of recall, precision and F1.\n"
"Finally, the result will be in the following format:\n"
"NAME_1\\tA_1\\tB_1\\tC_1\\tPRECISION_1\\tRECALL_1\\tF1_1\\tBEP_1\\n\n"
"...\n"
"NAME_K\\tA_K\\tB_K\\tC_K\\tPRECISION_K\\tRECALL_K\\tF1_K\\tBEP_K\\n\n"
"MACRO_AVG_PRECISION\\tMACRO_AVG_RECALL\\tuPRECISION\\tuRECALL\\tuF1\\tuBEP\\n"
"where K is the number of categories found in DOC_CAT file, A is the number\n"
"of documents classified correctly under C_i, B is the number of documents\n"
"classified incorrectly under C_i and C is the number of documents of C_i\n"
"not classified under C_i.\n"
"The result is output to the given file if an output file is specified, or\n"
"to stdout otherwise.\n",
"D:X:",
"-D DOC_CAT_FILE [-X EXCLUDED_CATEGORY]",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_doc_cat_file(buffer, BUFFER_SIZE, optarg, doc_cat_fn_GS);

break;

case 'X':
if (strlen(optarg) == 0) {
  fatal_error("Excluded category name cannot be empty string");
}
excluded_categories.insert(string(optarg));
break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
}
MAIN_INPUT_START
{
  load_doc_cat_file(buffer, BUFFER_SIZE, in_stream_name, doc_cat_fn_assigned);
}
MAIN_INPUT_END
{
  global_cat_stat global_stat;
  for (class_set_of_cat_stats::iterator i = cat_stats.begin();
       i != cat_stats.end(); i++)
    {
      const string &cat_name = i->first;

      if (excluded_categories.find(cat_name) != excluded_categories.end()) {
	continue;
      }

      cat_stat &stat = i->second;

      stat.compute();
      global_stat.push(stat);
      fprintf(out_stream, "%s\t%u\t%u\t%u\t%f\t%f\t%f\t%f\n",
	      cat_name.c_str(), stat.a, stat.b, stat.c,
	      stat.precision, stat.recall, stat.f1, stat.BEP);
    }
  global_stat.compute();
  fprintf(out_stream, "%f\t%f\t%f\t%f\t%f\t%f\n",
	  global_stat.avg_precision, global_stat.avg_recall,
	  global_stat.u_avg_precision, global_stat.u_avg_recall,
	  global_stat.u_avg_f1, global_stat.u_avg_BEP);
}
MAIN_END
