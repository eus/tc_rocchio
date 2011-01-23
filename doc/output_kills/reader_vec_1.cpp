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

using namespace std;

#define BUFFERED_VECTOR_COUNT 16

typedef vector<double> class_w_vector;
typedef pair<string, class_w_vector> class_doc;
typedef vector<class_doc> class_docs;
static class_docs docs;
static unsigned int doc_idx;
static unsigned int vector_size;
static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static inline void output_to_file(void)
{
  for (unsigned int i = 0; i < doc_idx; i++) {
    fprintf(out_stream, "%s", docs[i].first.c_str());
    for (unsigned int j = 0; j < vector_size; j++) {
      fprintf(out_stream, " %f", docs[i].second[j]);
    }
    fputc('\n', out_stream);
  }
}

static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;

  class_doc doc = class_doc();
  doc.second = class_w_vector(size, 0);

  docs = class_docs(BUFFERED_VECTOR_COUNT, doc);
}

static inline void string_partial_fn(char *str)
{
  if (doc_idx == BUFFERED_VECTOR_COUNT) {
    for (unsigned int i = 0; i < BUFFERED_VECTOR_COUNT; i++) {
      docs[i].first.clear();
    }
    doc_idx = 0;
  }

  docs[doc_idx].first.append(str);
}

static inline void string_complete_fn(void)
{
  doc_idx++;
}

static inline void double_fn(unsigned int index, double value)
{
  docs[doc_idx - 1].second[index] = value;
}

MAIN_BEGIN(
"reader_vec_1",
"Using vector<pair<string, vector<double>>> without outputting.\n",
"",
"",
0,
NO_MORE_CASE
) {
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
#ifdef TESTING
  /* Only for use with small_w_vectors_for_test.bin */
  output_to_file();
#endif
}
MAIN_INPUT_END
MAIN_END
