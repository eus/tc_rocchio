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

#include <list>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include "utility.h"
#include "utility_server_socket.h"

using namespace std;

static char *buffer = NULL;
static struct server *server = NULL;
CLEANUP_BEGIN
{
  if (buffer != NULL) {
    free(buffer);
  }
  if (server != NULL) {
    shutdown_server(&server);
  }
} CLEANUP_END

static unordered_map<string, pair<unsigned int, double>> idf_list;
static string word;
static list<string> word_sorter;

static inline void partial_fn(char *f)
{
  word.append(f);
}

static inline void complete_fn(void)
{
  int pos = word.find(' ');

  if (pos != -1) {
    word = word.substr(0, pos);
  }

  idf_list[word].second += 1;
  word_sorter.push_back(word);

  word.clear();
}

static void sigint_handler(int ignored)
{
  exit(EXIT_SUCCESS);
}

static int server_fn(int server_sock, ssize_t byte_rcvd,
		     struct sockaddr *client_addr, socklen_t addr_len)
{
  static unsigned int dic_size = idf_list.size();
  ssize_t byte_sent;
  ssize_t packet_size;

  buffer[byte_rcvd] = '\0'; // Prevents buffer overflow

  /* Construct reply */
  word = buffer;
  if (word.length() == 1 && word.compare(" ") == 0) { // Dic size inquiry
    packet_size = sizeof(dic_size);
    memcpy(buffer, &dic_size, packet_size);
  } else { // IDF inquiry
    unordered_map<string, pair<unsigned int, double>>::const_iterator i
      = idf_list.find(word);
    struct idf_reply_packet packet;
    packet_size = sizeof(packet);

    if (i == idf_list.end()) {
      memset(buffer, 0, packet_size);
    } else {
      packet.entry_exists = 1;
      packet.pos = i->second.first;
      packet.idf = i->second.second;
      memcpy(buffer, &packet, packet_size);
    }
  }
  /* End of construction */

  /* Send reply out */
  byte_sent = sendto(server_sock, buffer, packet_size,
		     0,
		     reinterpret_cast<struct sockaddr *>(client_addr),
		     addr_len);
  if (byte_sent == -1) {
    print_syserror("Cannot send reply");
  } else if (byte_sent != packet_size) {
    print_error("Reply only sent partially: %d out of %d bytes",
		byte_sent, packet_size);
  }
  /* End of sending */

  return 0;
}

static unsigned long M = 0;
static int server_only = 0;
static int no_server = 0;
MAIN_BEGIN(
"idf_dic",
"If input file is not given, stdin is read for input.\n"
"Otherwise, the input file(s) is read for input.\n"
"Then, if -s (i.e., server-only mode) is not given, the input stream is\n"
"expected to have the following form:\n"
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
"+----------------------------------------------------------------+\n"
"| Record count N in unsigned int (4 bytes)                       |\n"
"+--------------------------------------+-------------------------+\n"
"| NULL-terminated sorted unique word 1 | IDF in double (8 bytes) |\n"
"+--------------------------------------+-------------------------+\n"
"|                                ...                             |\n"
"+--------------------------------------+-------------------------+\n"
"| NULL-terminated sorted unique word N | IDF in double (8 bytes) |\n"
"+--------------------------------------+-------------------------+\n"
"The result is output to the given file if an output file is specified.\n"
"Otherwise, no output is produced.\n"
"If -s (i.e., server-only mode) is given, -M and -o are ignored and the input\n"
"stream is expected to be in the format of the output above and only either\n"
"stdin or a single input file is considered.\n"
"At the end, if -n (i.e., no-server mode) is not given, this processing\n"
"unit will become an online server serving request using Unix datagram\n"
"socket. The socket will be located in the current working directory and is\n"
"named `/idf_dic.socket'.\n"
"The IDF or the position in the w vector of a word can be inquired by\n"
"sending a datagram with the following structure:\n"
"+------------------------+\n"
"| NULL-terminated string |\n"
"+------------------------+\n"
"The reply will be a datagram with the following structure whose endianness\n"
"follows that of the host machine:\n"
"+----------------------------------------------------+\n"
"| Word exists or not in unsigned int (4 bytes)       |\n"
"+----------------------------------------------------+\n"
"| Position in the w vector in unsigned int (4 bytes) |\n"
"+----------------------------------------------------+\n"
"| IDF of the inquired word in double (8 bytes)       |\n"
"+----------------------------------------------------+\n"
"The number of words in the dictionary can be inquired by sending a datagram\n"
"with the following structure:\n"
"+-----------------------------------------------------------------+\n"
"| NULL-terminated string containing just a single space character |\n"
"+-----------------------------------------------------------------+\n"
"The reply will be a datagram with the following structure whose endianness\n"
"follows that of the host machine:\n"
"+-----------------------------------------------------------------+\n"
"| The number of words in the dictionary in unsigned int (4 bytes) |\n"
"+-----------------------------------------------------------------+\n"
"The online server can be terminated by sending SIGINT signal.\n",
"M:sn",
"(-M TOTAL_DOCUMENT_COUNT [-n] | -s)",
1,
case 'M':
M = strtoul(optarg, NULL, 10);
break;
case 's':
server_only = 1;
break;
case 'n':
no_server = 1;
break;
) {
  if (server_only) { // Only consider one input file
    multiple_input = 0;
    if (no_server) {
      fatal_error("-n cannot be used together with -s (-h for help)");
    }
  } else {
    if (M == 0) { // Check that M is greater than 0
      fatal_error("%lu is invalid total document count (-h for help)", M);
    }
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
  if (server_only) {
    size_t block_read;

    unsigned int i = 0;
    unsigned int count;
    block_read = fread(&count, sizeof(count), 1, in_stream);
    if (block_read == 0) {
      fatal_error("Malformed input: cannot read record count");
    }
    if (count == 0) { // No record
      exit(EXIT_SUCCESS);
    }
    unsigned int delta = 0;
    double idf;
    int idf_not_truncated = 1;
    while (count > 0) {
      block_read = fread(buffer, 1, BUFFER_SIZE - 1, in_stream);
      if (block_read == 0) {
	  fatal_error("Malformed input: record count is less than stated");
      }
      buffer[BUFFER_SIZE - 1] = '\0'; // Prevent buffer overflow
      unsigned int offset = 0;
      while (count > 0) {
	if (idf_not_truncated) {
	  word.append(buffer + offset);
	  offset += strlen(buffer + offset);
	  if (offset == block_read) { // Truncated word
	    break;
	  }

	  offset++;

	  if (offset + sizeof(idf) > block_read) { // Truncated IDF
	    delta = block_read - offset;
	    memcpy(&idf, buffer + offset, delta);
	    idf_not_truncated = 0;
	    break;
	  }

	  memcpy(&idf, buffer + offset, sizeof(idf));
	  offset += sizeof(idf);
	} else {
	  if (block_read + delta < sizeof(idf)) { // IDF is still truncated
	    memcpy(reinterpret_cast<char *>(&idf) + delta, buffer, block_read);
	    delta += block_read;
	    break;
	  } else { // IDF can be completed now
	    offset = sizeof(idf) - delta;
	    memcpy(reinterpret_cast<char *>(&idf) + delta, buffer, offset);
	    idf_not_truncated = 1;
	  }
	}

	pair<unsigned int, double> &data = idf_list[word];
	data.first = i;
	data.second = idf;

	i++;
	count--;
	word.clear();
      }
    }
  } else {
    tokenizer("\n", buffer, BUFFER_SIZE, partial_fn, complete_fn);
  }
}
MAIN_INPUT_END
{
  if (!server_only) {
    /* If requested to write an output file, output the header */
    size_t block_write;
    if (out_stream != stdout) {
      unsigned int count = idf_list.size();
      block_write = fwrite(&count, sizeof(count), 1, out_stream);
      if (block_write == 0) {
	fatal_syserror("Cannot write to output stream");
      }
    }
    /* End of outputting header */

    /* Calculating IDF and the word position in the vector and outputing to
     * a file if requested
     */
    word_sorter.sort();
    word_sorter.unique();

    unsigned int vec_at = 0;
    for (list<string>::const_iterator i = word_sorter.begin();
	 i != word_sorter.end();
	 ++i, ++vec_at) {

      pair<unsigned int, double> &data = idf_list[*i];
      data.first = vec_at;
      data.second = log10(static_cast<double>(M) / data.second);

      if (out_stream != stdout) {
	block_write = fwrite(i->c_str(), i->length() + 1, 1, out_stream);
	if (block_write == 0) {
	  fatal_syserror("Cannot write word to output stream");
	}
	block_write = fwrite(&data.second, sizeof(double), 1, out_stream);
	if (block_write == 0) {
	  fatal_syserror("Cannot write IDF to output stream");
	}
      }
    }
    /* End of IDF and word position in the vector calculations */

    if (out_stream != stdout) { // Close output stream
      if (fclose(out_stream) != 0) {
	fatal_syserror("Cannot close output %s", out_stream_name);
      }
      out_stream = stdout;
    }
  }

  if (no_server) {
    exit(EXIT_SUCCESS);
  }

  /* Setup server */
  server = setup_server("idf_dic.socket", SIGINT, sigint_handler);
  if (server == NULL) {
    fatal_error("Cannot setup server");
  }
  /* End of server setup */

  /* Serving until SIGINT */
  int buffer_size = BUFFER_SIZE;
  if (start_serving(server, reinterpret_cast<void **>(&buffer), &buffer_size,
		    server_fn) != 0) {
    fatal_error("Server malfunction");
  }
} MAIN_END
