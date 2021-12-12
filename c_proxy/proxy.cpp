#include <stdio.h>
#include <future>
#include <iostream>
#include <regex>
#include <map>
#include "helper.hpp"
#include "general.hpp"
#include <string>
#include <memory>
#include <set>
#include <optional>
#define MAX_OBJECT_SIZE 1024000
using std::set;
using std::string;
using std::map; 

static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "close\r\n";
static const char *proxy_hdr = "close\r\n";
extern char **environ;

void doit(int fd);
void clienterror(int fd, const char *cause, const char *errnum, const char *shortmsg, const char *longmsg);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void *Thread(void *vargp);

struct HttpStream; 
using StreamPtr = std::unique_ptr<HttpStream>; 
struct HttpStream {
public:
    Rio_t fd;
    string hostname;
    int port;
    HttpStream(int _fd) : fd(_fd) { }
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
    /**
     * @brief Get the Request object
     * 
     * @return int 0 means success
     */
    std::optional<Request> getRequest() {
        char _method[max_line], _uri[max_line], _version[max_line];
        char _hostname[max_line], _path[max_line];
        string buf;
        if (!fd.rio_readlineb(buf)) {
            return std::nullopt;
        }
        sscanf(buf.c_str(), "%s %s %s", _method, _uri, _version);
        Request r;
        r.method = string(_method);
        r.url = string(_uri);
        r.http_proto = string(_version);
        parse_uri(_uri, _hostname, _path, &port);
        r.hostname = hostname = string(_hostname);
        r.path = string(_path);
        if (r.method != "GET")
        {
            //printf("[Error] Not implemented!.\n");
            return std::nullopt;
        }
        std::cout << r.url << " " << r.method << std::endl;
        build_requesthdrs(r.headers);
        return std::make_optional(r);
    }
    std::optional<Response> getResponse() {
        Response r;
        int n; string buf;
        n = fd.rio_readlineb(r.headline); 
        char ver[max_line], code[max_line], status[max_line];
        sscanf(r.headline.c_str(), "%s%s%s", ver, code, status);
        r.version = string(ver); 
        r.code = string(code); 
        r.status = string(status); 

        while ((n = fd.rio_readlineb(buf)) > 0)
        {
            if (buf == "\r\n") break;
            int p;
            ssize_t len = buf.size();
            for (p = 0; p < len; ++p)
                if (buf[p] == ':')
                    break;
            if (p < len)
            //TODO:Not robust
                r.headers[buf.substr(0, p)] = buf.substr(p + 2, len);
        }
        return std::make_optional(r);
    }
    void build_requesthdrs(map<string,string> &headers)
    {
        headers.clear();
        string buf;
        while (fd.rio_readlineb(buf) > 0) {
            if (buf == "\r\n") break;
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
    static void sendReqeust(int fd, const Request &r) {
        char buf[max_line];
        sprintf(buf, "GET %s HTTP/1.1\r\n", r.path.c_str());
        Rio_t w(fd);
        w.rio_writen((void*)buf, strlen(buf));
        for (auto x : r.headers)
        {
            /*
            if (x.first == "Host")
                sprintf(buf, " %s: %s", x.first.c_str(), x.second.c_str());
            else
                sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            */
            sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            w.rio_writen((void *)buf, strlen(buf));
        }
        sprintf(buf, "\r\n");
        w.rio_writen((void*)buf, strlen(buf));
    }
    static void sendResponse(int fd, const Response &r) {
        char buf[max_line];
        Rio_t w(fd);
        w.rio_writen((void *)r.headline.c_str(), r.headline.size());
        for (auto x : r.headers)
        {
            sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            w.rio_writen((void *)buf, strlen(buf));
        }
        sprintf(buf, "\r\n");
        w.rio_writen((void*)buf, strlen(buf));
    }
};

int main(int argc, char **argv)
{
    int listenfd, *connfd;
    pthread_t tid;
    char hostname[max_line], port[max_line];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = (int*)malloc(sizeof(int));
        *connfd = Accept(listenfd, (sockaddr *)&clientaddr, &clientlen);

        Getnameinfo((sockaddr *)&clientaddr, clientlen, hostname, max_line,
                    port, max_line, 0);
        printf("Accepted connection from (%s:%s)\n", hostname, port);
        pthread_create(&tid, NULL, Thread, connfd);
    }
    Close(listenfd);
    return 0;
}

/* Thread routine */
void *Thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int client_fd)
{
    int endserver_fd;
    auto clientStream = HttpStream::generateStream(client_fd);
    if (!clientStream) {
        printf("[Err] Connection failed getStream\n");
        return;
    }
    auto req_ptr = clientStream->getRequest(); 
    if (!req_ptr) {
        printf("[Err] Connection failed at get reqeust.\n");
        return;
    }
    auto req = req_ptr.value();
    endserver_fd = Open_clientfd(clientStream->hostname.c_str(), std::to_string(clientStream->port).c_str());
    if (endserver_fd < 0)
    {
        printf("[Err] Connection failed at open endserver_fd\n");
        return;
    }
    auto serverStream = HttpStream::generateStream(endserver_fd);
    req.headers["User-Agent"] = user_agent_hdr;
    req.headers["Connection"] = conn_hdr;
    req.headers["Proxy-Connection"] = proxy_hdr;
    HttpStream::sendReqeust(endserver_fd, req);

    auto r = serverStream->getResponse().value(); 
    HttpStream::sendResponse(client_fd, r);
    int n; string buf;
    while ((n = serverStream->fd.rio_readlineb(buf)))
    {
        clientStream->fd.rio_writen((void *)buf.c_str(), n);
    }
    Close(endserver_fd);
}

void parse_uri(char *uri, char *hostname, char *path, int *port)
{
    *port = 80;
    char *pos1 = strstr(uri, "//");
    if (pos1 == NULL)
        pos1 = uri;
    else
        pos1 += 2;
    char *pos2 = strstr(pos1, ":");
    if (pos2 != NULL) {
        *pos2 = '\0'; 
        strncpy(hostname, pos1, max_line);
        sscanf(pos2 + 1, "%d%s", port, path); 
        *pos2 = ':';
    }
    else {
        pos2 = strstr(pos1, "/"); //pos2 /hub/index.html
        if (pos2 == NULL)
        { /*pos1 www.cmu.edu*/
            strncpy(hostname, pos1, max_line);
            strcpy(path, "");
            return;
        }
        *pos2 = '\0';
        strncpy(hostname, pos1, max_line);
        *pos2 = '/';
        strncpy(path, pos2, max_line);
    }
}
