// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "HttpResponse.h"
#include "Buffer.h"

#include <map>
#include <stdio.h>

void HttpResponse::appendToBuffer(Buffer *output) {
  char buf[32];

  output->append(version_ + " " + statusCode_ + " " + statusMessage_ + "\r\n");

  if (closeConnection_) {
    output->append("Connection: close\r\n");
  } else {
    snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
    output->append(buf);
    output->append("Connection: Keep-Alive\r\n");
  }

  for (const auto &header : headers_) {
    output->append(header.first);
    output->append(": ");
    output->append(header.second);
    output->append("\r\n");
  }

  output->append("\r\n");
  output->append(body_);
}