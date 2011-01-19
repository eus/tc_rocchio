#ifndef UTILITY_SERVER_SOCKET_H
#define UTILITY_SERVER_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "utility.h"
#include "utility_socket_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct server {
  int server_sock;
  char server_sock_name[32];
};

static int shutdown_server(struct server **s)
{
  int rc = 0;
  struct server *server = *s;

  if (server->server_sock != -1) {
    close_socket(server->server_sock, server->server_sock_name);
  }

  free(*s);
  *s = NULL;

  return rc;
}

static struct server *setup_server(const char *server_address,
				   int signal, void (*signal_handler)(int))
{
  int rc;
  struct server *s;
  struct sockaddr_un server_addr;
  socklen_t server_addr_len;

  /* Register signal handler */
  struct sigaction old_sigact;
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = signal_handler;
  if (sigaction(signal, &sigact, &old_sigact) != 0) {
    print_syserror("Cannot register signal handler");
    return NULL;
  }
  /* End of signal handler registration */

  s = (struct server *) malloc(sizeof(*s));
  if (s == NULL) {
    print_error("Insufficient memory");
    goto failed_1;
  }

  /* Create server socket */
  s->server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (s->server_sock == -1) {
    print_syserror("Cannot open server socket");
    goto failed_2;
  }
  /* End of server socket creation */

  /* Name the server socket */
  if (strlen(server_address) > 31) {
    print_error("Server address is too long");
    goto failed_3;
  }
  strcpy(s->server_sock_name, server_address);
  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, server_address);
  rc = unlink(server_addr.sun_path);
  if (rc != 0 && errno != ENOENT) {
    print_syserror("Cannot create server socket file");
    goto failed_3;
  }
  server_addr_len = (sizeof(server_addr.sun_family)
		     + strlen(server_addr.sun_path));
  if (bind(s->server_sock, (struct sockaddr *) &server_addr,
	   server_addr_len) != 0) {
    print_syserror("Cannot bind the server socket");
    goto failed_3;
  }
  /* End of naming */

  return s;

 failed_3:
  if (close_socket(s->server_sock, s->server_sock_name) != 0) {
    print_syserror("Cannot close failing socket");
  }
 failed_2:
  free(s);
 failed_1:
  if (sigaction(signal, &old_sigact, NULL) != 0) {
    print_syserror("Cannot recover original signal handler");
  }
  return NULL;
}

/**
 * For each received datagram, the callback function is called to process it.
 * The caller must supply a dynamically allocated memory area to receive any
 * incoming datagram. If the size of the datagram is equal to or greater than
 * the provided buffer, the dynamically allocated memory will be replaced with
 * a larger one. This function will not return unless the callback function
 * returns non-zero value or there is a failure in receiving a datagram.
 *
 * @param buffer the initial buffer to store any received datagram and the
 * final buffer if enlargement is performed
 * @param buffer_size the initial size of the buffer and the final size of the
 * buffer
 * @param callback_fn the callback function that will process any incoming
 * datagram
 *
 * @return zero if there is no failure or non-zero otherwise.
 */
static int start_serving(struct server *s, void **buffer, int *buffer_size,
			 int (*callback_fn)(int server_sock,
					    ssize_t byte_received,
					    struct sockaddr *client_addr,
					    socklen_t client_addr_len))
{
  while (1) {
    struct sockaddr_un client_addr;
    socklen_t addr_len;
    ssize_t byte_rcvd;
    
    while (1) {
      addr_len = sizeof(client_addr);
      byte_rcvd = recvfrom(s->server_sock, *buffer, *buffer_size,
			   MSG_PEEK,
			   (struct sockaddr *) &client_addr,
			   &addr_len);
      if (byte_rcvd == -1) {
	print_syserror("Cannot get client datagram");
	return -2;
      } else if (byte_rcvd == *buffer_size) {
	*buffer_size += BUFFER_SIZE;
	void *enlarged_buffer = realloc(*buffer, *buffer_size);
	if (enlarged_buffer == NULL) {
	  print_error("Insufficient memory to process datagram");
	  return -1;
	}
	*buffer = enlarged_buffer;
	continue;
      } else {
	break;
      }
    }

    addr_len = sizeof(client_addr);
    byte_rcvd = recvfrom(s->server_sock, *buffer, *buffer_size,
			 0,
			 (struct sockaddr *) &client_addr,
			 &addr_len);
    if (byte_rcvd == -1) {
      print_syserror("Cannot get client datagram");
      return -2;
    }

    if (callback_fn(s->server_sock, byte_rcvd,
		    (struct sockaddr *) &client_addr, addr_len) != 0) {
      return 0;
    }
  }
}

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_SERVER_SOCKET_H */
