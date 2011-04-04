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
#include <vector>
#include <unordered_map>
#include <unordered_set>
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

/* Data structures */
typedef pair<string, class_sparse_vector> class_name_vec_pair;
typedef unordered_map<string, class_name_vec_pair> class_name_vec_list;

typedef vector<class_name_vec_pair *> class_output_buffer;
static inline void output_buffer_insert(class_name_vec_pair &name_vec,
					class_output_buffer &buffer)
{
  buffer.push_back(&name_vec);
}
/* End of data structures */

/* Outputting result */
static inline void output_result(const class_output_buffer &result,
				 unsigned int vector_size)
{
  size_t block_write = fwrite(&vector_size, sizeof(vector_size), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write normal vector size to output stream");
  }

  unsigned int i_cnt = 0;
  for (class_output_buffer::const_iterator entry = result.begin();
       entry != result.end();
       entry++)
    {
      const class_name_vec_pair *name_vec = *entry;

      block_write = fwrite(name_vec->first.c_str(),
			   name_vec->first.length() + 1, 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write name #%u to output stream", i_cnt + 1);
      }

      const class_sparse_vector &vec = name_vec->second;

      unsigned int Q = vec.size();
      block_write = fwrite(&Q, sizeof(Q), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot output offset count of vector #%u", i_cnt + 1);
      }

      unsigned int k_cnt = 0;
      struct sparse_vector_entry e;
      for (class_sparse_vector::const_iterator k = vec.begin();
	   k != vec.end();
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
/* End of outputting result */

static class_name_vec_list name_vec_list; // All input data are stored here

/* Read (name, sparse vector) pairs */
static unsigned int vector_size = 0;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
}

static string name;
static inline void string_partial_fn(char *str)
{
  name.append(str);
}

static class_sparse_vector *active_vector = NULL;
static inline void string_complete_fn(void)
{
  name_vec_list[name].first = name;
  active_vector = &name_vec_list[name].second;
  name.clear();
}

static inline void offset_count_fn(unsigned int count)
{
}

static inline void double_fn(unsigned int index, double value)
{
  (*active_vector)[index] = value;
}

static inline void end_of_vector_fn(void)
{
}
/* End of reading pairs */

/* Reading filter file */
static inline void filtering_partial_fn(char *f)
{
  name.append(f);
}

typedef unordered_set<string> class_filter_file;
static class_filter_file filter_file;
static inline void filtering_complete_fn(void)
{
  filter_file.insert(name);
  name.clear();
}
/* End of reading filter file */

static class_name_vec_list ref_name_vec_list; /* All reference data are stored
					       * here
					       */

/* Read (name, sparse vector) reference pairs */
static unsigned int ref_vector_size = 0;
static inline void ref_vector_size_fn(unsigned int size)
{
  ref_vector_size = size;
}

static string ref_name;
static inline void ref_string_partial_fn(char *str)
{
  ref_name.append(str);
}

static class_sparse_vector *ref_active_vector = NULL;
static inline void ref_string_complete_fn(void)
{
  ref_name_vec_list[ref_name].first = ref_name;
  ref_active_vector = &ref_name_vec_list[ref_name].second;
  ref_name.clear();
}

static inline void ref_offset_count_fn(unsigned int count)
{
}

static inline void ref_double_fn(unsigned int index, double value)
{
  (*ref_active_vector)[index] = value;
}

static inline void ref_end_of_vector_fn(void)
{
}
/* End of reading reference pairs */

static const char *ref_file_path = NULL;
static double ref_tolerance = 0.0;
static const char *filter_file_path = NULL;
static int filter_file_inverted = 0;
MAIN_BEGIN(
"mod_vec",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of names and their\n"
"corresponding sparse vectors in the following binary format whose\n"
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
"Next, if the optional option -D is specified, this processing unit filters\n"
"out sparse vectors whose names are given in FILTER_FILE. Each line in\n"
"FILTER_FILE specifies a name to be filtered out. To invert the filter,\n"
"specify the optional option -v. In this way, only the pairs whose names are\n"
"in the FILTER_FILE are output.\n"
"The remaining pairs of name and sparse vector are output in the same binary\n"
"format as the input to stdout if no output file is given, or to the given\n"
"file otherwise.\n"
"If the optional option -C is specified, this processing unit compares the\n"
"input with the sparse vectors contained in the file whose name is specified\n"
"as the argument to option -C. The optional option -t can be specified to\n"
"specify a tolerance when comparing the vector elements. If the two set of\n"
"sparse vectors are completely the same, the output will be empty. Otherwise,\n"
"the output will report all differences that the set of sparse vectors given\n"
"in the input stream have in the following format:\n"
"{VECTOR_SIZE REFERENCE_SIZE THIS_SIZE\\n}?\n"
"{{ADDED A_NAME_IN_THIS_SET_THAT_IS_NOT_IN_THE_REFERENCE_SET\\n}*\n"
" | {MISSING A_NAME_IN_THE_REFERENCE_SET_THAT_IS_NOT_IN_THIS_SET\\n}*\n"
" | {DIFF NAME OFFSET REFERENCE_VALUE THIS_VALUE\\n}*}*\n"
"If no optional option is given, the given input is simply output.\n"
"In all operational mode, even if nothing is specified, this processing unit\n"
"may alter the original positions of the vector entries in a single vector.\n"
"For example, if the original vector is as follows:\n"
"+--------+---+---+-----+---+-----+\n"
"| name_1 | 2 | 3 | 1.2 | 5 | 3.4 |\n"
"+--------+---+---+-----+---+-----+, it may be altered as follows:\n"
"+--------+---+---+-----+---+-----+\n"
"| name_1 | 2 | 5 | 3.4 | 3 | 1.2 |\n"
"+--------+---+---+-----+---+-----+\n",
"D:vC:t:",
"[-D FILTER_FILE [-v]] | [-C SET_OF_REFERENCE_VECTORS [-t TOLERANCE]]",
1,
case 'D': {
  filter_file_path = optarg;
}
break;
case 'v': {
  filter_file_inverted = 1;
}
break;
case 'C': {
  ref_file_path = optarg;
}
break;
case 't': {
  ref_tolerance = strtod(optarg, NULL);
  if (ref_tolerance < 0) {
    ref_tolerance = -ref_tolerance;
  }
}
break;
)

enum operational_mode {
  NOTHING,
  FILTERING,
  COMPARING,
};
enum operational_mode mode = NOTHING;

/* Determining desired operational mode */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
}

if (filter_file_path != NULL) {
  open_in_stream(filter_file_path);
  tokenizer("\n", buffer, BUFFER_SIZE,
	    filtering_partial_fn, filtering_complete_fn);
  mode = FILTERING;
} else if (ref_file_path != NULL) {
  open_in_stream(ref_file_path);
  parse_vector(buffer, BUFFER_SIZE, ref_vector_size_fn, ref_string_partial_fn,
	       ref_string_complete_fn, ref_offset_count_fn, ref_double_fn,
	       ref_end_of_vector_fn);
  mode = COMPARING;
}
/* End of determining operational mode */

MAIN_INPUT_START /* Reading input stream */

parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	     string_complete_fn, offset_count_fn, double_fn,
	     end_of_vector_fn);

MAIN_INPUT_END /* End of reading input stream */

/* Process name-vector entries */

if (mode == FILTERING) {
  class_output_buffer output_buffer;

  foreach(class_name_vec_list, name_vec_list, name_vec) {
    if ((!filter_file_inverted
	 && filter_file.find(name_vec->first) == filter_file.end())
	|| (filter_file_inverted
	    && filter_file.find(name_vec->first) != filter_file.end()))
      {
	output_buffer_insert(name_vec->second, output_buffer);
      }
  }

  output_result(output_buffer, vector_size);

} else if (mode == COMPARING) {

  if (vector_size != ref_vector_size) {
    fprintf(out_stream, "VECTOR_SIZE %u %u\n", ref_vector_size, vector_size);
  }

  foreach(class_name_vec_list, name_vec_list, name_vec) {
    const string &name = name_vec->first;
    class_name_vec_list::iterator ref = ref_name_vec_list.find(name);

    if (ref == ref_name_vec_list.end()) {

      fprintf(out_stream, "ADDED %s\n", name.c_str());

    } else {

      class_sparse_vector &ref_vec = ref->second.second;
      class_sparse_vector &vec = name_vec->second.second;
      foreach(class_sparse_vector, vec, val) {
	class_sparse_vector::iterator ref_val = ref_vec.find(val->first);
	if (ref_val == ref_vec.end()) {
	  fprintf(out_stream, "DIFF %s %u %f %f\n", name.c_str(), val->first,
		  0.0, val->second);
	} else {
	  if (fabs(ref_val->second - val->second) > ref_tolerance) {
	    fprintf(out_stream, "DIFF %s %u %f %f\n", name.c_str(), val->first,
		    ref_val->second, val->second);
	  }

	  ref_vec.erase(ref_val);
	}
      }
      foreach(class_sparse_vector, ref_vec, ref_val) {
	fprintf(out_stream, "DIFF %s %u %f %f\n", name.c_str(), ref_val->first,
		ref_val->second, 0.0);
      }

      ref_name_vec_list.erase(ref);
    }
  }
  foreach(class_name_vec_list, ref_name_vec_list, ref_name_vec) {
    fprintf(out_stream, "MISSING %s\n", ref_name_vec->first.c_str());
  }

} else { // mode == NOTHING
  class_output_buffer output_buffer;

  foreach(class_name_vec_list, name_vec_list, name_vec) {
    output_buffer_insert(name_vec->second, output_buffer);
  }

  output_result(output_buffer, vector_size);
}
/* End of processing */

MAIN_END

