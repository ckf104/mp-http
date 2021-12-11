#include <stdio.h>
#include <iostream>
#include <regex>
#include <map>
#include "csapp.h"
#include <string>
#include <memory>
#include <set>
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
    map<string, string> headers;
    rio_t fd;
    string method;
    string uri; 
    string version;
    string hostname; 
    string path;
    int port;
    /**
     * @brief factory method for HttpStream
     * 
     * @param _fd file descriptor of the stream
     * @return if this function returns null, the stream is closed.
     */
    static StreamPtr generateStream(int _fd) {
        auto r = std::make_unique<HttpStream>();
        Rio_readinitb(&r->fd, _fd);
        return r;
    }
    /**
     * @brief Get the Request object
     * 
     * @return int 0 means success
     */
    int getRequest() {
        char buf[MAXLINE], _method[MAXLINE], _uri[MAXLINE], _version[MAXLINE];
        char _hostname[MAXLINE], _path[MAXLINE];
        if (!Rio_readlineb(&fd, buf, MAXLINE))
        {
            return 1;
        }
        sscanf(buf, "%s %s %s", _method, _uri, _version);
        method = string(_method);
        uri = string(_uri);
        version = string(_version);
        parse_uri(_uri, _hostname, _path, &port);
        hostname = string(_hostname);
        path = string(_path);
        if (method != "GET")
        {
            printf("[Error] Not implemented!.");
            return 1;
        }
        build_requesthdrs();
        return 0;
    }
    void build_requesthdrs()
    {
        //TODO:update it
        headers.clear();
        char buf[MAXLINE];
        while (Rio_readlineb(&fd, buf, MAXLINE) > 0)
        {
            if (!strcmp(buf, "\r\n"))
                break;
            int p, len = strlen(buf);
            for (p = 0; p < len; ++p)
                if (buf[p] == ':')
                    break;
            if (p < len)
            {
                string a(buf, buf + p), b(buf + p + 1, buf + len);
                headers[a] = b;
            }
        }
    }
    static void send_requesthdrs(int fd, const std::map<string, string> &headers, const char *path)
    {
        char buf[MAXLINE];
        sprintf(buf, "GET %s HTTP/1.1\r\n", path);
        Rio_writen(fd, buf, strlen(buf));
        for (auto x : headers)
        {
            if (x.first == "Host")
                sprintf(buf, " %s: %s", x.first.c_str(), x.second.c_str());
            else
                sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            Rio_writen(fd, buf, strlen(buf));
        }
        sprintf(buf, "\r\n");
        Rio_writen(fd, buf, strlen(buf));
    }
};

int main(int argc, char **argv)
{
    int listenfd, *connfd;
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = (int*)Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s:%s)\n", hostname, port);
        Pthread_create(&tid, NULL, Thread, connfd);
    }
    Close(listenfd);
    return 0;
}

/* Thread routine */
void *Thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int client_fd)
{
    int endserver_fd;
    rio_t to_endserver;
    auto clientStream = HttpStream::generateStream(client_fd);
    if (!clientStream) {
        printf("[Err] Connection failed getStream\n");
        return;
    }
    int r = clientStream->getRequest(); 
    endserver_fd = Open_clientfd(clientStream->hostname.c_str(), std::to_string(clientStream->port).c_str());
    if (endserver_fd < 0)
    {
        printf("[Err] Connection failed at open endserver_fd\n");
        return;
    }
    Rio_readinitb(&to_endserver, endserver_fd);
    if (r < 0) {
        printf("[Err] Connection failed at get reqeust.\n");
    }
    clientStream->headers["User-Agent"] = user_agent_hdr;
    clientStream->headers["Connection"] = conn_hdr;
    clientStream->headers["Proxy-Connection"] = proxy_hdr;
    printf("Headers built. 1.2\n"); 
    HttpStream::send_requesthdrs(endserver_fd, clientStream->headers, clientStream->path.c_str());

    printf("Headers built. 2\n"); 
    int n, end = 0, sum = 0;
    char data[MAX_OBJECT_SIZE], buf[MAXLINE];
    while ((n = Rio_readlineb(&to_endserver, buf, MAXLINE))) {//real server response to buf
        if (end) sum += n;
        if (!strcmp(buf, "\r\n")) end = 1;
        Rio_writen(client_fd, buf, n);  //real server response to real client
    }
    printf("Headers built. 3\n"); 
    Close(endserver_fd);
}

void parse_uri(char *uri, char *hostname, char *path, int *port) {
    *port = 80;
    char *pos1 = strstr(uri, "//");
    if (pos1 == NULL)
        pos1 = uri;
    else
        pos1 += 2;
    char *pos2 = strstr(pos1, ":");
    if (pos2 != NULL) {
        *pos2 = '\0'; 
        strncpy(hostname, pos1, MAXLINE);
        sscanf(pos2 + 1, "%d%s", port, path); 
        *pos2 = ':';
    }
    else {
        pos2 = strstr(pos1, "/"); //pos2 /hub/index.html
        if (pos2 == NULL)
        { /*pos1 www.cmu.edu*/
            strncpy(hostname, pos1, MAXLINE);
            strcpy(path, "");
            return;
        }
        *pos2 = '\0';
        strncpy(hostname, pos1, MAXLINE);
        *pos2 = '/';
        strncpy(path, pos2, MAXLINE);
    }
}

