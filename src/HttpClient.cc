#include "HttpClient.h"
#include "macro.h"

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
void HttpClient::WritableCallback() {
  if (is_close) {
    return;
  }
  // TODO : optimization
  ssize_t n =
      write(sock_fd, output_buffer.peek(), output_buffer.readableBytes());
  if (n > 0) {
    output_buffer.retrieve(n);
    if (output_buffer.readableBytes() == 0) {
      // TODO : disable EPOLLOUT in event loop
    }
  } else {
    MPHTTP_LOG(error, "HttpClient : WritableCallback error %s\n",
               strerror(errno));
  }
}

// @brief : the close callback for MpHttpClient to close me (:
void HttpClient::CloseCallback() {
  loop_->enqueueCallback([this]() {
    // refcnt decreases to zero, sock_fd would be unregistered from the event
    // loop automatically.
    close(sock_fd);
    // FIXME : delete this could be dangerous (:
    delete this;
  });
}