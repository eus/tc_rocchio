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
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "utility.h"

using namespace std;

static char *buffer = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
} CLEANUP_END

static unordered_map<string, int> feature_list;
static string word;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  feature_list[word] += 1;
  word.clear();
}

MAIN_BEGIN(
"tf",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to contain words separated by a newline\n"
"character (i.e., '\\n'). Logically the words should come from a single\n"
"document since this is the semantic of TF (Term Frequency).\n"
"Next, the occurence of every unique word is counted.\n"
"The result takes the following form:\n"
"UNIQUE_WORD WORD_COUNT\\n\n"
"and the result is output to stdout if no output file is given. Otherwise,\n"
"the result is output to the given file.\n",
"",
"",
0,
NO_MORE_CASE
) {
  /* Allocate tokenizing buffer */
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }
}
MAIN_INPUT_START
{
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
}
MAIN_INPUT_END
{
  for (unordered_map<string, int>::const_iterator i = feature_list.cbegin();
       i != feature_list.end();
       ++i) {
    fprintf(out_stream, "%s %d\n", i->first.c_str(), i->second);
  }
}
MAIN_END
