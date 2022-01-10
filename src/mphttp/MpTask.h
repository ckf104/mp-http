#ifndef MPTASK_H
#define MPTASK_H

#include "HttpResponse.h"
#include <string.h>
#include <string>

struct MpTask {
  MpTask() = default;

  // only initial response would be prepend
  void prependResponse(const HttpResponse &response) {
    if (!prepend_response_header) {
      // prepend for the MpTask for only once
      prepend_response_header = true;
      task_buffer_ += "HTTP/1.1 " + std::to_string(response.statusCode_) + " " +
                      response.statusMessage_ + "\r\n";

      if (response.closeConnection_) {
        task_buffer_ += "Connection: close\r\n";
      } else {
        task_buffer_ += "Connection: Keep-Alive\r\n";
      }

      for (const auto &header : response.headers_) {
        task_buffer_ += header.first + ": " + header.second + "\r\n";
      }
      task_buffer_ += "\r\n";

      if (get_file_size) {
        size_t new_length = task_buffer_.size();
        new_length += end_ - start_;
        task_buffer_.resize(new_length, 0);
      }
    }
  }

  void setFileSize(size_t start, size_t end) {
    MPHTTP_ASSERT(start_ <= end_, "MpTask.setFileSize() : start > end\n");
    get_file_size = true;
    start_ = start;
    end_ = end;
  }

  void memcpyFrom(const char *pointer, size_t start, size_t end) {
    MPHTTP_ASSERT(start <= end, "MpTask.memCpyFrom() : start > end\n");

    size_t length = end - start;

    if (!get_file_size) {
      size_t old_length = task_buffer_.size();
      size_t new_length = old_length + length;
      task_buffer_.resize(new_length, 0);
      memcpy(const_cast<char *>(task_buffer_.c_str() + old_length), pointer,
             length);
    } else {
      memcpy(const_cast<char *>(task_buffer_.c_str() + start), pointer, length);
    }
  }

  bool get_file_size{false};
  bool prepend_response_header{false};
  bool is_range_header{false};
  size_t start_{0};
  size_t end_{0};
  std::string task_buffer_;
};

#endif // MPTASK_H