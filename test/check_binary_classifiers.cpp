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

#include <list>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "utility.h"
#include "utility.hpp"
#include "utility_vector.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

#include "utility_idf_dic.hpp"

typedef pair<string /* cat name */,
	     pair<double /* Th */,
		  class_sparse_vector>> class_classifier_list_entry;
typedef list<class_classifier_list_entry> class_classifier_list;
static class_classifier_list classifier_list;
static unsigned int vector_size;
static string word;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
}

static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static class_classifier_list_entry *active_entry;
static inline void string_complete_fn(void)
{
  classifier_list.push_back(class_classifier_list_entry());
  active_entry = &classifier_list.back();
  active_entry->first = word;

  word.clear();
}

static inline void offset_count_fn(unsigned int count)
{
}

static inline void double_fn(unsigned int index, double value)
{
  if (index == vector_size) {
    active_entry->second.first = value;
  } else if (index < vector_size) {
    active_entry->second.second[index] = value;
  }
}

static inline void end_of_vector_fn(void)
{
}

static double tolerance;
#define check_discrepancy(roi, mine, ...) do {		\
    double diff = roi - mine;				\
    if ((diff > tolerance) || (diff < -tolerance)) {	\
      fprintf(out_stream, __VA_ARGS__);			\
    }							\
  } while (0)

static inline void th_partial_fn(char *f)
{
  word.append(f);
}

static inline void th_complete_fn(void)
{
  double roi_th = strtod(word.c_str(), NULL);
  double &my_th = active_entry->second.first;

  check_discrepancy(roi_th, my_th, "* %s %f %f %f\n",
		    active_entry->first.c_str(), diff, roi_th, my_th);

  word.clear();
}

static unsigned int w_offset = 0;
static inline void w_partial_fn(char *f)
{
  word.append(f);
}

static inline void w_complete_fn(void)
{
  double roi_w_f = strtod(word.c_str(), NULL);

  class_sparse_vector &w_vector = active_entry->second.second;
  double my_w_f = 0;
  class_sparse_vector::const_iterator e = w_vector.find(w_offset);
  if (e != w_vector.end()) {
    my_w_f = e->second;
  }

  check_discrepancy(roi_w_f, my_w_f, "%s %s %f %f %f\n",
		    active_entry->first.c_str(),
		    idf_list_reversed.find(w_offset)->second.first.c_str(),
		    diff, roi_w_f, my_w_f);
  
  w_offset++;
  word.clear();
}

static char *roi_data_dir;
MAIN_BEGIN(
"check_binary_classifiers",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have the following sparse\n"
"vector structure whose endianness follows that of the host machine:\n"
"+-------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes) |\n"
"+-----------------------------+---+-------+-----+-----+-------+-----+\n"
"| NULL-terminated string      | Q | off_1 | w_1 | ... | off_Q | w_Q |\n"
"+-----------------------------+---+-------+-----+-----+-------+-----+\n"
"|                                ...                                |\n"
"+-------------------------------------------------------------------+\n"
"The input stream must come from rocchio processing unit without using any ES\n"
"and P is set to 1.\n"
"Then, for each classifier (an entry in the sparse vector structure) named\n"
"CAT, two files CAT.weight and CAT.th are opened in the directory specified\n"
"using the mandatory option -D.\n"
"Next, the entries and the threshold of the classifier are matched with those\n"
"given in the CAT.weight and CAT.th files, respectively, using the tolerance\n"
"that is specified using the mandatory option -T.\n"
"For each deviation above the specified tolerance, the following is written\n"
"to the given file if an output file is specified or to stdout otherwise:\n"
"1. Deviation in the threshold will output:\n"
"* CAT_NAME ROI_TH-MY_TH ROI_TH MY_TH\\n\n"
"2. Deviation in the feature weight will output:\n"
"CAT_NAME FEATURE ROI_w_f-MY_w_f ROI_w_f MY_w_f\\n\n"
"To resolve FEATURE from the offset, the mandatory option -R is used to\n"
"specify the output file produced by idf_dic processing unit.\n",
"D:R:T:",
"-D ROI_CLASSIFIERS_DIR -R FILE_IDF_DIC -T TOLERATED_DIFF_IN_DOUBLE",
0,

case 'D':
roi_data_dir = optarg;
break;

case 'R':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_idf_dic_file_reversed_mapping(optarg);
break;

case 'T':
tolerance = strtod(optarg, NULL);
if (tolerance < 0) {
  fatal_error("Tolerance must be greater than or equal to 0");
}
break;
)

  if (buffer == NULL) {
    fatal_error("-R must be specified (-h for help)");
  }
  if (roi_data_dir == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }

MAIN_INPUT_START

parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	     string_complete_fn, offset_count_fn, double_fn,
	     end_of_vector_fn);

MAIN_INPUT_END

for (class_classifier_list::iterator i = classifier_list.begin();
     i != classifier_list.end();
     i++)
  {
    active_entry = &(*i);
    string &cat_name = i->first;

    string th_file(roi_data_dir);
    th_file.push_back(OS_PATH_DELIMITER);
    th_file.append(cat_name).append(".th");
    open_in_stream(th_file.c_str());
    tokenizer("\n", buffer, BUFFER_SIZE, th_partial_fn, th_complete_fn);

    w_offset = 0;
    string w_file(roi_data_dir);
    w_file.push_back(OS_PATH_DELIMITER);
    w_file.append(cat_name).append(".weight");
    open_in_stream(w_file.c_str());
    tokenizer("\n", buffer, BUFFER_SIZE, w_partial_fn, w_complete_fn);
  }

MAIN_END
