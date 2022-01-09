#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "Buffer.h"
#include "common.h"
#include "eventloop.h"
#include "utils.h"

// forward declaration
struct Buffer;
struct EventLoop;
struct MpHttpClient;

// state machine for onReadEvent
// the state for reading response
enum ReadableState {
  kInitial,
  kReadingHeaders,
  kGetAllHeaders,
  kReadingBody,
};

// state machine for onWriteEvent
// the state for sending request
// on writing request, you only need to write to the buffer.
enum WritableState {
  kConnecting,
  kWritingRequest,
  kWritingComplete,
};

struct HttpClient {
  // TODO : parent MpHttpClient
  HttpClient(EventLoop *loop, MpHttpClient *mp, int fd, size_t start,
             size_t end)
      : loop_(loop), mp_(mp), sock_fd(fd), range({start, end, 0}) {

    MPHTTP_ASSERT(setNonBlocking(sock_fd) != -1,
                  "HttpClient : socket %d set nonblock fail\n", sock_fd);

    MPHTTP_ASSERT(loop_->registerEvent(sock_fd, EPOLLIN | EPOLLOUT, this) != -1,
                  "HttpClient : socket %d add into epoll fail\n", sock_fd);
  }

  void onReadable(size_t recv_bytes, timestamp_t received_time,
                  bool is_end_of_stream) {
    if (read_state == kInitial) {
      // TODO : see if an range header in the Request Header
      // if not, add one ...
      read_state = kReadingHeaders;
      return;
    }
    if (read_state == kReadingHeaders) {
      // TODO : an inline function to parse the HttpResponse
      // if the buffer reaches \r\n\r\n, then switch to kGetAllHeaders
      read_state = kGetAllHeaders;
    }
    if (read_state == kGetAllHeaders) {
      if (range.end == 0) {
        // TODO : parse all headers to find out the content-range
        // mp->onInitGetFileSize();

        // TODO : prepend header for the task
      }
      read_state = kReadingBody;
    }
    if (read_state == kReadingBody) {
      // TODO : onBody()
      // onBody(buffer, recv_bytes, recv_time);
    }
  }

  void onWritable() {
    if (write_state == kConnecting) {
      // connection established
      write_state = kWritingRequest;
      send_time = std::chrono::high_resolution_clock::now();
      // TODO : drain the HttpRequest
    }
    if (write_state == kWritingRequest) {
      // TODO : drain the output buffer
      // if the output buffer is empty, then the writing is complete.
    }
  }

  // @brief : set the Range.end
  void ResetRangeEnd(size_t end) {
    // FIXME : assert that end >= new_end
    range.end = end;
  }

  // @brief : get the remaining bytes for this client
  size_t GetRemaining() const {
    if (range.end == 0 || range.end <= range.start + range.received) {
      return 0;
    } else {
      return range.end - range.start - range.received;
    }
  }

  // @brief : get the bandwidth of this client
  size_t GetBandwidth() const {
    if (bandwidth == 0) {
      return 128; // just an arbitrary number (:
    } else {
      return bandwidth;
    }
  }

  // @brief : update the bandwidth with sample (recv_bytes, recv_time)
  size_t UpdateBandwidth(size_t recv_bytes, timestamp_t recv_time) {
    if (range.received == 0) {
      last_received = recv_time;
      delta_received = recv_bytes;
      return;
    }

    size_t time_delta = std::chrono::duration_cast<std::chrono::microseconds>(
                            recv_time - last_received)
                            .count();

    if (time_delta == 0) {
      delta_received += recv_bytes;
      return;
    }

    size_t delta_sample = delta_received * 1000.0 / time_delta; // in KB/s

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
  void WritableCallback();
  // @brief : the close callback for MpHttpClient to close me (:
  void CloseCallback();

  // request time
  timestamp_t send_time_;

  // this client is close or not
  bool is_close;
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