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
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "utility.h"
#include "utility.hpp"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static unordered_set<string> stop_list;
static string word;

static inline void stop_list_partial_fn(char *f)
{
  word.append(f);
}

static inline void stop_list_complete_fn(void)
{
  stop_list.insert(word);
  word.clear();
}

static inline void load_stop_list_file(const char *filename)
{
  open_in_stream(filename);

  tokenizer("\n", buffer, BUFFER_SIZE,
	    stop_list_partial_fn, stop_list_complete_fn);
}

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  if (stop_list.find(word) == stop_list.end()) {
    fprintf(out_stream, "%s\n", word.c_str());
  }
  word.clear();
}

MAIN_BEGIN(
"stop_list",
"If input file is not given, stdin is read for a list of paths of input files."
"\n"
"Otherwise, the input file is read for such a list.\n"
"Then, each of the file in the list is expected to contain words separated by\n"
"a newline character (i.e., '\\n').\n"
"Each line containing word found in the stop list given by the mandatory\n"
"option -D is removed from the file modifying the original file.\n"
"Once a file processing is completed, the path of the file is output to the\n"
"given file if an output file is specified, or otherwise, to stdout.\n"
"The mandatory option -D specifies the name of a file containing words\n"
"separated by a newline character (i.e., '\\n').\n",
"D:",
"-D STOP_LIST_FILE",
0,
case 'D':
/* Allocating tokenizing buffer */
buffer = static_cast<char *>(malloc(BUFFER_SIZE));
if (buffer == NULL) {
  fatal_error("Insufficient memory");
 }
/* End of allocation */

load_stop_list_file(optarg);
break;
) {

  if (buffer == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }
}
MAIN_INPUT_START
MAIN_LIST_OF_FILE_START
{
  string tmp_file_name(*file_path);
  tmp_file_name.append(".tmp");
  open_out_stream(tmp_file_name.c_str());

  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);

  recover_stdout();
  recover_stdin();

  if (rename(tmp_file_name.c_str(), file_path->c_str()) != 0) {
    fatal_syserror("Cannot rename %s to %s", tmp_file_name.c_str(),
		   file_path->c_str());
  }

  fprintf(out_stream, "%s\n", get_file_name(file_path->c_str()));
}
MAIN_LIST_OF_FILE_END
MAIN_INPUT_END
MAIN_END
