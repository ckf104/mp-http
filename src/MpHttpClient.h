#ifndef MPHTTPCLIENT_H
#define MPHTTPCLIENT_H

#include "common.h"
#include <queue>

// forward declaration
struct EventLoop;
struct HttpClient;

struct MpHttpClient {
  MpHttpClient(struct EventLoop *loop)
      : loop_(loop), tasks_({nullptr, nullptr}) {}

  // the main event loop
  struct EventLoop *loop_;
  // the task queue of MpHttpClient
  struct Tasks {
    HttpClient *running;
    HttpClient *pending;
  };
  struct Tasks tasks_;
  // the sampled rtt (in ms)
  size_t sample_rtt_{0};
};

#endif // ! MPHTTPCLIENT_H