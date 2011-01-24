#ifndef UTILITY_VECTOR_H
#define UTILITY_VECTOR_H

#include <stdio.h>
#include <string.h>
#include "utility.h"

#ifdef __cplusplus
extern "C" {
#endif

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
						  double value))
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

    i++;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_VECTOR_H */
