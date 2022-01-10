#ifndef MPTASK_H
#define MPTASK_H

#include "HttpResponse.h"
#include <string.h>
#include <string>

// the file size, the content-range (start - end/file_size), the buffer length
// 无论是有Range请求还是没有Range请求，我们都可以一次性地完成buffer的分配
// 有Range : 直接从Request里面parse出来，然后设置
//没有Range : 第一次Response必然是准确的，只需要parse出来就好了

struct MpTask {
  MpTask() = default;

  void setResponse(HttpResponse &response) { response_.swap(response); }

  void setContentRange(size_t start, size_t end) {
    MPHTTP_ASSERT(start_ <= end_, "MpTask.setFileSize() : start > end\n");
    start_ = start;
    end_ = end;
    size_t length = end_ - start_;
    task_buffer_.resize(length, 0);
  }

  void setFileSize(size_t file_size) { file_size_ = file_size; }

  void memcpyFrom(const char *pointer, size_t start, size_t end) {
    MPHTTP_ASSERT(start <= end, "MpTask.memCpyFrom() : start > end\n");
  }

  bool get_file_size{false};
  bool prepend_response_header{false};
  bool is_range_header{false};
  size_t start_{0};
  size_t end_{0};
  size_t file_size_{0};

  // the response header
  struct HttpResponse response_;
  // the response body
  std::string task_buffer_;
};

#endif // MPTASK_H