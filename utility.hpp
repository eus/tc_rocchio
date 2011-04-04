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

#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <unordered_map>
#include <string>
#include <list>
#include "utility.h"

#define foreach(type, container, itr)		\
  for (type::iterator itr = container.begin();	\
       itr != container.end();			\
       itr++)

#define const_foreach(type, container, itr)		\
  for (type::const_iterator itr = container.begin();	\
       itr != container.end();				\
       itr++)

#define foreach_r(type, container, itr)			\
  for (type::reverse_iterator itr = container.rbegin();	\
       itr != container.rend();				\
       itr++)

#define const_foreach_r(type, container, itr)			\
  for (type::const_reverse_iterator itr = container.rbegin();	\
       itr != container.rend();					\
       itr++)

#define MAIN_LIST_OF_FILE_START						\
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn_file, complete_fn_file); \
  for (class_input_file_paths::iterator file_path = input_file_paths.begin(); \
       file_path != input_file_paths.end(); file_path++)		\
    {									\
      open_in_stream(file_path->c_str());

#define MAIN_LIST_OF_FILE_END }

using namespace std;

static string name_in_list_of_file;
static inline void partial_fn_file(char *path)
{
  name_in_list_of_file.append(path);
}

typedef list<string> class_input_file_paths;
static class_input_file_paths input_file_paths;
static inline void complete_fn_file(void)
{
  input_file_paths.push_back(name_in_list_of_file);
  name_in_list_of_file.clear();
}

#endif /* UTILITY_HPP */
