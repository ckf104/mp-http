#ifndef MPTASK_H
#define MPTASK_H

#include <string.h>

#include <functional>
#include <mutex>
#include <string>

#include "HttpResponse.h"
#include "macro.h"
#include "types.h"

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

    void setRequest(struct Request *request) { request_ = request; }
    void setResponse(HttpResponse &response) {
        std::call_once(
            set_response_flag_,
            [this](HttpResponse &response) { response_ = response; }, response);
    }

    void setStartPoint(size_t start) { start_ = start; }
    void setContentRange(size_t start, size_t end) {
        MPHTTP_LOG(debug, "MPTask.setContentRange() : start = %zd, end = %zd",
                   start, end);
        MPHTTP_ASSERT(start_ <= end_,
                      "MpTask.setContentRange() : start > end\n");
        start_ = start;
        end_ = end;
        size_t length = end_ - start_ + 1;
        workload_[0] = length;
        workload_[1] = 0;
        task_buffer_.resize(length, 0);
    }
    void setFileSize(size_t file_size) { file_size_ = file_size; }
    void setStartTime(timestamp_t start_time) {
        std::call_once(
            set_start_time_flag_,
            [this](timestamp_t start_time) { start_time_ = start_time; },
            start_time);
    }
    void setMpHttpType(const Request &request, bool enable_mphttp);

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
            memcpy(const_cast<char *>(task_buffer_.c_str()) + start - start_,
                   pointer, end - start);
        }
    }

    //@brief the task type
    enum MpHttpType task_type_;

    //@brief the task start time
    timestamp_t start_time_;

    //@brief the http task (e.g. a DASH block) range
    size_t start_{0};
    size_t end_{0};
    size_t file_size_{0};

    //@brief the initial dispatch for this task
    bool is_initialize_dispatch_{true};
    size_t workload_[2];

    //@brief once flag : set Response
    std::once_flag set_response_flag_;
    //@brief once flag : set Start Time
    std::once_flag set_start_time_flag_;
    //@brief once flag : set File Size
    // TODO : directly get the file size and do prefetch
    std::once_flag set_size_flag_;

    //@brief the request header
    struct Request *request_;
    //@brief the response header
    struct HttpResponse response_;
    //@brief the response body
    std::string task_buffer_;
};

#endif  // MPTASK_H