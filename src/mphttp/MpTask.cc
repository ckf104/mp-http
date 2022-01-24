#include "MpTask.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>
#include <string>

#include "helper.hpp"

namespace detail {
ssize_t writen(int fd, const void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = (char *)usrbuf;

    while (nleft > 0) {
        if ((nwritten = send(fd, bufp, nleft, MSG_NOSIGNAL)) <= 0) {
            if (errno == EINTR) {
                // call send again if being interrupted.
                nwritten = 0;
            } else {
                return -1;
            }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}
}  // namespace detail

void MpTask::setMpHttpType(const Request &request, bool enable_mphttp) {
    if (request.method.compare("GET") != 0) {
        task_type_ = MpHttpType::kNotUse;
    } else if (request.headers.count("Range") == 1) {
        int ret = sscanf(request.headers.at("Range").c_str(), "bytes=%zd-%zd",
                         &start_, &end_);

        if (ret < 2) {
            task_type_ = MpHttpType::kNotUse;
        } else if (end_ - start_ < threshold || !enable_mphttp) {
            setContentRange(start_, end_);
            task_type_ = MpHttpType::kNotUse;
        } else {
            setContentRange(start_, end_);
            task_type_ = MpHttpType::kHasRangeHeader;
        }

    } else {
        size_t index = request.url.find_last_of('.');
        if (index != string::npos &&
            request.url.substr(index).compare(".m4s") == 0 && enable_mphttp) {
            task_type_ = MpHttpType::kNoRangeHeader;
        } else {
            task_type_ = MpHttpType::kNotUse;
        }
    }
}

ssize_t MpTask::writeToSocket(int fd) {
    std::string status_code;
    std::string status_message;

    status_code = response_.statusCode_;
    status_message = response_.statusMessage_;

    // add filter for kNoRangeHeader
    if (task_type_ == MpHttpType::kNoRangeHeader) {
        // a hook to modify the status code and statue message
        if (status_code == "206") {
            status_code = "200";
            status_message = "OK";
        }
    }

    std::string response_line =
        response_.version_ + " " + status_code + " " + status_message + "\r\n";

    if (detail::writen(fd, (void *)response_line.c_str(),
                       response_line.size()) < 0) {
        MPHTTP_LOG(error, "WriteToSocket error : header %s\n",
                   response_line.c_str());
        return -1;
    }

    for (const auto &header : response_.headers_) {
        std::string result;
        if (header.first == "Content-Range") {
            // a hook to modify the Content-Range header
            if (task_type_ == MpHttpType::kNoRangeHeader) {
                // filter for kNorangeHeader
                continue;
            } else if (task_type_ == MpHttpType::kHasRangeHeader) {
                result = "Content-Range: " + std::to_string(start_) + "-" +
                         std::to_string(end_) + "/*\r\n";
            } else {
                result = header.first + ": " + header.second + "\r\n";
            }
        } else if (header.first == "Content-Length") {
            // a hook to modify the Content-Length header
            if (task_type_ == MpHttpType::kHasRangeHeader) {
                result = "Content-Length: " +
                         std::to_string(end_ - start_ + 1) + "\r\n";
            } else {
                result = header.first + ": " + header.second + "\r\n";
            }
        } else {
            result = header.first + ": " + header.second + "\r\n";
        }

        if (detail::writen(fd, (void *)result.c_str(), result.size()) < 0) {
            MPHTTP_LOG(error, "WriteToSocket error : %s\n", result.c_str());
            return -1;
        }
    }

    std::string crlf = "\r\n";
    if (detail::writen(fd, crlf.c_str(), crlf.size()) < 0) {
        MPHTTP_LOG(error, "WriteToSocket error : \\r\\n");
        return -1;
    }

    if (detail::writen(fd, task_buffer_.c_str(), task_buffer_.size()) < 0) {
        MPHTTP_LOG(error, "WriteToSocket error : task_buffer");
        return -1;
    }
}