#ifndef UTILITY_SOCKET_COMMON_H
#define UTILITY_SOCKET_COMMON_H

#include <errno.h>
#include <unistd.h>
#include "utility.h"

#ifdef __cplusplus
extern "C" {
#endif

static int close_socket(int socket, const char *socket_name)
{
  int rc = 0;

  if (close(socket) != 0) {
    print_syserror("Cannot close socket");
    rc = -1;
  }
  if (unlink(socket_name) != 0 && errno != ENOENT) {
    print_syserror("Cannot unlink socket");
    rc = -1;
  }

  return rc;
}

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_SOCKET_COMMON_H */
