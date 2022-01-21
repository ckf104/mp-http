#ifndef MPTASK_H
#define MPTASK_H

#include "HttpResponse.h"
#include <string.h>
#include <string>
#include <mutex>
#include <functional>
#include "macro.h"


// forward declaration
class Request;

// @brief : the multipath http type
enum class MpHttpType {
  kNotUse = 0,
  kHasRangeHeader,
  kNoRangeHeader,
};

struct MpTask {
  MpTask() = default;

  void setRequest(struct Request* request) {  request_ = request;  }
  void setResponse(HttpResponse &response) { 
    std::call_once(set_response_flag_, [this](HttpResponse & response){
      response_ = response;
    }, response);
  }

  void setStartPoint(size_t start) { start_ = start; }

  void setContentRange(size_t start, size_t end) {
    MPHTTP_LOG(debug, "MPTask.setContentRange() : start = %zd, end = %zd", start, end);
    MPHTTP_ASSERT(start_ <= end_, "MpTask.setContentRange() : start > end\n");
    start_ = start;
    end_ = end;
    size_t length = end_ - start_ + 1;
    task_buffer_.resize(length, 0);
  }

  void setFileSize(size_t file_size) { file_size_ = file_size; }

  void setMpHttpType(const Request& request);

  ssize_t writeToSocket(int fd);

  void memcpyFrom(const char *pointer, size_t start, size_t end) {
    MPHTTP_ASSERT(start <= end, "MpTask.memCpyFrom() : start > end\n");

    if (task_type_ == MpHttpType::kHasRangeHeader) {
      memcpy(const_cast<char *>(task_buffer_.c_str()) + start - start_, 
              pointer, end - start);
    } else {
      size_t offset = end - start_;
      if (offset > task_buffer_.size()) {
        task_buffer_.resize(offset - task_buffer_.size());
      }
      memcpy(const_cast<char*>(task_buffer_.c_str()) + start - start_, pointer, end - start);
    }
  }

  // the task type
  enum MpHttpType task_type_;

  bool get_file_size{false};
  bool originally_has_range_header{false};
  bool write_buffer_safe_{false};
  size_t start_{0};
  size_t end_{0};
  size_t file_size_{0};

  // once flag : set Response
  std::once_flag set_response_flag_;

  // the request header
  struct Request *request_;
  // the response header
  struct HttpResponse response_;
  // the response body
  std::string task_buffer_;
};

#endif // MPTASK_H