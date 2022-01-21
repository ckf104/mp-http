// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef HTTP_HTTPRESPONSE_H
#define HTTP_HTTPRESPONSE_H

#include "types.h"

#include <map>
#include <string>

struct Buffer;
struct HttpResponse {
public:
  enum HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k206PartialContent = 206,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k404NotFound = 404,
  };

  HttpResponse() : closeConnection_(false) {}

  explicit HttpResponse(bool close)
      : closeConnection_(close) {}

  void setVersion(const std::string& version) {  version_ = version;  }

  void setStatusCode(const std::string& code) { statusCode_ = code; }

  void setStatusMessage(const std::string &message) {
    statusMessage_ = message;
  }

  const std::string &StatusMessage() const { return statusMessage_; }

  void setCloseConnection(bool on) { closeConnection_ = on; }

  bool closeConnection() const { return closeConnection_; }

  void setContentType(const std::string &contentType) {
    addHeader("Content-Type", contentType);
  }

  void swap(HttpResponse &that) {
    std::swap(statusCode_, that.statusCode_);
    std::swap(statusMessage_, that.statusMessage_);
    std::swap(closeConnection_, that.closeConnection_);
    std::swap(body_, that.body_); // should be a buffer
    headers_.swap(that.headers_);
  }

  // FIXME: replace string with StringPiece
  void addHeader(const std::string &key, const std::string &value) {
    headers_[key] = value;
  }

  std::string getHeader(const std::string &field) const {
    std::string result = "";
    std::map<std::string, std::string>::const_iterator it =
        headers_.find(field);
    if (it != headers_.end()) {
      result = it->second;
    }
    return result;
  }

  void setBody(const std::string &body) { body_ = body; }

  void appendToBuffer(Buffer *output);

  std::map<std::string, std::string> headers_;
  std::string version_;
  std::string statusCode_;
  // FIXME: add http version
  std::string statusMessage_;
  bool closeConnection_;
  std::string body_;
};

#endif // MUDUO_NET_HTTP_HTTPRESPONSE_H
