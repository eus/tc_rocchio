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

#include <csignal>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "utility.h"

using namespace std;

static char *buffer = NULL;
static int server_sock = -1;
static const char *server_sock_name;
static char client_sock_name[10];
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
  if (server_sock != -1) {
    if (close(server_sock) != 0) {
      print_syserror("Cannot close server socket");
    }
    if (unlink(client_sock_name) != 0) {
      print_syserror("Cannot unlink client socket");
    }
  }
} CLEANUP_END

static unordered_map<string, double> w_list;
static string word;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  int pos = word.find(' ');

  if (pos == -1) {
    fatal_error("%s is a malformed input", word.c_str());
  } else {
    w_list[word.substr(0, pos)] = strtod(word.substr(pos + 1).c_str(), NULL);
  }

  word.clear();
}

MAIN_BEGIN(
"w",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have the following form:\n"
"WORD WORD_COUNT\\n\n"
"Logically, data should come from a TF processing unit in which\n"
"each TF processing unit produces a list of unique words in a document\n"
"The mandatory option -D specifies the name of the IDF Unix socket.\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+-----------------------------------------------------+\n"
"| Record count in unsigned int (4 bytes)              |\n"
"+-----------------------------+-----------------------+\n"
"| NULL-terminated unique word | w in double (8 bytes) |\n"
"+-----------------------------+-----------------------+\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n",
"D:",
"-D IDF_SERVER_UNIX_SOCKET_PATHNAME",
0,
case 'D':
server_sock_name = optarg;
break;
) {
  if (server_sock_name == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }

  /* Establishing contact with the server */
  server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (server_sock == -1) {
    fatal_syserror("Cannot open socket to contact server");
  }

  struct sockaddr_un client_addr;
  client_addr.sun_family = AF_UNIX;
  snprintf(client_sock_name, sizeof(client_sock_name), "%lu.socket",
	   static_cast<unsigned long>(getpid()));
  strcpy(client_addr.sun_path, client_sock_name);
  socklen_t client_addr_len = (sizeof(client_addr.sun_family)
			       + strlen(client_addr.sun_path));

  if (bind(server_sock, reinterpret_cast<struct sockaddr *>(&client_addr),
	   client_addr_len) != 0) {
    fatal_syserror("Cannot name client socket");
  }

  struct sockaddr_un server_addr;
  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, server_sock_name);
  socklen_t server_addr_len = (sizeof(server_addr.sun_family)
			       + strlen(server_addr.sun_path));
  if (connect(server_sock, reinterpret_cast<struct sockaddr *>(&server_addr),
	      server_addr_len) != 0) {
    fatal_syserror("Cannot connect to IDF server at %s", server_sock_name);
  }
  /* End of contact establishment */

  /* Allocating tokenizing buffer */
  buffer = static_cast<char *>(malloc(BUFFER_SIZE));
  if (buffer == NULL) {
    fatal_error("Insufficient memory");
  }
  /* End of allocation */
}
MAIN_INPUT_START
{
  tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
}
MAIN_INPUT_END
{
  double normalizer = 0;
  unordered_map<string, double> valid_w_list;
  for (unordered_map<string, double>::iterator i = w_list.begin();
       i != w_list.end();
       ++i) {
    const char *f = i->first.c_str();
    ssize_t f_len = i->first.length() + 1;
    ssize_t byte_sent;
    ssize_t byte_rcvd;

    strcpy(buffer, f);
    byte_sent = send(server_sock, f, f_len, 0);
    if (byte_sent == -1 || byte_sent != f_len) {
      fatal_syserror("Cannot send request properly");
    }

    struct idf_reply_packet packet;
    byte_rcvd = recv(server_sock, &packet, sizeof(packet), 0);
    if (byte_rcvd == -1 || byte_rcvd != sizeof(packet)) {
      fatal_syserror("Malformed reply");
    }

    if (!packet.entry_exists) {
      continue;
    } else {
      double tf_idf = i->second * packet.idf;
      valid_w_list[i->first] = tf_idf;
      normalizer += tf_idf * tf_idf;
    }
  }

  normalizer = sqrt(normalizer);

  unsigned int record_count = valid_w_list.size();
  size_t block_write = fwrite(&record_count, sizeof(record_count), 1,
			      out_stream);
  if (block_write == 0) {
    fatal_syserror("Cannot write record count to output stream");
  }
  for (unordered_map<string, double>::iterator i = valid_w_list.begin();
       i != valid_w_list.end();
       ++i) {
    i->second /= normalizer;

    block_write = fwrite(i->first.c_str(), i->first.length() + 1, 1,
				out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write word to output stream");
    }
    block_write = fwrite(&i->second, sizeof(double), 1, out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write weight to output stream");
    }
  }
} MAIN_END
