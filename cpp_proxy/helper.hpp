#ifndef __HELPER_HPP
#define __HELPER_HPP

#include <string>
#include <vector>
#include <memory>

#include <sys/socket.h>
#include "general.hpp"

class Request;
class Response;
class Rio_t;

using std::string;
using std::vector;
using Request_ptr = std::unique_ptr<Request>;
using Response_ptr = std::unique_ptr<Response>;
using Rio_ptr = std::unique_ptr<Rio_t>;

class Rio_t
{
public:
    int rio_fd;                    /* Descriptor for this internal buf */
    int rio_cnt;                   /* Unread bytes in internal buf */
    char *rio_bufptr;              /* Next unread byte in internal buf */
    char rio_buf[rio_buffer_size]; /* Internal buffer */

    Rio_t(int fd);

    ssize_t rio_readlineb(string &usrbuf);
    ssize_t rio_readnb(vector<uint8_t> &usrbuf, size_t n);
    ssize_t rio_read(void *usr_buf, size_t n);

    ssize_t rio_writen(void *usrbuf, size_t n, int flags = 0);
};

class Path
{
public:
    string ip_addr;
    Rio_ptr rio_ptr;
    int latency;
    int bandwidth;

    Path(const string &addr, int fd, int init_latency);
    Path();
};

class Request
{
public: // private ?
    string method;
    string host;
    string url;
    string http_proto;   // http/1.x
    string request_line; // method url http_proto

    vector<string> header; // string + \r\n ?
    vector<uint8_t> body;  // to avoid \0 in body, so not use string

    int send(int fd);                      // send a http request, return < 0 means failed
    static Request_ptr recv(Rio_t *rio_t); // return nullptr means failed
};

class Response
{
public:
    string http_proto;
    string status_code;
    string status_msg;
    string response_line; // http_proto status_code status_msg

    vector<string> header;
    vector<uint8_t> body;

    int send(int fd);                       //  give a http response, return < 0 means failed
    static Response_ptr recv(Rio_t *rio_t); // return nullptr means failed
};

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int Open_listenfd(char *ip_addr, char *port);
void Close(int fd);

#endif
