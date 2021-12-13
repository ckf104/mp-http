#ifndef __HELPER_HPP
#define __HELPER_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>

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

enum class mp_http{
    not_use,
    has_range_header,   // use mp_http
    no_range_header,    // use mp_http
};

struct Body{
    std::unique_ptr<uint8_t[]> content;
    uint64_t length = 0;
};

class Rio_t
{
private:
    //int rio_fd;                    /* Descriptor for this internal buf */
    int rio_cnt;                   /* Unread bytes in internal buf */
    char *rio_bufptr;              /* Next unread byte in internal buf */
    char rio_buf[rio_buffer_size]; /* Internal buffer */

public:
    int rio_fd;    // for convenience :)
    Rio_t(int fd);

    ssize_t rio_readlineb(string &usrbuf);
    ssize_t rio_readnb(Body& buf, size_t n);
    ssize_t rio_read(void *usr_buf, size_t n);
    ssize_t rio_writen(const void *usrbuf, size_t n, int flags = 0);
    operator bool();   // return rio_fd != 0 ?
};

class Request
{
public: 
    string method;
    //string host;
    string url;
    string headline;
    string http_proto = "HTTP/1.1";   // http/1.x
    //string hostname;
    //string path;
    //Includs \r\n in the latter string
    std::map<string,string> headers; // string + \r\n ?
};

class Response
{
public:
    string headline; 
    string version, code, status;
    std::map<string,string> headers;
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


*/

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int Open_listenfd(const char *port);
int Open_listenfd(const char *port);
void Close(int fd);
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags);
int Open_clientfd(const char *hostname, const char *port, uint* latency);
#endif

