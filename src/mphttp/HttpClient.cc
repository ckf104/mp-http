#include "HttpClient.h"
#include "HttpResponse.h"
#include "MpHttpClient.h"
#include "MpTask.h"
#include "macro.h"

#include <cstring>

void HttpClient::constructRangeHeader(size_t start, size_t end) {
  // construct range request for this HttpClient
  std::string range_request;
  if (end > 0) {
    range_request =
        "bytes=" + std::to_string(start) + "-" + std::to_string(end);
  } else {
    range_request = "bytes=" + std::to_string(start) + "-";
  }

  bool found_range = false;
  // FIXME : append what ?
  output_buffer.append(" ");
  for (auto &header : mp_->request_header->headers) {
    if (header.first != "Range") {
      // FIXME : necessary?
      output_buffer.append(header.first + ": " + header.second + "\r\n");
    } else {
      output_buffer.append(header.first + ": " + range_request + "\r\n");
    }
  }
  if (!found_range) {
    output_buffer.append("Range: " + range_request + "\r\n");
  }
  output_buffer.append("\r\n" + *mp_->request_body);
}

bool HttpClient::processResponseLine(const char *begin, const char *end) {
  bool succeed = true;
  std::string response_line(begin, end);

  char version[500], status[500], reason[500];
  sscanf(response_line.c_str(), "%s %s %s", version, status, reason);

  response_.setStatusCode(HttpResponse::k200Ok);
  response_.setStatusMessage(reason);
  return succeed;
}

// return false if any error
bool HttpClient::parseResponse(timestamp_t receiveTime) {
  bool ok = true;
  bool hasMore = true;
  while (hasMore) {
    if (read_state == kReadingResponseLine) {
      // parsing the Http response line
      const char *crlf = input_buffer.findCRLF();
      if (crlf) {
        ok = processResponseLine(input_buffer.peek(), crlf);
        if (ok) {
          input_buffer.retrieveUntil(crlf + 2);
          read_state = kReadingHeaders;
        } else {
          hasMore = false;
        }
      } else {
        hasMore = false;
      }
    } else if (read_state == kReadingHeaders) {
      const char *crlf = input_buffer.findCRLF();
      if (crlf) {
        const char *colon = std::find(input_buffer.peek(), crlf, ':');
        if (colon != crlf) {
          std::string field(input_buffer.peek(), colon);
          ++colon;
          while (colon < crlf && isspace(*colon)) {
            ++colon;
          }
          std::string value(colon, crlf);
          while (!value.empty() && isspace(value[value.size() - 1])) {
            value.resize(value.size() - 1);
          }
          response_.addHeader(field, value);
        } else {
          read_state = kGetAllHeaders;
          hasMore = false;
        }
        input_buffer.retrieveUntil(crlf + 2);
      } else {
        hasMore = false;
      }
    } else if (read_state == kReadingBody) {
      return ok;
    }
  }
  return ok;
}

void HttpClient::onGetAllHeaders() {
  if (range.end == 0) {
    std::string result =
        std::move(response_.getHeader(std::string("Content-Range")));
    size_t start = 0, end = 0, file_size = 0;
    if (result != "") {
      // TODO : prefetch
      if (sscanf(result.c_str(), "%zu-%zu/%zu", &start, &end, &file_size) < 3) {
        MPHTTP_LOG(error, "HttpClient GetSize() : fail to get "
                          "Content-Range from response\n");
        return;
      } else {
        range.start = start;
        range.end = end;
        task_buffer_->setContentRange(start, end);
        task_buffer_->setFileSize(file_size);
        mp_->rival_->reschedule();
      }
    }
    task_buffer_->setResponse(response_);
  }
}

void HttpClient::onBody(size_t recv_bytes, timestamp_t recv_time) {
  // update the bandwidth
  UpdateBandwidth(recv_bytes, recv_time);

  range.received += recv_bytes;

  size_t end = range.start + range.received;

  // write to the buffer
  size_t length = end - range.start;
  MPHTTP_LOG(info, "HttpClient : write %zu bytes to the task buffer\n", length);
  task_buffer_->memcpyFrom(input_buffer.peek(), range.start, end);
  input_buffer.retrieve(length);

  // if the job is almost complete, rescedule
  if (GetRemaining() < 1.5 * GetBandwidth() * mp_->GetRTT()) {
    if (!has_reschedule) {
      has_reschedule = true;
      mp_->reschedule();
    }
  }

  if (end >= range.end || is_close) {
    is_close = true;
    CloseCallback();
    mp_->resetTaskQueue(this);
  }
}

void HttpClient::onReadable(size_t recv_bytes, timestamp_t received_time,
                            bool is_end_of_stream) {
  if (read_state == kInitial) {
    // FIXME : see if an range header in the Request Header
    // if not, add one ...
    mp_->UpdateRTT(send_time_, received_time);
    read_state = kReadingHeaders;
  }
  if (read_state == kReadingHeaders) {
    parseResponse(received_time);
  }
  if (read_state == kGetAllHeaders) {
    onGetAllHeaders();
    read_state = kReadingBody;
  }
  if (read_state == kReadingBody) {
    onBody(recv_bytes, received_time);
  }
}

// @brief : the readable callback for the event loop to execute
void HttpClient::ReadableCallback(timestamp_t now) {
  int saved_errno = 0;
  bool is_end_of_stream = false;
  ssize_t ret = input_buffer.readFd(sock_fd, &saved_errno);

  if (ret < 0) {
    MPHTTP_LOG(error, "HttpClient : socket %d read with error %s\n", sock_fd,
               strerror(saved_errno));
    errno = saved_errno;
  } else if (ret == 0) {
    is_close = true;
    is_end_of_stream = true;
  }

  onReadable(ret, now, is_end_of_stream);
}

// @brief : the writable callback for the event loop to execute
void HttpClient::WritableCallback(timestamp_t now) {
  if (is_close) {
    return;
  }
  if (write_state == kConnecting) {
    // connection established
    write_state = kWritingRequest;
    send_time_ = now;
  }
  if (write_state == kWritingRequest) {
    ssize_t n =
        write(sock_fd, output_buffer.peek(), output_buffer.readableBytes());
    if (n > 0) {
      output_buffer.retrieve(n);
      if (output_buffer.readableBytes() == 0) {
        // TODO : disable EPOLLOUT in event loop
        write_state = kWritingComplete;
      }
    } else {
      MPHTTP_LOG(error, "HttpClient : WritableCallback error %s\n",
                 strerror(errno));
    }
  }
}

// @brief : the close callback
void HttpClient::CloseCallback() {
  loop_->enqueueCallback([this]() {
    // refcnt decreases to zero, sock_fd would be unregistered from the event
    // loop automatically.
    close(sock_fd);
    // FIXME : delete this could be dangerous
    delete this;
  });
}