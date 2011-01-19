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
static const char *server_sock_name = "idf.socket";
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
  if (server_sock != -1) {
    if (close(server_sock) != 0) {
      print_syserror("Cannot close server socket");
    }
    if (unlink(server_sock_name) != 0) {
      print_syserror("Cannot unlink server socket");
    }
  }
} CLEANUP_END

static unordered_map<string, double> idf_list;
static string word;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  int pos = word.find(' ');

  if (pos == -1) {
    idf_list[word] += 1;
  } else {
    idf_list[word.substr(0, pos)] += 1;
  }

  word.clear();
}

static void sigint_handler(int ignored)
{
  exit(EXIT_SUCCESS);
}

static unsigned long M = 0;
MAIN_BEGIN(
"idf",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file(s) is read for input.\n"
"Then, the input stream is expected to have the following form:\n"
"WORD( FIELD)*\\n\n"
"in which only WORD (i.e., the first field separated by space) matters.\n"
"Logically, data should come from one or more TF processing unit in which\n"
"each TF processing unit produces a list of unique words so that the number\n"
"of duplicates of a word in the input stream can be taken as the number of\n"
"documents having that particular word.\n"
"The mandatory option -M specifies a positive non-zero integer specifying\n"
"the total number of documents, or equivalenty, the total number of connected\n"
"TF processing units. This is needed to calculate the inverse document\n"
"frequency (IDF).\n"
"Finally, the result will be in the following binary format whose endianness\n"
"follows that of the host machine:\n"
"+---------------------------------------------------------+\n"
"| Record count N in unsigned int (4 bytes)                |\n"
"+-------------------------------+-------------------------+\n"
"| NULL-terminated unique word 1 | IDF in double (8 bytes) |\n"
"+-------------------------------+-------------------------+\n"
"|                            ...                          |\n"
"+-------------------------------+-------------------------+\n"
"| NULL-terminated unique word N | IDF in double (8 bytes) |\n"
"+-------------------------------+-------------------------+\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, no output is produced.\n"
"In both cases, this processing unit will become an online server serving\n"
"request using Unix datagram socket. The socket will be located in the\n"
"current working directory and is named `/idf.socket'.\n"
"The IDF of a word can be inquired by sending a datagram with the following\n"
"structure:\n"
"+------------------------+\n"
"| NULL-terminated string |\n"
"+------------------------+\n"
"The reply will be a datagram with the following structure:\n"
"+-------------------------------------------------------------------+\n"
"| IDF exists or not in unsigned int (4 bytes)                       |\n"
"+-------------------------------------------------------------------+\n"
"| IDF of the inquired word in double (8 bytes) in host's endianness |\n"
"+-------------------------------------------------------------------+\n"
"If the word is not in the IDF database, the double value will be NaN.\n"
"The online server can be terminated by sending SIGINT signal.\n",
"M:",
"-M TOTAL_DOCUMENT_COUNT",
1,
case 'M':
M = strtoul(optarg, NULL, 10);
break;
) {
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = sigint_handler;
  if (sigaction(SIGINT, &sigact, NULL) != 0) { // Register SIGINT handler
    fatal_syserror("Cannot register SIGINT handler");
  }
  
  /* End of SIGINT handler registration */

  if (M == 0) { // Check that M is greater than 0
    fatal_error("%lu is invalid total document count (-h for help)", M);
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
  /* If requested to write an output file, output the header */
  size_t block_write;
  unsigned int count = idf_list.size();
  if (out_stream != stdout) {
    block_write = fwrite(&count, sizeof(count), 1, out_stream);
    if (block_write == 0) {
      fatal_syserror("Cannot write to output stream");
    }
  }
  /* End of outputting header */

  /* Calculating IDF and outputing to a file if requested */
  for (unordered_map<string, double>::iterator i = idf_list.begin();
       i != idf_list.end();
       ++i) {

    i->second = log10(static_cast<double>(M) / i->second);

    if (out_stream != stdout) {
      block_write = fwrite(i->first.c_str(), i->first.length() + 1, 1,
			   out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write to output stream");
      }
      block_write = fwrite(&i->second, sizeof(double), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write to output stream");
      }
    }
  }
  /* End of IDF calculation */

  if (out_stream != stdout) { // Close output stream
    if (fclose(out_stream) != 0) {
      fatal_syserror("Cannot close output %s", out_stream_name);
    }
    out_stream = stdout;
  }

  /* Becoming a server */
  server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (server_sock == -1) {
    fatal_syserror("Cannot open server socket");
  }

  struct sockaddr_un server_addr;
  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, server_sock_name);
  unlink(server_addr.sun_path);
  socklen_t server_addr_len = (sizeof(server_addr.sun_family)
			       + strlen(server_addr.sun_path));

  if (bind(server_sock, reinterpret_cast<struct sockaddr *>(&server_addr),
	   server_addr_len) != 0) {
    fatal_syserror("Cannot bind the server socket");
  }
  /* End of becoming a server */

  /* Serving until SIGINT */
  int buffer_size = BUFFER_SIZE;
  while (1) {
    struct sockaddr_un client_addr;
    socklen_t addr_len;
    ssize_t byte_rcvd;
    ssize_t byte_sent;
    
    while (1) {
      addr_len = sizeof(client_addr);
      byte_rcvd = recvfrom(server_sock, buffer, buffer_size,
			   MSG_PEEK,
			   reinterpret_cast<struct sockaddr *>(&client_addr),
			   &addr_len);
      if (byte_rcvd == -1) {
	fatal_error("Cannot get client datagram");
      } else if (byte_rcvd == buffer_size) {
	buffer_size += BUFFER_SIZE;
	void *enlarged_buffer = realloc(buffer, buffer_size);
	if (enlarged_buffer == NULL) {
	  fatal_error("Insufficient memory to process datagram");
	}
	buffer = static_cast<char *>(enlarged_buffer);
	continue;
      } else {
	break;
      }
    }

    addr_len = sizeof(client_addr);
    byte_rcvd = recvfrom(server_sock, buffer, buffer_size,
			 0,
			 reinterpret_cast<struct sockaddr *>(&client_addr),
			 &addr_len);
    if (byte_rcvd == -1) {
      fatal_error("Cannot get client datagram");
    }

    buffer[buffer_size - 1] = '\0';
    word = buffer;
    unordered_map<string, double>::const_iterator i = idf_list.find(word);
    struct idf_reply_packet packet;

    if (i == idf_list.end()) {
      memset(buffer, 0, sizeof(packet));
    } else {
      packet.entry_exists = 1;
      packet.idf = i->second;
      memcpy(buffer, &packet, sizeof(packet));
    }
    byte_sent = sendto(server_sock, buffer, sizeof(packet),
		       0,
		       reinterpret_cast<struct sockaddr *>(&client_addr),
		       addr_len);
    if (byte_sent == -1 || byte_sent != sizeof(packet)) {
      print_syserror("Cannot send reply properly");
    }
  }
} MAIN_END
