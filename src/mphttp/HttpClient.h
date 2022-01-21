#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "Buffer.h"
#include "HttpResponse.h"
#include "common.h"
#include "eventloop.h"
#include "utils.h"

// forward declaration
struct Buffer;
struct EventLoop;
struct MpHttpClient;
struct HttpResponse;
struct MpTask;
struct Request;
struct Body;

// state machine for onReadEvent
// the state for reading response
enum ReadableState {
  kInitial = 0,
  kReadingResponseLine,
  kReadingHeaders,
  kGetAllHeaders,
  kReadingBody,
};

// state machine for onWriteEvent
// the state for sending request
// on writing request, you only need to write to the buffer.
enum WritableState {
  kConnecting = 0,
  kWritingRequest,
  kWritingComplete,
};

struct HttpClient {
  HttpClient(EventLoop *loop, MpHttpClient *mp, MpTask *task_buffer,
             struct sockaddr_in addr_in, size_t start, size_t end)
      : loop_(loop), mp_(mp), task_buffer_(task_buffer), server_addr_(addr_in),
        range({start, end, 0}) {

    MPHTTP_ASSERT((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1,
                  "HttpClient : set up new socket fail\n");

    MPHTTP_ASSERT(setNonBlocking(sock_fd) != -1,
                  "HttpClient : socket %d set nonblock fail\n", sock_fd);

    int ret = connect(sock_fd, (const sockaddr *)&server_addr_,
                          sizeof(server_addr_));
    if (ret < 0) {
      if (errno != EINPROGRESS) {
        MPHTTP_FATAL("HttpClient : socket %d non blocking connect fail\n", sock_fd);
      }
    }

    MPHTTP_ASSERT(loop_->registerEvent(sock_fd, EPOLLIN | EPOLLOUT, this) != -1,
                  "HttpClient : socket %d add into epoll fail\n", sock_fd);

    constructRangeHeader(start, end);
  }

  void constructRangeHeader(size_t start, size_t end);

  void sendRequest();

  bool processResponseLine(const char *begin, const char *end);

  bool parseResponse(timestamp_t recv_time);

  void onGetAllHeaders();

  void onBody(timestamp_t recv_time);

  void onReadable(timestamp_t received_time,
                  bool is_end_of_stream);

  // @brief : set the Range.end
  void ResetRangeEnd(size_t end) {
    MPHTTP_ASSERT(
        end <= range.end,
        "HttpClient.ResetRangeEnd() : reset client end with %zu, which "
        "is longer than %zu\n",
        end, range.end);
    MPHTTP_ASSERT(
        range.start <= end,
        "HttpClient.ResetRangeEnd() : reset client end with %zu, which "
        "is less than %zu\n",
        end, range.start);
    range.end = end;
  }

  // @brief : get the remaining bytes for this client
  size_t GetRemaining() const {
    if (range.end == 0 || range.end < range.start + range.received) {
      return 0;
    } else {
      return range.end - range.start - range.received + 1;
    }
  }

  // @brief : get the bandwidth of this client
  size_t GetBandwidth() const { return bandwidth; }

  // @brief : update the bandwidth with sample (recv_bytes, recv_time)
  void UpdateBandwidth(size_t recv_bytes, timestamp_t recv_time) {

    if (range.received == 0) {
      last_received = recv_time;
      delta_received = recv_bytes;
      return;
    }

    delta_received += recv_bytes;
    size_t time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                            recv_time - last_received)
                            .count();

    if (time_delta == 0) {
      return;
    }

    size_t delta_sample = delta_received / time_delta; // in KB/s

    delta_received = 0;
    last_received = recv_time;

    if (bandwidth == 0) {
      bandwidth = delta_sample;
    } else {
      bandwidth = 0.9 * bandwidth + 0.1 * delta_sample;
    }
  }

  // @brief : the readable callback for the event loop to execute
  void ReadableCallback(timestamp_t now);
  // @brief : the writable callback for the event loop to execute
  void WritableCallback(timestamp_t now);
  // @brief : the close callback for MpHttpClient to close me (:
  void CloseCallback();

  // server address
  struct sockaddr_in server_addr_;
  // request time
  timestamp_t send_time_;

  // http response for this line
  struct HttpResponse response_;

  // this client is close or not
  bool is_close{false};
  // this client has reschedule or not
  bool has_reschedule{false};

  // the task buffer
  struct MpTask *task_buffer_;
  // input buffer for http client
  Buffer input_buffer;
  // output buffer for http client
  Buffer output_buffer;
  // task of the HttpClient
  struct Range {
    size_t start{0};
    size_t end{0};
    size_t received{0};
  };
  struct Range range;

  // bandwidth sampler
  size_t delta_received{0};
  timestamp_t last_received;
  size_t bandwidth{0};

  int sock_fd;       // my socket fd
  EventLoop *loop_;  // my event loop
  MpHttpClient *mp_; // which MpHttpClient it belongs to
  enum ReadableState read_state { kInitial };     // my reading state
  enum WritableState write_state { kConnecting }; // my writing state
};

#endif