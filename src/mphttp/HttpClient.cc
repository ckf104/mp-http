#include "HttpClient.h"
#include "HttpResponse.h"
#include "MpHttpClient.h"
#include "MpTask.h"
#include "macro.h"

#include <cstring>
#include <iostream>

void HttpClient::constructRangeHeader(size_t start, size_t end) {
  // construct request for HttpClient, just like what okHttp interceptor do.
  std::string range_header;

  if (end > 0) {
    MPHTTP_ASSERT(start <= end, "HttpClient : construct header fail, start > end");
    range_header = "Range: bytes=" + std::to_string(start) + "-" + std::to_string(end) + "\r\n";
  } else {
    range_header = "Range: bytes=" + std::to_string(start) + "-\r\n";
  }

  auto &request_header = *(mp_->request_header);
  output_buffer.append(request_header.method + " " + request_header.url + " HTTP/1.1\r\n");

  for (auto &header : request_header.headers) {
    if (task_buffer_->task_type_ != MpHttpType::kNotUse && 
        header.first == "Range") {
      output_buffer.append(range_header);
    } else {
      output_buffer.append(header.first + ": " + header.second);
    }
  }
  if (task_buffer_->task_type_ == MpHttpType::kNoRangeHeader) {
    output_buffer.append(range_header);
  }
  output_buffer.append("\r\n" + *mp_->request_body);
}

bool HttpClient::processResponseLine(const char *begin, const char *end) {
  bool succeed = true;
  std::string response_line(begin, end);

  size_t index;
  std::string version;
  std::string status;
  std::string reason;

  index = response_line.find_first_of(" ");

  MPHTTP_ASSERT(index != std::string::npos, "Httpclient : fail to parse http version.");
  version = response_line.substr(0, index);

  response_line = response_line.substr(index + 1);
  index = response_line.find_first_of(" ");
  MPHTTP_ASSERT(index != std::string::npos, "Httpclient : fail to parse http status code.");
  status = response_line.substr(0, index);

  reason = response_line.substr(index + 1);

  response_.setVersion(version);
  response_.setStatusCode(status);
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
  if (task_buffer_->task_type_ != MpHttpType::kHasRangeHeader && 
      range .end == 0) {

    if (task_buffer_->task_type_ == MpHttpType::kNotUse) {
      std::string content_length_header = response_.getHeader("Content-Length");
      if (content_length_header != "") {
        size_t length = 0;
        if (sscanf(content_length_header.c_str(), "%zu", &length) < 1) {
          MPHTTP_FATAL("HttpClient GetSize : fail to get "
                       "Content-Length from response");
        }
        range.end = length - 1;
        MPHTTP_LOG(debug, "content length = %zd", length);
        task_buffer_->setContentRange(0, length - 1);
      }
    } else if (task_buffer_->task_type_ == MpHttpType::kNoRangeHeader) {
      std::string result =
          response_.getHeader(std::string("Content-Range"));
      size_t start = 0, end = 0, file_size = 0;

      if (result != "") {
      // TODO : do prefetch when getting the whole file size.
        if (sscanf(result.c_str(), "bytes %zu-%zu/%zu", &start, &end, &file_size) < 3) {
          MPHTTP_FATAL("HttpClient GetSize : fail to get "
                            "Content-Range from response");
        } else {
          range.start = start;
          range.end = end;
          task_buffer_->setContentRange(start, end);
          task_buffer_->setFileSize(file_size);
          mp_->rival_->reschedule();
        }
      }
    }
    task_buffer_->setResponse(response_);
  } else if (task_buffer_->task_type_ == MpHttpType::kHasRangeHeader) {
    task_buffer_->setResponse(response_);
  }
}

void HttpClient::onBody(timestamp_t recv_time) {
  // update the bandwidth
  size_t recv_bytes = input_buffer.readableBytes();
  
  UpdateBandwidth(recv_bytes, recv_time);

  size_t end = range.start + range.received + recv_bytes;

  if (range.end < end) {
    is_close = true;
    end = range.end + 1;
  }

  size_t length = end - range.start - range.received;

  //MPHTTP_LOG(info, "HttpClient : current start = %zu, current end = %zu, range.end = %zu", 
  //          range.start + range.received, end, range.end);

  task_buffer_->memcpyFrom(input_buffer.peek(), range.start + range.received, end);
  input_buffer.retrieve(length);

  range.received += length;

  // if the job is almost complete, rescedule

  if (GetRemaining() < 1.5 * GetBandwidth() * mp_->GetRTT()) {
    if (!has_reschedule) {
      has_reschedule = true;
      mp_->reschedule();
    }
  }

  if (is_close) {
    CloseCallback();
    mp_->resetTaskQueue(this);
  }
}

void HttpClient::onReadable(timestamp_t received_time,
                            bool is_end_of_stream) {
  if (read_state == kInitial) {
    mp_->UpdateRTT(send_time_, received_time);
    read_state = kReadingResponseLine;
  }
  if (read_state == kReadingResponseLine || 
      read_state == kReadingHeaders) {
    parseResponse(received_time);
  }
  if (read_state == kGetAllHeaders) {
    onGetAllHeaders();
    read_state = kReadingBody;
  }
  if (read_state == kReadingBody) {
    onBody(received_time);
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

  onReadable(now, is_end_of_stream);
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