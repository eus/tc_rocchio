#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <unordered_map>
#include <string>
#include <list>
#include "utility.h"

#define MAIN_LIST_OF_FILE_START						\
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn_file, complete_fn_file); \
  for (class_input_file_paths::iterator file_path = input_file_paths.begin(); \
       file_path != input_file_paths.end(); file_path++)		\
    {									\
      open_in_stream(file_path->c_str());

#define MAIN_LIST_OF_FILE_END }

using namespace std;

typedef unsigned int class_sparse_vector_offset;

/* An empty sparse vector means that all entries are zero. */
typedef unordered_map<class_sparse_vector_offset, double> class_sparse_vector;

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
