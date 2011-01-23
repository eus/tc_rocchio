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

#include <vector>
#include <string>
#include "../../utility.h"
#include "../../utility_vector.h"
#include "reader_vec_cpp_output.h"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static inline void vector_size_fn(unsigned int size)
{
}

static string word;
static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static unsigned int first_time = 1;
static inline void string_complete_fn(void)
{
  if (first_time) {
    first_time = 0;
  } else {
    outfile << endl;
  }

  outfile << word;
  word.clear();
}

static inline void double_fn(unsigned int index, double value)
{
  outfile << " " << value;
}

MAIN_BEGIN(
"reader_vec_14",
"C++ file output.\n",
"",
"",
0,
NO_MORE_CASE
) {

  set_cpp_output();

  /* Allocating tokenizing buffer */
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }
  /* End of allocation */
}
MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, double_fn);
  if (!first_time) {
    outfile << endl;
  }
}
MAIN_INPUT_END
MAIN_END
