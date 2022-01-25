#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <iostream>

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

// state machine for connect pool
enum PoolState {
    kInUsed = 0,
    kIdleConnecting,
    kIdleConnected,
};

// state machine for onReadEvent
// the state for reading response
enum ReadableState {
    kInitial = 0,
    kReadingResponseLine,
    kReadingHeaders,
    kGetAllHeaders,
    kGetAllHeadersWithHead,  // TODO : for head method
    kReadingBody,
};

// state machine for onWriteEvent
// the state for sending request
// on writing request, you only need to write to the buffer.
enum WritableState {
    kConnecting = 0,
    kWritingRequest,
    kWritingComplete,
    kWritingHeadRequest,          // TODO : for head method
    kWritingHeadRequestComplete,  // TODO : for head method
};

struct HttpClient {
    HttpClient(EventLoop *loop, MpHttpClient *mp, MpTask *task_buffer,
               struct sockaddr_in addr_in, size_t start, size_t end)
        : loop_(loop),
          mp_(mp),
          task_buffer_(task_buffer),
          server_addr_(addr_in),
          range({start, end, 0}) {
        ssize_t result;

        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        MPHTTP_ASSERT(sock_fd != -1, "HttpClient : set up new socket fail\n");

        result = setNonBlocking(sock_fd);
        MPHTTP_ASSERT(result != -1,
                      "HttpClient : socket %d set nonblock fail\n", sock_fd);

        int ret = connect(sock_fd, (const sockaddr *)&server_addr_,
                          sizeof(server_addr_));

        // FIXME :
        send_time_ = std::chrono::high_resolution_clock::now();
        if (ret < 0) {
            if (errno != EINPROGRESS) {
                MPHTTP_FATAL(
                    "HttpClient : socket %d non blocking connect fail\n",
                    sock_fd);
            }
        }

        event_state_ = EPOLLIN | EPOLLOUT | EPOLLET;
        result = loop_->registerEvent(sock_fd, event_state_, this);
        MPHTTP_ASSERT(result != -1,
                      "HttpClient : socket %d add into epoll fail\n", sock_fd);

        constructRangeHeader(start, end);
    }

    void StartWith(struct MpTask *task_buffer, size_t start, size_t end) {
        task_buffer_ = task_buffer;
        range.start = start;
        range.end = end;
        range.received = 0;

        // construct request
        constructRangeHeader(start, end);

        // if (pool_state_ == kIdleConnected) {
        //     MPHTTP_LOG(debug, "HttpClient : Connected");
        //     pool_state_ = kInUsed;
        //  register the event again
        //    event_state_ = EPOLLIN | EPOLLOUT | EPOLLET;
        //    int result = loop_->registerEvent(sock_fd, event_state_, this);
        //    MPHTTP_ASSERT(result != -1,
        //                  "HttpClient : socket %d add into epoll fail\n",
        //                  sock_fd);
        // directly write to the socket
        //    WritableCallback(std::chrono::high_resolution_clock::now());
        //}

        // pool_state_ = kInUsed;
    }

    void ResetWith(struct MpTask *task_buffer, size_t start, size_t end) {
        MPHTTP_LOG(debug, "HttpClient.reset() : reset http clint state.");
        send_time_ = {};

        {
            struct HttpResponse response;
            response_.swap(response);
        }

        is_close = false;
        has_reschedule = false;
        task_buffer_ = task_buffer;

        input_buffer.retrieveAll();
        output_buffer.retrieveAll();

        range = {start, end, 0};

        delta_received = 0;
        last_received = {};
        bandwidth = 0;

        // reset read state
        read_state = kInitial;
        // reset write state
        write_state = kWritingRequest;

        // enable write again
        enableWrite();

        // reconstruct request
        constructRangeHeader(start, end);

        // FIXME : send time
        // send_time_ = std::chrono::high_resolution_clock::now();
        // TODO : We don't reset socket fd and address, since this function is
        // for next initial server choosing....
    }

    void constructRangeHeader(size_t start, size_t end);

    void sendRequest();

    bool processResponseLine(const char *begin, const char *end);

    bool parseResponse(timestamp_t recv_time);

    void onGetAllHeaders();

    void onBody(timestamp_t recv_time);

    void onReadable(timestamp_t received_time, bool is_end_of_stream);

    void enableRead() {
        event_state_ |= EPOLLIN;
        loop_->modifyEvent(sock_fd, event_state_, this);
    }

    void enableWrite() {
        event_state_ |= EPOLLOUT;
        loop_->modifyEvent(sock_fd, event_state_, this);
    }

    void enableAll() {
        event_state_ |= EPOLLIN | EPOLLOUT;
        loop_->modifyEvent(sock_fd, event_state_, this);
    }

    void disableRead() {
        event_state_ &= ~EPOLLIN;
        // FIXME :
        loop_->modifyEvent(sock_fd, event_state_, this);
    }

    void disableWrite() {
        event_state_ &= ~EPOLLOUT;
        loop_->modifyEvent(sock_fd, event_state_, this);
    }

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
        size_t time_delta =
            std::chrono::duration_cast<std::chrono::milliseconds>(recv_time -
                                                                  last_received)
                .count();

        if (time_delta == 0) {
            return;
        }

        size_t delta_sample = delta_received * 1000 / time_delta;  // in byte/s

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

    timestamp_t start_time;

    // TODO : remove old bandwidth sampler
    size_t delta_received{0};
    timestamp_t last_received;
    size_t bandwidth{0};

    int sock_fd;        // my socket fd
    EventLoop *loop_;   // my event loop
    int event_state_;   // my event state, EPOLLIN | EPOLLOUT | EPOLLET
    MpHttpClient *mp_;  // which MpHttpClient it belongs to
    enum ReadableState read_state { kInitial };      // my reading state
    enum WritableState write_state { kConnecting };  // my writing state
    enum PoolState pool_state_ { kIdleConnecting };
};

#endif