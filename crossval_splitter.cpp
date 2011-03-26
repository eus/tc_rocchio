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
#include <list>
#include <unordered_map>
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

static inline void output(FILE *file_doc, FILE *file_doc_cat,
			  FILE *file_doc_cat_pm, const string &doc_name,
			  const string &path, const class_set_of_cats &GS,
			  const char *excluded_cat_name)
{
  fprintf(file_doc, "%s\n", path.c_str());

  bool not_excluded = GS.find(string(excluded_cat_name)) == GS.end();

  for (class_set_of_cats::const_iterator cat = GS.begin();
       cat != GS.end();
       cat++)
    {
      fprintf(file_doc_cat_pm, "%s %s\n", doc_name.c_str(), cat->c_str());
      if (not_excluded) {
	fprintf(file_doc_cat, "%s %s\n", doc_name.c_str(), cat->c_str());
      }
    }
}

/* Reading DOC_CAT file */
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
/* End of reading DOC_CAT file */

/* Reading input file */
string path;
static inline void partial_fn(char *f)
{
  path.append(f);
}

static unsigned int testset_percentage_set = 0;
static unsigned int testset_percentage;
static const unsigned int PERCENTAGE_MULTIPLIER = 1000000;
static const char *excluded_cat_name = NULL;
static FILE *file_training_doc = NULL;
static FILE *file_training_doc_cat = NULL;
static FILE *file_training_doc_cat_pm = NULL;
static FILE *file_testing_doc = NULL;
static FILE *file_testing_doc_cat = NULL;
static FILE *file_testing_doc_cat_pm = NULL;
static inline void complete_fn(void)
{
  string doc_name(get_file_name(path.c_str()));

  class_doc_cat_list::iterator GS = gold_standard.find(doc_name);
  if (GS == gold_standard.end()) {
    fatal_error("Document %s has no category", doc_name.c_str());
  }

  if ((uniform_deviate(rand()) * PERCENTAGE_MULTIPLIER) < testset_percentage) {
    output(file_testing_doc, file_testing_doc_cat, file_testing_doc_cat_pm,
	   doc_name, path, GS->second, excluded_cat_name);
  } else {
    output(file_training_doc, file_training_doc_cat, file_training_doc_cat_pm,
	   doc_name, path, GS->second, excluded_cat_name);
  }

  path.clear();
}
/* End of reading input file */

static const char *file_training_doc_path = NULL;
static const char *file_training_doc_cat_path = NULL;
static const char *file_training_doc_cat_pm_path = NULL;
static const char *file_testing_doc_path = NULL;
static const char *file_testing_doc_cat_path = NULL;
static const char *file_testing_doc_cat_pm_path = NULL;
MAIN_BEGIN(
"crossval_splitter",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of file paths, the last\n"
"part of which is expected to be a document name.\n"
"Next, this processing unit randomly takes a percentage of the list of paths\n"
"to be designated as testing set. The percentage is specified using the\n"
"mandatory option -P. The remaining paths are designated as training set.\n"
"Finally, the paths in the training set are output to the file specified\n"
"using the mandatory option -1. For each path, the document name and the\n"
"corresponding categories as resolved using the DOC_CAT file specified using\n"
"the mandatory option -D is output to the file specified using the mandatory\n"
"option -3 (if a document has N categories, N lines will be output). The\n"
"same data is output to the file specified using the mandatory option -2\n"
"except that all lines containing document names that are assigned to the\n"
"excluded category as specified using the mandatory option -X are filtered\n"
"out. The same things are done to the paths in the testing set outputting the\n"
"data to the files specified using the mandatory option -3, -4 and -5,\n"
"respectively. It is a fatal error if a document name cannot be found in the\n"
"DOC_CAT_FILE. A string that does not correspond to any category name (e.g.,\n"
"an empty string) can be specified using -X when no category should be\n"
"excluded.\n",
"P:X:D:1:2:3:4:5:6:",
"-D DOC_CAT_FILE -P TESTSET_PERCENTAGE_OF_DOC\n"
"-X EXCLUDED_CAT_NAME\n"
"-1 TRAINING_DOC_FILE -2 TRAINING_DOC_CAT_FILE\n"
"-3 TRAINING_PERF_MEASURE_DOC_CAT_FILE\n"
"-4 TESTING_DOC_FILE -5 TESTING_DOC_CAT_FILE\n"
"-6 TESTING_PERF_MEASURE_DOC_CAT_FILE\n",
1,
case 'D': {
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }

  load_doc_cat_file(buffer, BUFFER_SIZE, optarg, doc_cat_fn);
}
break;
case 'P': {
  double num = strtod(optarg, NULL) / 100 * PERCENTAGE_MULTIPLIER;
  if (num < 0) {
    fatal_error("TESTSET_PERCENTAGE_OF_DOC must be >= 0");
  }
  testset_percentage = static_cast<unsigned int>(num);
  testset_percentage_set = 1;
}
break;
case 'X': {
  excluded_cat_name = optarg;
}
break;
case '1': {
  file_training_doc = open_local_out_stream(optarg);
  file_training_doc_path = optarg;
}
break;
case '2': {
  file_training_doc_cat = open_local_out_stream(optarg);
  file_training_doc_cat_path = optarg;
}
break;
case '3': {
  file_training_doc_cat_pm = open_local_out_stream(optarg);
  file_training_doc_cat_pm_path = optarg;
}
break;
case '4': {
  file_testing_doc = open_local_out_stream(optarg);
  file_testing_doc_path = optarg;
}
break;
case '5': {
  file_testing_doc_cat = open_local_out_stream(optarg);
  file_testing_doc_cat_path = optarg;
}
break;
case '6': {
  file_testing_doc_cat_pm = open_local_out_stream(optarg);
  file_testing_doc_cat_pm_path = optarg;
}
break;
) {

  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
  if (!testset_percentage_set) {
    fatal_error("-P must be specified (-h for help)");
  }
  if (excluded_cat_name == NULL) {
    fatal_error("-X must be specified (-h for help)");
  }
  if (file_training_doc_path == NULL) {
    fatal_error("-1 must be specified (-h for help)");
  }
  if (file_training_doc_cat_path == NULL) {
    fatal_error("-2 must be specified (-h for help)");
  }
  if (file_training_doc_cat_pm_path == NULL) {
    fatal_error("-3 must be specified (-h for help)");
  }
  if (file_testing_doc_path == NULL) {
    fatal_error("-4 must be specified (-h for help)");
  }
  if (file_testing_doc_cat_path == NULL) {
    fatal_error("-5 must be specified (-h for help)");
  }
  if (file_testing_doc_cat_pm_path == NULL) {
    fatal_error("-6 must be specified (-h for help)");
  }
}
MAIN_INPUT_START

tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);

MAIN_INPUT_END

close_local_out_stream(file_training_doc, file_training_doc_path);
close_local_out_stream(file_training_doc_cat, file_training_doc_cat_path);
close_local_out_stream(file_training_doc_cat_pm, file_training_doc_cat_pm_path);
close_local_out_stream(file_testing_doc, file_testing_doc_path);
close_local_out_stream(file_testing_doc_cat, file_testing_doc_cat_path);
close_local_out_stream(file_testing_doc_cat_pm, file_testing_doc_cat_pm_path);

MAIN_END
