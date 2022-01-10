#include "utils.h"

//@brief : set the socket to be non blocking
int setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}