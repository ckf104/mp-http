#include <stdio.h>
#include <iostream>
#include <regex>
#include <map>
#include "csapp.h"
#include <string>
#include <set>
/* Recommended max cache and object sizes */
#define MAX_OBJECT_SIZE 1024000
using std::set;
using std::string;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "close\r\n";
static const char *proxy_hdr = "close\r\n";
extern int h_errno;
extern char **environ;

void doit(int fd);
void send_requesthdrs(int fd, const std::map<string, string> &headers, const char *) ;
void clienterror(int fd, const char *cause, const char *errnum, const char *shortmsg, const char *longmsg);
void parse_uri(char *uri, char *hostname, char *path, int *port);
std::map<string,string> build_requesthdrs(rio_t *rp, char *hostname);
void *Thread(void *vargp);

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

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int client_fd)
{
    int endserver_fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t from_client, to_endserver;
    /*store the request line arguments*/
    char hostname[MAXLINE], path[MAXLINE]; //path eg  /hub/index.html
    int port;

    /* Read request line and headers */
    Rio_readinitb(&from_client, client_fd);

    if (!Rio_readlineb(&from_client, buf, MAXLINE)) return;
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET"))
    {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy Server does not implement this method");
        printf("[Error] Not implemented!.");
        return;
    }
    //parse uri then open a clientfd

    parse_uri(uri, hostname, path, &port);

    char port_str[10];
    sprintf(port_str, "%d", port);
    endserver_fd = Open_clientfd(hostname, port_str);
    if (endserver_fd < 0)
    {
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&to_endserver, endserver_fd);

    auto headers = build_requesthdrs(&from_client, hostname);
    headers["User-Agent"] = user_agent_hdr;
    headers["Connection"] = conn_hdr;
    headers["Proxy-Connection"] = proxy_hdr;
    //TODO:Parse request headers...
    send_requesthdrs(endserver_fd, headers, path);

    int n, end = 0, sum = 0;
    char data[MAX_OBJECT_SIZE];
    while ((n = Rio_readlineb(&to_endserver, buf, MAXLINE))) {//real server response to buf
        if (end) sum += n;
        if (!strcmp(buf, "\r\n")) end = 1;
        Rio_writen(client_fd, buf, n);  //real server response to real client
    }
    Close(endserver_fd);
}

// returns an error message to the client
void clienterror(int fd, const char *cause, const char *errnum,
                 const char *shortmsg, const char *longmsg) {
    char buf[MAXLINE], body[MAXBUF], *p = body;
    p += snprintf(p, MAXBUF-(p-body), "<html><title>Proxy Error</title>");
    p += snprintf(p, MAXBUF-(p-body), "%s<body bgcolor=ffffff>\r\n", body);
    p += snprintf(p, MAXBUF-(p-body), "%s%s: %s\r\n", body, errnum, shortmsg);
    p += snprintf(p, MAXBUF-(p-body), "%s<p>%s: %s\r\n", body, longmsg, cause);
    p += snprintf(p, MAXBUF-(p-body), "%s<hr><em>The Proxy Web server</em>\r\n", body);
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
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
        *pos2 = '\0'; //pos1 www.cmu.edu/08080/hub/index.html
        strncpy(hostname, pos1, MAXLINE);
        sscanf(pos2 + 1, "%d%s", port, path); //pos2+1 8080/hub/index.html
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

std::map<string,string> build_requesthdrs(rio_t *rp, char *hostname) {
    char buf[MAXLINE];
    std::map<string, string> headers;
    while (Rio_readlineb(rp, buf, MAXLINE) > 0)
    {
        if (!strcmp(buf, "\r\n")) break;
        int p, len = strlen(buf);
        for (p = 0; p < len; ++p) 
            if (buf[p] == ':') break;
        if (p < len) {
            string a(buf, buf + p), b(buf + p + 1, buf + len); 
            headers[a] = b;
            std::cout << a << " " << b << std::endl;
        }
    }
    return headers;
}

void send_requesthdrs(int fd, const std::map<string, string> &headers, const char *path) {
    char buf[MAXLINE];
    sprintf(buf, "GET %s HTTP/1.1\r\n", path);
    Rio_writen(fd, buf, strlen(buf));
    for (auto x : headers) {
        if (x.first == "Host")
            sprintf(buf, " %s: %s", x.first.c_str(), x.second.c_str());
        else 
            sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
        Rio_writen(fd, buf, strlen(buf));
    }
    sprintf(buf, "\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
