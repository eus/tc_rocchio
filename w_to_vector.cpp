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
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "utility.h"
#include "utility_client_socket.h"

using namespace std;

static char *buffer = NULL;
static struct client *client = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
  if (client != NULL) {
    disconnect_server(&client);
  }
} CLEANUP_END

static list<pair<string, double>> w_list;
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
    w_list.push_back(pair<string, double>
		     (word.substr(0, pos),
		      strtod(word.substr(pos + 1).c_str(), NULL)));
  }

  word.clear();
}

enum inquiry_about {
  DIC_SIZE,
  IDF
};
static void inquire_idf_server(enum inquiry_about type,
			       const char *f, ssize_t f_len)
{
  ssize_t byte_sent;

  switch (type) {
  case DIC_SIZE:
    strcpy(buffer, " ");
    f_len = 1;
    break;
  case IDF:
    strcpy(buffer, f);
    break;
  default:
    fatal_error("programming error: unknown inquire type");
  }

  byte_sent = send(client->client_sock, buffer, f_len, 0);
  if (byte_sent == -1) {
    fatal_syserror("Cannot send request");
  } else if (byte_sent != f_len) {
    fatal_error("Reply only sent partially: %d out of %d bytes",
		byte_sent, f_len);
  }
}

static void listen_to_idf_server(void *buffer, ssize_t buffer_size)
{
  ssize_t byte_rcvd;

  while (1) {
    byte_rcvd = recv(client->client_sock, buffer, buffer_size, 0);
    if (byte_rcvd == -1) {
      if (errno == EAGAIN) {
	continue;
      }
      fatal_syserror("Failure in listening to IDF server");
    } else if (byte_rcvd != buffer_size) {
      fatal_error("Malformed reply");
    }
    break;
  }
}

static const char *server_addr;
MAIN_BEGIN(
"w_to_vector",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file is read for input.\n"
"Then, the input stream is expected to have the following form:\n"
"WORD WORD_COUNT\\n\n"
"Logically, data should come from a TF processing unit in which\n"
"each TF processing unit produces a list of unique words in a document\n"
"The mandatory option -D specifies the name of the IDF_DIC Unix socket.\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+-------------------------+\n"
"| w_1 in double (8 bytes) |\n"
"+-------------------------+\n"
"|           ...           |\n"
"+-------------------------+\n"
"| w_N in double (8 bytes) |\n"
"+-------------------------+\n"
"where N is the number of words in the dictionary as returned by\n"
"IDF_DIC server.\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, stdout is used to output binary data.\n",
"D:",
"-D IDF_DIC_SERVER_UNIX_SOCKET_PATHNAME",
0,
case 'D':
server_addr = optarg;
break;
) {
  if (server_addr == NULL) {
    fatal_error("-D must be specified (-h for help)");
  }

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
  client = connect_to_server(server_addr);
  if (client == NULL) {
    fatal_error("Cannot connect to IDF server at %s", server_addr);
  }

  double normalizer = 0;
  list<pair<unsigned int, double>> valid_w_list;
  for (list<pair<string, double>>::iterator i = w_list.begin();
       i != w_list.end();
       ++i) {

    inquire_idf_server(IDF, i->first.c_str(), i->first.length() + 1);

    struct idf_reply_packet packet;
    listen_to_idf_server(&packet, sizeof(packet));

    if (!packet.entry_exists) {
      continue;
    } else { // Calculation of a feature's weight
      unsigned int pos = packet.pos;
      double tf_idf = i->second * packet.idf;

      valid_w_list.push_back(pair<unsigned int, double>(pos, tf_idf));
      normalizer += tf_idf * tf_idf;
    }
  }
  normalizer = sqrt(normalizer);

  valid_w_list.sort();

  inquire_idf_server(DIC_SIZE, NULL, 0);
  unsigned int dic_size;
  listen_to_idf_server(&dic_size, sizeof(dic_size));

  list<pair<unsigned int, double>>::iterator j = valid_w_list.begin();
  for (unsigned int i = 0; i < dic_size; i++) {
    double weight = 0.0;

    if (i == j->first) { // This word exists in the input stream
      weight = j->second / normalizer;

      j++;
    }

    if (fwrite(&weight, sizeof(weight), 1, out_stream) == 0) {
      fatal_syserror("Cannot write weight to output stream");
    }
  }
} MAIN_END
