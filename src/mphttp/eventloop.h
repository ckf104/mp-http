#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "types.h"
#include "macro.h"

#include <functional>
#include <queue>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>

struct HttpClient;
struct EventLoop {
  EventLoop() { 
    epoll_fd = epoll_create(1); 
    MPHTTP_ASSERT(epoll_fd != -1, "EventLoop : create epoll fd fail");
  }
  ~EventLoop() {  close(epoll_fd);  }



  // @brief : main loop execution function
  void run(int timeout);

  // @brief : register an socket fd into the epoll_fd
  int registerEvent(int sock_fd, int events, struct HttpClient *pointer);

  // @brief : modify the event of an registered socket fd
  int modifyEvent(int sock_fd, int method, int events);

  // @brief : append a callback function to execute later
  void enqueueCallback(std::function<void()> callback);

  int epoll_fd;
  const int kMaxSize = 20;
  std::queue<std::function<void()>> pending_queue;
};

#endif //! EVENTLOOP_H