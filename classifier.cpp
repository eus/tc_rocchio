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
#include <string>
#include "utility.h"
#include "utility.hpp"
#include "utility_vector.h"

using namespace std;

static char *buffer = NULL;
static double *dot_products = NULL;
static size_t dot_products_size;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
  if (dot_products != NULL) {
    free(dot_products);
  }
} CLEANUP_END

typedef pair<string, class_sparse_vector> class_W_vector;
typedef vector<class_W_vector> class_W_vectors;
static class_W_vectors W_vectors;
static unsigned int W_vectors_count;

static unsigned int vector_size;
static inline void vector_size_fn(unsigned int size)
{
  vector_size = size;
}

string word;
static inline void string_partial_fn(char *str)
{
  word.append(str);
}

static class_sparse_vector *W;
static inline void string_complete_fn(void)
{
  W_vectors.push_back(class_W_vector());

  class_W_vector &W_vector = W_vectors.back();
  W_vector.first.append(word);
  W_vector.second = class_sparse_vector();
  W = &W_vector.second;

  word.clear();
}

static inline void offset_count_fn(unsigned int count)
{
}

static inline void double_fn(unsigned int index, double value)
{
  (*W)[index] = value;
}

static inline void load_profile_file(char *filename)
{
  in_stream = fopen(filename, "r");
  in_stream_name = filename;
  if (in_stream == NULL) {
    fatal_syserror("Cannot open PROFILE file %s", filename);
  }

  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, offset_count_fn, double_fn);
}

static inline void vector_size_fn_dot(unsigned int size)
{
  if (size != vector_size) {
    fatal_error("w vector and W vector have different sizes");
  }
}

static inline void string_partial_fn_dot(char *str)
{
  word.append(str);
}

static inline void string_complete_fn_dot(void)
{
  fprintf(out_stream, "%s ", word.c_str()); // Output document name

  word.clear();

  memset(dot_products, 0, dot_products_size);
}

static unsigned int offset_count;
static inline void offset_count_fn_dot(unsigned int count)
{
  offset_count = count;
}

static inline void double_fn_dot(unsigned int index, double value)
{
  static unsigned int count = 0;

  count++;

  for (unsigned int i = 0; i < W_vectors_count; i++) {

    class_sparse_vector &W = W_vectors[i].second;
    class_sparse_vector::iterator entry = W.find(index);
    if (entry != W.end()) { // W[index] is not zero
      dot_products[i] += entry->second * value; // the dot product
    }
  }

  if (count == offset_count) { // All entries of w vector have been processed

    count = 0;

    /* Output the highest category */
    int cat_idx = 0; // If all dot products are the same, pick the first cat
    double max_dot_product = dot_products[0];
    for (unsigned int i = 1; i < W_vectors_count; i++) {
      if (max_dot_product < dot_products[i]) {
	max_dot_product = dot_products[i];
	cat_idx = i;
      }
    }
    fprintf(out_stream, "%s\n", W_vectors[cat_idx].first.c_str());
  }
}

static char *profile_file_name;
MAIN_BEGIN(
"classifier",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of document names and\n"
"their corresponding w vectors in the following binary format whose\n"
"endianness follows that of the host machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes)        |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated doc_name_1 |Q_1| off_1_1 | w_1_1 | ... | off_Q_1 | w_Q_1 |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"|                                    ...                                   |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated doc_name_M |Q_M| off_1_M | w_1_M | ... | off_Q_M | w_Q_M |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"Q_i is an unsigned int datum (4 bytes) expressing the number of\n"
"offset-element pairs of sparse vector representation of document i.\n"
"off_i_j is an unsigned int datum (4 bytes) expressing the offset of\n"
"element i within sparse vector j.\n"
"w_i_j is a double datum (8 bytes) expressing the value at position i within\n"
"sparse vector j.\n"
"Logically, the file should come from w_to_vector processing unit.\n"
"The mandatory option -D specifies the name of PROFILE file containing the\n"
"profile vectors of K categories in the following binary format whose\n"
"endianness is expected to be that of the host machine:\n"
"+--------------------------------------------------------------------------+\n"
"| Normal vector size of the sparse vector in unsigned int (4 bytes)        |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated cat_name_1 |Q_1| off_1_1 | w_1_1 | ... | off_Q_1 | w_Q_1 |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"|                                    ...                                   |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"| NULL-terminated cat_name_K |Q_K| off_1_K | w_1_K | ... | off_Q_K | w_Q_K |\n"
"+----------------------------+---+---------+-------+-----+---------+-------+\n"
"Logically, the file should come from rocchio processing unit.\n"
"Then, this processing unit will calculate the dot product of each document\n"
"weight vector w with all profile vectors. Then, following OVA\n"
"(One-vs-All) approach, the document represented by the weight vector w is\n"
"classified into the category whose profile vector W gives the maximum\n"
"dot product.\n"
"Finally, the result will be in the following format:\n"
"DOC_NAME CAT_NAME\\n\n"
"The result is output to the given file if an output file is specified, or to\n"
"stdout otherwise.\n",
"D:",
"-D PROFILE_FILE",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }

profile_file_name = optarg;
/* End of allocation */
break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }

  load_profile_file(profile_file_name);
  W_vectors_count = W_vectors.size();
  if (W_vectors_count == 0) {
    fatal_error("Cannot carry out dot product: PROFILE file is empty");
  }

  dot_products_size = sizeof(*dot_products) * W_vectors_count;
  dot_products = static_cast<double *>(malloc(dot_products_size));
  if (dot_products == NULL) {
    fatal_error("No memory to store all profile vectors"
		" (try to program differently)");
  }
}
MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn_dot, string_partial_fn_dot,
	       string_complete_fn_dot, offset_count_fn_dot, double_fn_dot);
}
MAIN_INPUT_END
MAIN_END
