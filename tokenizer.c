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

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utility.h"

static char *d;

CLEANUP_BEGIN
{
  if (d != NULL) {
    free(d);
  }
} CLEANUP_END

static char *delimiter = " ,.-;:?!()[]\"'{}\r\n\t\v";

static inline void strtolower(char *s)
{
  int i = 0;

  while (s[i] != '\0') {
    s[i] = tolower(s[i]);
    i++;
  }
}

static inline void partial_fn(char *f)
{
  strtolower(f);

  if (fprintf(out_stream, "%s", f) < 0) { // Print the token out
    fatal_error("Error writing %s", out_stream_name);
  }
}

static inline void complete_fn(void)
{
  if (fputc('\n', out_stream) == EOF) {
    fatal_syserror("Error writing %s", out_stream_name);
  }
}

MAIN_BEGIN(
"tokenizer",
"If no optional file is present, standard input is read.\n"
"Otherwise, the optional file is read for input.\n"
"Then, the input is tokenized according to the delimiter list.\n"
"C control characters like \\n can be included in the delimiter list by\n"
"writing two hexadecimal digits preceded by a backslash like \\0a.\n"
"If the delimiter list is not given, it defaults to:\n"
", .-;:?!()[]\"'{}\\r\\n\\t\\v\n"
"If no output file is specified, the token list is output to stdout.\n"
"Otherwise, the token list is output to the specified file.\n"
"[NOTE]: The tokens are normalized to lowercase letters.\n",
"d:",
"[-d DELIMITER_LIST]",
0,
case 'd':
delimiter = optarg;
/* Converting C control characters */
int i = 0;
int j = 0;
char hexdigit[3] = {0};
while (delimiter[i] != '\0') {
  if (delimiter[i] == '\\') {
    strncpy(hexdigit, &delimiter[i + 1], 2);
    delimiter[j] = strtoul(hexdigit, NULL, 16);
    i += 3;
    j++;
    continue;
  }
  delimiter[j] = delimiter[i];
  i++;
  j++;
 }
delimiter[j] = '\0';
/* End of conversion */
break;
) {
  d = malloc(BUFFER_SIZE); // Allocate tokenizing buffer
  if (d == NULL) {
    fatal_error("Insufficient memory");
  }
}
MAIN_INPUT_START
{
  tokenizer(delimiter, d, BUFFER_SIZE, partial_fn, complete_fn);
}
MAIN_INPUT_END
MAIN_END
