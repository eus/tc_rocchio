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
#include "utility.h"
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
"Next, this processing unit filters out sparse vectors whose names are given\n"
"in FILTER_FILE that is specified using the optional option -D. Each line in\n"
"FILTER_FILE specifies a name to be filtered out. To invert the filter,\n"
"specify the optional option -v. In this way, only the pairs whose names are\n"
"in the FILTER_FILE are output.\n"
"The remaining pairs of name and sparse vector are output in the same binary\n"
"format as the input to stdout if no output file is given, or to the given\n"
"file otherwise.\n",
"D:v",
"[-D FILTER_FILE [-v]]",
1,
case 'D': {
  filter_file_path = optarg;
}
break;
case 'v': {
  filter_file_inverted = 1;
}
break;
)

enum operational_mode {
  NOTHING,
  FILTERING,
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
}
/* End of determining operational mode */

MAIN_INPUT_START /* Reading input stream */

parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	     string_complete_fn, offset_count_fn, double_fn,
	     end_of_vector_fn);

MAIN_INPUT_END /* End of reading input stream */

/* Process name-vector entries */

class_output_buffer output_buffer;
for (class_name_vec_list::iterator name_vec = name_vec_list.begin();
     name_vec != name_vec_list.end();
     name_vec++)
  {
    if (mode == FILTERING) {
      if ((!filter_file_inverted
	   && filter_file.find(name_vec->first) == filter_file.end())
	  || (filter_file_inverted
	      && filter_file.find(name_vec->first) != filter_file.end()))
	{
	  output_buffer_insert(name_vec->second, output_buffer);
	}
    }
  }
/* End of processing */

output_result(output_buffer, vector_size);

MAIN_END

