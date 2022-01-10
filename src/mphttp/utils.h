#ifndef UTILS_H
#define UTILS_H

#include <fcntl.h>
#include <sys/stat.h>

//@brief : set the socket to be non blocking
int setNonBlocking(int fd);

//@brief : read from a non blocking socket

//@brief : write to an non blocking socket until EAGAIN

#endif // UTILS_H