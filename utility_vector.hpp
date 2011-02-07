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

#ifndef UTILITY_VECTOR_HPP
#define UTILITY_VECTOR_HPP

#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "utility.h"

using namespace std;

typedef unsigned int class_sparse_vector_offset;

/* An empty sparse vector means that all entries are zero. */
typedef unordered_map<class_sparse_vector_offset, double> class_sparse_vector;

/* Assign weight * src to dst */
static inline void assign_weighted_sparse_vector(class_sparse_vector &dst,
						 const class_sparse_vector &src,
						 const double weight)
{
  dst.clear();

  for (class_sparse_vector::const_iterator i = src.cbegin();
       i != src.cend(); i++)
    {
      dst[i->first] = weight * i->second;
    }
}

/* Add src to dst */
static inline void add_sparse_vector(class_sparse_vector &dst,
				     const class_sparse_vector &src)
{
  for (class_sparse_vector::const_iterator i = src.cbegin();
       i != src.cend(); i++)
    {
      dst[i->first] += i->second;
    }
}

static inline double dot_product_sparse_vector(const class_sparse_vector &a,
					       const class_sparse_vector &b)
{
  const class_sparse_vector *v1, *v2; /* v1.size() <= v2.size() */
  double result = 0;

  /* Set to work on the shorter vector */
  if (a.size() > b.size()) {
    v1 = &b;
    v2 = &a;
  } else {
    v1 = &a;
    v2 = &b;
  }
  /* End of setting */

  for (class_sparse_vector::const_iterator i = v1->cbegin(); i != v1->cend();
       i++)
    {
      class_sparse_vector::const_iterator e_ptr = v2->find(i->first);
      if (e_ptr == v2->cend()) {
	continue;
      }

      result += e_ptr->second * i->second;
    }

  return result;
}

static inline void load_next_chunk(char *buffer, size_t buffer_size,
				   size_t *byte_read, size_t *offset)
{
  *byte_read = fread(buffer, 1, buffer_size - 1, in_stream);
  buffer[buffer_size - 1] = '\0'; // Avoid buffer overflow
  *offset = 0;
}

static inline int read_string(char *buffer, size_t buffer_size,
			      size_t *byte_read, size_t *offset,
			      void (*string_partial_fn)(char *),
			      unsigned int i)
{
  if (*offset == *byte_read) {

    if (*byte_read < buffer_size - 1) { // EOF
      return 0;
    }

    // We land at the chunk boundary; load next one
    load_next_chunk(buffer, buffer_size, byte_read, offset);

    if (*byte_read == 0) { // EOF
      return 0;
    }
  }

  size_t len = strlen(buffer + *offset);
  if (len == 0) { // Empty string is fine
    goto out;
  }

  string_partial_fn(buffer + *offset);
  *offset += len;

  while (*offset == *byte_read) { // Truncated word

    load_next_chunk(buffer, buffer_size, byte_read, offset);

    if (*byte_read == 0) { // Empty stream
      fatal_error("Malformed record #%u: corrupted string", i + 1);
    }

    len = strlen(buffer);
    if (len == 0) { // Empty string is fine
      break;
    }

    string_partial_fn(buffer);
    *offset += len;    
  }

 out:
  *offset += 1;

  return 1;
}

static inline void read_fixed_datum(char *buffer, size_t buffer_size,
				    size_t *byte_read, size_t *offset,
				    unsigned int i, int j,
				    void *result, size_t result_size)
{
  if (*offset == *byte_read) {

    if (*byte_read < buffer_size - 1) { // EOF
      fatal_error("Malformed record #%u: incomplete vector", i + 1);
    }

    // We land at the chunk boundary; load next one
    load_next_chunk(buffer, buffer_size, byte_read, offset);

    if (*byte_read == 0) { // EOF
      fatal_error("Malformed record #%u: incomplete vector", i + 1);
    }
  }

  char *ptr = (char *) result;
  size_t remaining = result_size;
  while (1) {
    size_t len = *byte_read - *offset;
    if (len >= remaining) {
      memcpy(ptr, buffer + *offset, remaining);
      *offset += remaining;
      break;
    }

    remaining -= len;
    memcpy(ptr, buffer + *offset, len);
    ptr += len;

    load_next_chunk(buffer, buffer_size, byte_read, offset);
    if (*byte_read == 0) {
      fatal_error("Malformed record #%u: corrupted vector at element #%u",
		  i + 1, j + 1);
    }
  }
}

static inline void parse_vector(char *buffer, size_t buffer_size,
				void (*vector_size_fn)(unsigned int size),
				void (*string_partial_fn)(char *str),
				void (*string_complete_fn)(void),
				void (*offset_count_fn)(unsigned int count),
				void (*double_fn)(unsigned int index,
						  double value),
				void (*end_of_vector_fn)(void))
{
  size_t block_read;
  unsigned int count;
  block_read = fread(&count, sizeof(count), 1, in_stream);
  if (block_read == 0) {
    fatal_error("Malformed input: cannot read record count");
  }
  vector_size_fn(count);

  size_t offset = 0;
  load_next_chunk(buffer, buffer_size, &block_read, &offset);

  if (block_read == 0) { // Empty stream
    return;
  }

  unsigned int i = 0;
  while (1) {
    if (read_string(buffer, buffer_size,
		    &block_read, &offset,
		    string_partial_fn, i) == 0) { // No more record
      break;
    }

    string_complete_fn();

    read_fixed_datum(buffer, buffer_size, &block_read, &offset, i, -1,
		     &count, sizeof(count));
    offset_count_fn(count);

    unsigned int j = 0;
    struct sparse_vector_entry e;
    while (j < count) {
      read_fixed_datum(buffer, buffer_size, &block_read, &offset, i, j,
		       &e, sizeof(e));
      double_fn(e.offset, e.value);
      j++;
    }

    end_of_vector_fn();

    i++;
  }
}

#endif /* UTILITY_VECTOR_HPP */
