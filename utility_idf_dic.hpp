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

#ifndef UTILITY_IDF_DIC_HPP
#define UTILITY_IDF_DIC_HPP

#include <unordered_map>
#include <string>
#include <list>
#include "utility.h"

using namespace std;

typedef pair<unsigned int, double> class_idf_entry;
typedef unordered_map<string, class_idf_entry> class_idf_list;
static class_idf_list idf_list;

static string utility_idf_dic_string;      
static inline void utility_idf_dic_vector_size_fn(unsigned int size)
{
}

static inline void utility_idf_dic_string_partial_fn(char *str)
{
  utility_idf_dic_string.append(str);
}

static inline void utility_idf_dic_string_complete_fn(void)
{
}

static inline void utility_idf_dic_offset_count_fn(unsigned int count)
{
}

static unsigned int utility_idf_dic_M_has_been_taken = 0;
static unsigned int utility_idf_dic_vector_position = 0;
static inline void utility_idf_dic_double_fn(unsigned int index, double value)
{
  if (index != 0) {
    return;
  }

  if (utility_idf_dic_M_has_been_taken) {
    class_idf_entry &data = idf_list[utility_idf_dic_string];
    data.first = utility_idf_dic_vector_position++;
    data.second = value;
  }

  utility_idf_dic_string.clear(); 
}

static inline void utility_idf_dic_end_of_vector_fn(void)
{
  utility_idf_dic_M_has_been_taken = 1;
}

/* Map feature to offset and IDF */
static inline void load_idf_dic_file(const char *filename)
{
  open_in_stream(filename);

  parse_vector(buffer, BUFFER_SIZE,
	       utility_idf_dic_vector_size_fn,
	       utility_idf_dic_string_partial_fn,
	       utility_idf_dic_string_complete_fn,
	       utility_idf_dic_offset_count_fn,
	       utility_idf_dic_double_fn,
	       utility_idf_dic_end_of_vector_fn);
}

typedef pair<string, double> class_idf_entry_reversed;
typedef unordered_map<unsigned int,
		      class_idf_entry_reversed> class_idf_list_reversed;
static class_idf_list_reversed idf_list_reversed;

static inline void utility_idf_dic_vector_size_reversed_fn(unsigned int size)
{
}

static inline void utility_idf_dic_string_partial_reversed_fn(char *str)
{
  utility_idf_dic_string.append(str);
}

static inline void utility_idf_dic_string_complete_reversed_fn(void)
{
}

static inline void utility_idf_dic_offset_count_reversed_fn(unsigned int count)
{
}

static inline void utility_idf_dic_double_reversed_fn(unsigned int index,
						      double value)
{
  if (index != 0) {
    return;
  }

  if (utility_idf_dic_M_has_been_taken) {
    class_idf_entry_reversed &data
      = idf_list_reversed[utility_idf_dic_vector_position++];
    data.first = utility_idf_dic_string;
    data.second = value;
  }

  utility_idf_dic_string.clear(); 
}

static inline void utility_idf_dic_end_of_vector_reversed_fn(void)
{
  utility_idf_dic_M_has_been_taken = 1;
}

/* Map offset to feature and IDF */
static inline void load_idf_dic_file_reversed_mapping(const char *filename)
{
  open_in_stream(filename);

  parse_vector(buffer, BUFFER_SIZE,
	       utility_idf_dic_vector_size_reversed_fn,
	       utility_idf_dic_string_partial_reversed_fn,
	       utility_idf_dic_string_complete_reversed_fn,
	       utility_idf_dic_offset_count_reversed_fn,
	       utility_idf_dic_double_reversed_fn,
	       utility_idf_dic_end_of_vector_reversed_fn);
}

#endif /* UTILITY_IDF_DIC_HPP */
