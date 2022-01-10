#include "mphttp/general.hpp"
#include "mphttp/helper.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <poll.h>
#include <regex>
#include <set>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unistd.h>

#include <arpa/inet.h>
#include <chrono>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "general.hpp"
#include "helper.hpp"

#include "mphttp/MpHttpClient.h"
#include "mphttp/MpTask.h"
#include "mphttp/eventloop.h"

#define MAX_OBJECT_SIZE 1024000
using std::map;
using std::set;
using std::string;

/*static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64;
rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n"; static const char *conn_hdr =
"close\r\n"; static const char *proxy_hdr = "close\r\n";*/
static string range_header("Range");
static string host_header("Host");
static string content_len_hdr("Content-Length");
static string connect_response("HTTP/1.1 200 Connection established\r\n\r\n");
static string connect_method("CONNECT");

extern char **environ;
struct HttpStream;
using StreamPtr = std::unique_ptr<HttpStream>;

void OnNewConnection(int fd);
void clienterror(int fd, const char *cause, const char *errnum,
                 const char *shortmsg, const char *longmsg);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void *Thread(void *vargp);
void OnConnectionMethod(HttpStream *client);

mp_http need_mp_http(const Request &req, std::pair<uint64_t, uint64_t> &range);
std::optional<Response> send_normal_request(HttpStream &client,
                                            HttpStream (&servers)[path_num],
                                            const Request &client_req,
                                            string &req_body,
                                            string &reply_body);
std::optional<Response> send_mp_request();
std::optional<Response>
send_mp_request(HttpStream &client, HttpStream (&servers)[path_num],
                const Request &client_req, string &req_body, string &reply_body,
                mp_http type, const std::pair<uint64_t, uint64_t> &byte_range);

struct HttpStream {
public:
  Rio_t fd;
  uint latency = 0;   // unit ms
  uint bandwidth = 0; // unit kB
  string hostname;
  int port;
  HttpStream(int _fd) : fd(_fd) {}
  void close() {
    if (fd)
      Close(fd.rio_fd);
  }
  /**
   * @brief factory method for HttpStream
   *
   * @param _fd file descriptor of the stream
   * @return if this function returns null, the stream is closed.
   */
  static StreamPtr generateStream(int _fd) {
    auto r = std::make_unique<HttpStream>(_fd);
    return r;
  }
  operator bool() { return fd; }
  /**
   * @brief Get the Request object
   *
   * @return int 0 means success
   */
  std::optional<Request> getRequest(string &req_body) {
    // char _method[max_line], _uri[max_line], _version[max_line];
    // char _hostname[max_line], _path[max_line];
    // req_body.clear();
    Request r;

    if (fd.rio_readlineb(r.headline) <= 0) {
      return std::nullopt;
    }

    if (r.headline == "\r\n") {
      return std::nullopt;
    }

    std::istringstream first_line(r.headline);
    // sscanf(buf.c_str(), "%s %s %s", _method, _uri, _version);

    first_line >> r.method >> r.url >>
        r.http_proto; /* r.http_proto = http/1.1/r/n */

    std::cout << r.url << " " << r.method << std::endl;

    build_requesthdrs(r.headers);

    if (r.headers.count(content_len_hdr) == 1) {
      auto leng = std::stoul(r.headers.at(content_len_hdr));
      if (fd.rio_readnb(req_body, leng) != (ssize_t)leng)
        return std::nullopt;
    }
    string host;
    try {
      host = r.headers.at(host_header);
    } catch (std::exception &e) {
      std::cout << "error at : " << __LINE__ << " " << e.what() << std::endl;
    }
    auto index = host.find(':'); // Host: <hostname>:<port>
    if (index == string::npos) {
      hostname = host.substr(0, host.size() - 2);
      port = 80;
    } else {
      hostname = host.substr(0, index);
      port = std::stoi(host.substr(index + 1));
    }
    return std::make_optional(r);
  }

  std::optional<Response> getResponse() {
    Response r;
    int n;
    string buf;
    n = fd.rio_readlineb(r.headline);
    char ver[max_line], code[max_line], status[max_line];
    sscanf(r.headline.c_str(), "%s%s%s", ver, code, status);
    r.version = string(ver);
    r.code = string(code);
    r.status = string(status);

    while ((n = fd.rio_readlineb(buf)) > 0) {
      if (buf == "\r\n")
        break;
      int p;
      ssize_t len = buf.size();
      for (p = 0; p < len; ++p)
        if (buf[p] == ':')
          break;
      if (p < len)
        // TODO:Not robust
        r.headers[buf.substr(0, p)] = buf.substr(p + 2, len);
    }
    return std::make_optional(r);
  }

  void build_requesthdrs(map<string, string> &headers) {
    headers.clear();
    string buf;
    while (fd.rio_readlineb(buf) > 0) {
      if (buf == "\r\n")
        break;
      int p;
      ssize_t len = buf.size();
      for (p = 0; p < len; ++p)
        if (buf[p] == ':')
          break;
      if (p < len) {
        headers[buf.substr(0, p)] = buf.substr(p + 2, len);
      }
    }
  }

  static bool sendReqeust(Rio_t &w, const Request &r, const string &body) {
    char buf[max_line];
    sprintf(buf, "GET %s HTTP/1.1\r\n", r.url.c_str());
    if (w.rio_writen((void *)buf, strlen(buf), MSG_MORE) < 0)
      return false;

    for (auto x : r.headers) {
      sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
      if (w.rio_writen((void *)buf, strlen(buf), MSG_MORE) < 0)
        return false;
    }
    sprintf(buf, "\r\n");

    if (w.rio_writen((void *)buf, strlen(buf)) < 0 ||
        w.rio_writen(body.c_str(), body.size()) < 0)
      return false;
    return true;
  }

  static bool sendResponse(Rio_t &w, const Response &r,
                           const Body &reply_body) {
    char buf[max_line];
    if (w.rio_writen((void *)r.headline.c_str(), r.headline.size()) < 0)
      return false;
    for (auto x : r.headers) {
      sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
      if (w.rio_writen((void *)buf, strlen(buf)) < 0)
        return false;
    }
    sprintf(buf, "\r\n");
    if (w.rio_writen((void *)buf, strlen(buf)) < 0)
      return false;
    if (w.rio_writen(reply_body.content.get(), reply_body.length) < 0)
      return false;
    return true;
  }
};

int main(int argc, char **argv) {
  int listenfd, connfd;
  pthread_t tid;
  char hostname[max_line], port[max_line];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (sockaddr *)&clientaddr, &clientlen);

    Getnameinfo((sockaddr *)&clientaddr, clientlen, hostname, max_line, port,
                max_line, 0);

    printf("Accepted connection from (%s:%s)\n", hostname, port);

    std::thread worker = std::thread([connfd]() { OnNewConnection(connfd); });

    worker.detach();
  }

  Close(listenfd);
  return 0;
}

void OnNewConnection(int client_fd) {
  auto clientStream = HttpStream(client_fd);

  if (!clientStream) {
    printf("[Err] Connection failed getStream\n");
    return;
  }

  while (1) {
    string req_body, reply_body;

    auto req_ptr = clientStream.getRequest(req_body);

    if (!req_ptr) {
      // printf("[Err] Connection failed at get reqeust.\n");
      break;
    }

    Request &req = req_ptr.value();

    if (req.method == connect_method) {
      OnConnectionMethod(&clientStream);
      break;
    }

    struct sockaddr_in address[2];

    address[0].sin_family = AF_INET;
    address[0].sin_addr.s_addr = inet_addr("10.100.1.2");
    address[0].sin_port = htons(80);

    address[1].sin_family = AF_INET;
    address[1].sin_addr.s_addr = inet_addr("10.100.2.2");
    address[1].sin_port = htons(80);

    struct MpTask task;
    struct EventLoop loop;
    struct MpHttpClient *clients[2];

    for (int i = 0; i < 2; i++) {
      clients[i] = new struct MpHttpClient(&loop, &task, &address[i]);
      clients[i]->request_header = &req;
      clients[i]->request_body = &req_body;
    }

    for (int i = 0; i < 2; i++) {
      clients[i]->rival_ = clients[1 - i];
    }

    clients[0]->run(0, 0);

    auto isComplete = [clients]() {
      for (int i = 0; i < 2; i++) {
        if (clients[i]->tasks_.running || clients[i]->tasks_.next) {
          return false;
        }
      }
      return true;
    };

    while (!isComplete()) {
      loop.run(1000);
    }

    // TODO : send the buffer in MpTask.
    if (!clientStream.fd.rio_writen((void *)task.task_buffer_.c_str(),
                                    task.task_buffer_.size())) {
      break;
    }

    // cleanup:
    clientStream.close();
    delete clients[0];
    delete clients[1];
  }
}

std::optional<Response> send_normal_request(HttpStream &client,
                                            HttpStream (&servers)[path_num],
                                            const Request &client_req,
                                            string &req_body,
                                            string &reply_body) {

  if (!servers[0] && !servers[1]) {
    servers[0].fd.rio_fd =
        Open_clientfd(client.hostname.c_str(),
                      std::to_string(client.port).c_str(), &servers[0].latency);
    if (servers[0].fd < 0) {
      servers[0].fd = 0;
      printf("[Err] Connection failed at open endserver_fd\n");
      return std::nullopt;
    }
  }

  HttpStream &server = servers[0] ? servers[0] : servers[1];
  if (!server.sendReqeust(server.fd, client_req, req_body))
    return std::nullopt;

  using namespace std::chrono;

  auto now = system_clock::now();
  auto response = server.getResponse();
  server.latency =
      duration_cast<milliseconds>(system_clock::now() - now).count();

  if (!response)
    return std::nullopt;
  decltype(auto) r = response.value();
  if (r.headers.count(content_len_hdr) == 1) {
    auto leng = std::stoul(r.headers.at(content_len_hdr));
    now = system_clock::now();
    if (server.fd.rio_readnb(reply_body, leng) != (ssize_t)leng)
      return std::nullopt;
    auto delay = duration_cast<milliseconds>(system_clock::now() - now).count();
    if (delay > 0)
      server.bandwidth = leng / delay;
  } else { // connection close or transfer-encoding
    printf("[Error] Not implemented!.\n");
    return std::nullopt;
  }
  return response;
}

void OnConnectionMethod(HttpStream *client) {
  uint latency;
  int byte_count;
  auto server_fd = Open_clientfd(
      client->hostname.c_str(), std::to_string(client->port).c_str(), &latency);
  if (server_fd < 0)
    return;
  client->fd.rio_writen(connect_response.c_str(),
                        connect_response.size()); // send connect response

  pollfd fd_pool[2]; // fd_pool[0] -> client, fd_pool[1] -> server
  fd_pool[0].fd = client->fd.rio_fd;
  fd_pool[0].events = POLLIN;
  fd_pool[0].revents = 0;
  fd_pool[1].fd = server_fd;
  fd_pool[1].events = POLLIN;
  fd_pool[1].revents = 0;
  char buf[max_line];

  while (poll(fd_pool, 2, -1) >= 0) // -1 means no timeout
  {
    for (int i = 0; i < 2; ++i) {
      if (fd_pool[i].revents & POLLIN) {
        byte_count = read(fd_pool[i].fd, buf, max_line);
        if (writen(fd_pool[1 - i].fd, buf, byte_count) < 0)
          break;
      }
    }
    if (!(fd_pool[0].revents & POLLIN) && (fd_pool[0].revents & POLLHUP) &&
        !(fd_pool[1].revents & POLLIN) && (fd_pool[1].revents & POLLHUP))
      break;
  }
  Close(server_fd);
}