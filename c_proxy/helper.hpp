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
using Rio_ptr = std::shared_ptr<Rio_t>;

class Rio_t
{
private:
    int rio_fd;                    /* Descriptor for this internal buf */
    int rio_cnt;                   /* Unread bytes in internal buf */
    char *rio_bufptr;              /* Next unread byte in internal buf */
    char rio_buf[rio_buffer_size]; /* Internal buffer */

public:
    Rio_t(int fd);

    ssize_t rio_readlineb(string &usrbuf);
    ssize_t rio_readnb(vector<uint8_t> &usrbuf, size_t n);
    ssize_t rio_read(void *usr_buf, size_t n);
    ssize_t rio_writen(const void *usrbuf, size_t n, int flags = 0);
};

/*
class Path
{
public:
    string ip_addr;       
    Rio_ptr rio_ptr;
    int latency;
    int bandwidth;
    bool closed;  // corresponding socket has been closed ? 

    Path(const string &addr, int fd, int init_latency);
    Path();
    void close();   // close tcp socket in this path
    bool is_closed() const;
};

class Request
{
public: 
    string method;
    string host;
    string url;
    string http_proto = "HTTP/1.1";   // http/1.x
    string request_line; // method url http_proto

    vector<string> header; // string + \r\n ?

    int send(Rio_ptr p, const uint8_t* body, int body_len) ;   // send a http request, return < 0 means failed
    static Request_ptr recv(Rio_t *rio_t, vector<uint8_t>& req_body);    // return nullptr means failed
};

class Response
{
public:
    string http_proto;
    string status_code;
    string status_msg;
    string response_line; // http_proto status_code status_msg

    vector<string> header;
    //vector<uint8_t> body;

    //  give a http response, return < 0 means failure
    int send(int fd, const uint8_t* body, int body_len) const; 
    static Response_ptr recv(Rio_t *rio_t); // return nullptr means failed
};
*/

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int Open_listenfd(const char *port);
int Open_listenfd(const char *port);
void Close(int fd);
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags);
int Open_clientfd(const char *hostname, const char *port);
#endif

