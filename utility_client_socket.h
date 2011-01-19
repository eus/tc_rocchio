#ifndef UTILITY_CLIENT_SOCKET_H
#define UTILITY_CLIENT_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "utility.h"
#include "utility_socket_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct client {
  int client_sock;
  char client_sock_name[32];
};

static int set_timeout(int socket, unsigned us_timeout)
{
  struct timeval timeout;

  timeout.tv_sec = 0;
  timeout.tv_usec = us_timeout;

  if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
		 sizeof (timeout)) != 0) {
    print_syserror("Cannot set timeout on socket");
    return -1;
  }

  return 0;
}

static int disconnect_server(struct client **c)
{
  int rc = 0;
  struct client *client = *c;

  if (client->client_sock != -1) {
    close_socket(client->client_sock, client->client_sock_name);
  }

  free (*c);
  *c = NULL;

  return rc;
}

static struct client *connect_to_server(const char *server_address)
{
  struct client *c;
  struct sockaddr_un client_addr;
  socklen_t client_addr_len;
  struct sockaddr_un server_addr;
  socklen_t server_addr_len;

  c = (struct client *) malloc(sizeof(*c));
  if (c == NULL) {
    print_error("Insufficient memory");
    return NULL;
  }

  /* Create client socket */
  c->client_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (c->client_sock == -1) {
    print_syserror("Cannot open socket to contact server");
    goto failed_1;
  }
  /* End of client socket creation */

  /* Name client socket to permit the server to reply */
  client_addr.sun_family = AF_UNIX;
  snprintf(c->client_sock_name, sizeof(c->client_sock_name), "%lu.socket",
	   (unsigned long) getpid());
  strcpy(client_addr.sun_path, c->client_sock_name);
  client_addr_len = (sizeof(client_addr.sun_family)
		     + strlen(client_addr.sun_path));

  if (bind(c->client_sock, (struct sockaddr *) &client_addr,
	   client_addr_len) != 0) {
    print_syserror("Cannot name client socket");
    goto failed_2;
  }
  /* End of naming */

  /* Connect to IDF server */
  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, server_address);
  server_addr_len = (sizeof(server_addr.sun_family)
		     + strlen(server_addr.sun_path));
  if (connect(c->client_sock, (struct sockaddr *) &server_addr,
	      server_addr_len) != 0) {
    print_syserror("Cannot connect to IDF server");
    goto failed_2;
  }
  /* End of connecting */

  /* Set receiving timeout */
  srand(time(NULL));
  if (set_timeout(c->client_sock, RCV_TIMEOUT_MIN + rand() % RCV_TIMEOUT_MAX)) {
    goto failed_2;
  }
  /* End of setting timeout */
   
  return c;

 failed_2:  
  if (close_socket(c->client_sock, c->client_sock_name) != 0) {
    print_syserror("Cannot close failing socket");
  }
 failed_1:
  free(c);
  return NULL;
}


#ifdef __cplusplus
}
#endif

#endif /* UTILITY_CLIENT_SOCKET_H */
