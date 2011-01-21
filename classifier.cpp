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
#include "utility_vector.h"

using namespace std;

static char *buffer = NULL;
static double *W_vectors = NULL;
static double *w_vector = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
  if (W_vectors != NULL) {
    free(W_vectors);
  }
  if (w_vector != NULL) {
    free(w_vector);
  }
} CLEANUP_END

typedef vector<string> class_cat_list;
static class_cat_list cat_list;
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

static inline void string_complete_fn(void)
{
  cat_list.push_back(word);
  word.clear();
}

static inline void double_fn(unsigned int index, double value)
{
  static size_t offset = 0;

  if (index == 0) {
    double *b = static_cast<double *>(realloc(W_vectors,
					      (sizeof(*W_vectors) * vector_size
					       * cat_list.size())));
    if (b == NULL) {
      fatal_error("Insufficient memory to load all W vectors"
		  " (try to program differently)");
    }
    W_vectors = b;
  }

  W_vectors[offset++] = value;
}

static inline void load_profile_file(char *filename)
{
  in_stream = fopen(filename, "r");
  in_stream_name = filename;
  if (in_stream == NULL) {
    fatal_syserror("Cannot open PROFILE file %s", filename);
  }

  parse_vector(buffer, BUFFER_SIZE, vector_size_fn, string_partial_fn,
	       string_complete_fn, double_fn);
}

static inline void vector_size_fn_dot(unsigned int size)
{
  if (size != vector_size) {
    fatal_error("w vector and W vector have different sizes");
  }

  w_vector = static_cast<double *>(malloc(sizeof(*w_vector) * size));
}

static inline void string_partial_fn_dot(char *str)
{
  word.append(str);
}

static inline void string_complete_fn_dot(void)
{
  fprintf(out_stream, "%s ", word.c_str()); // Output document name

  word.clear();
}

static inline void double_fn_dot(unsigned int index, double value)
{
  w_vector[index] = value;

  if (index + 1 == vector_size) { // All entries of w vector have been loaded
    double max_dot_product = 0;
    int cat_idx = 0; // If all dot products are zero, just pick the first cat

    /* Time to do dot product with W_vectors in one swipe in the hope of
     * getting the maximum number of cache hits possible
     */
    unsigned int W_vectors_count = cat_list.size();
    for (unsigned int i = 0; i < W_vectors_count; i++) {
      double dot_product = 0;
      size_t offset = i * vector_size;
      for (unsigned int j = 0; j < vector_size; j++) {
	dot_product += W_vectors[offset + j] * w_vector[j];
      }

      if (dot_product > max_dot_product) {
	max_dot_product = dot_product;
	cat_idx = i;
      }
    }

    /* Output the highest category */
    fprintf(out_stream, "%s\n", cat_list[cat_idx].c_str());
  }
}

MAIN_BEGIN(
"classifier",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have a list of document names and\n"
"their corresponding w vectors in the following binary format whose\n"
"endianness follows that of the host machine; size of `double' is 8 bytes:\n"
"+----------------------------------------------------------------------+\n"
"| N in unsigned int (4 bytes)                                          |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated doc_name_1 | w_1_1 in double | ... | w_N_1 in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"|                                 ...                                  |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated doc_name_M | w_1_1 in double | ... | w_N_M in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"Logically, the file should come from w_to_vector processing unit.\n"
"The mandatory option -D specifies the name of PROFILE file containing the\n"
"profile vectors of K categories in the following binary format whose\n"
"endianness is expected to be that of the host machine:\n"
"+----------------------------------------------------------------------+\n"
"| N in unsigned int (4 bytes)                                          |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated cat_name_1 | W_1_1 in double | ... | W_N_1 in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"|                                 ...                                  |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
"| NULL-terminated cat_name_K | W_1_1 in double | ... | W_N_K in double |\n"
"+----------------------------+-----------------+-----+-----------------+\n"
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
/* End of allocation */

load_profile_file(optarg);

break;

) {
  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
}
MAIN_INPUT_START
{
  parse_vector(buffer, BUFFER_SIZE, vector_size_fn_dot, string_partial_fn_dot,
	       string_complete_fn_dot, double_fn_dot);
}
MAIN_INPUT_END
MAIN_END
