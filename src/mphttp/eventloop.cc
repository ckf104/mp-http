#include "eventloop.h"
#include "HttpClient.h"

#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

void EventLoop::run(int timeout) {
  struct epoll_event event_queue[kMaxSize];

  int num_events = epoll_wait(epoll_fd, event_queue, kMaxSize, timeout);

  timestamp_t now = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_events; i++) {

    struct HttpClient *client =
        static_cast<struct HttpClient *>(event_queue[i].data.ptr);

    if (event_queue[i].events & EPOLLIN) {
      client->ReadableCallback(now);
    }

    if (event_queue[i].events & EPOLLOUT) {
      client->WritableCallback(now);
    }
  }

  // execute some callback functions
  while (!pending_queue.empty()) {
    auto callback = std::move(pending_queue.front());
    callback();
    pending_queue.pop();
  }
}

int EventLoop::registerEvent(int sock_fd, int events,
                             struct HttpClient *pointer) {
  struct epoll_event event;
  event.events = events;
  event.data.ptr = pointer;

  int ret;
  if ((ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event)) == -1) {
    std::cout << "eventloop : register event fail" << strerror(errno)
              << std::endl;
    return -1;
  }
  return 0;
}

int EventLoop::modifyEvent(int sock_fd, int method, int events) {
  MPHTTP_ASSERT(0, "EventLoop.modify() : not implemented\n");
  return 0;
}

void EventLoop::enqueueCallback(std::function<void()> callback) {
  pending_queue.push(std::move(callback));
}
