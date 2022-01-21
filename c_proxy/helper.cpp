#include <type_traits>

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <chrono>
#include <iostream>

#include "general.hpp"
#include "helper.hpp"

ssize_t Rio_t::rio_read(void *usrbuf, size_t n)
{
    int cnt;

    while (rio_cnt <= 0)
    { /* Refill if buf is empty */
        rio_cnt = read(rio_fd, rio_buf,
                       sizeof(rio_buf));
        if (rio_cnt < 0)
        {
            if (errno != EINTR) /* Interrupted by sig handler return */
                return -1;
        }
        else if (rio_cnt == 0) /* EOF */
            return 0;
        else
            rio_bufptr = rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rio_cnt < (int)n)
        cnt = rio_cnt;
    memcpy(usrbuf, rio_bufptr, cnt);
    rio_bufptr += cnt;
    rio_cnt -= cnt;
    return cnt;
}

ssize_t Rio_t::rio_readnb(Body &usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    usrbuf.content.reset(new uint8_t[n]);
    usrbuf.length = n;
    uint8_t *bufp = usrbuf.content.get();

    while (nleft > 0)
    {
        if ((nread = rio_read(bufp, nleft)) < 0)
            return -1; /* errno set by read() */
        else if (nread == 0)
            break; /* EOF */
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft); /* return >= 0 */
}

ssize_t Rio_t::rio_readlineb(string &usrbuf)
{
    usrbuf.clear();
    int rc;
    char c;
    while (true)
    {
        if ((rc = rio_read(&c, 1)) == 1)
        {
            usrbuf += c;
            if (c == '\n')
                return usrbuf.size();
        }
        else if (rc == 0)
        {
            return usrbuf.size();
        }
        else
        {
            return -1;
        }
    }
}

// don't use rio buffer
ssize_t Rio_t::rio_writen(const void *usrbuf, size_t n, int flags)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = (char *)usrbuf;

    while (nleft > 0)
    {
        if ((nwritten = send(rio_fd, bufp, nleft, flags | MSG_NOSIGNAL)) <= 0)
        {
            if (errno == EINTR) /* Interrupted by sig handler return */
                nwritten = 0;   /* and call write() again */
            else
                return -1; /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

Rio_t::operator bool()
{
    return rio_fd != 0;
}

void unix_error(const char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;
    if ((rc = getnameinfo(sa, salen, host, hostlen, serv,
                          servlen, flags)) != 0)
        unix_error("Getnameinfo error");
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
        unix_error("Accept error");
    return rc;
}

void Close(int fd)
{
    int rc;

    if ((rc = close(fd)) < 0)
        unix_error("Close error");
}

int Open_listenfd(const char *ip_addr, const char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Accept connections */
    hints.ai_flags = AI_ADDRCONFIG;   /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV; /* ... using port number */
    if ((rc = getaddrinfo(ip_addr, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Eliminates "Address already in use" error from bind */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, //line:netp:csapp:setsockopt
                   (const void *)&optval, sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* Success */
        if (close(listenfd) < 0)
        { /* Bind failed, try the next */
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* No address worked */
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, listen_queue_len) < 0)
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

// use latency to record time of three-handshakes
int Open_clientfd(const char *hostname, const char *port, uint* latency, uint* ip_addr)
{
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV; /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG; /* Recommended for connections */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */      
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Connect to the server */
        auto now = std::chrono::system_clock::now();
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1){
            *ip_addr = ((sockaddr_in*)p->ai_addr)->sin_addr.s_addr;                      
            break; /* Success */
        }
        *latency = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - now).count();
        if (close(clientfd) < 0)
        { /* Connect failed, try another */ //line:netp:openclientfd:closefd
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else /* The last connect succeeded */
        return clientfd;
}

/*  
 * open_listenfd - Open and return a listening socket on port. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns: 
 *       -2 for getaddrinfo error
 *       -1 with errno set for other errors.
 */
int Open_listenfd(const char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Eliminates "Address already in use" error from bind */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, //line:netp:csapp:setsockopt
                   (const void *)&optval, sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* Success */
        if (close(listenfd) < 0)
        { /* Bind failed, try the next */
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* No address worked */
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, 1024) < 0)
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

Rio_t::Rio_t(int x)
{
    rio_fd = x;
    rio_cnt = 0;
}
size_t Rio_t::rio_get_rest(){
    return rio_cnt;
}

ssize_t writen(int rio_fd, const void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = (char *)usrbuf;

    while (nleft > 0)
    {
        if ((nwritten = send(rio_fd, bufp, nleft, MSG_NOSIGNAL)) <= 0)
        {
            if (errno == EINTR) /* Interrupted by sig handler return */
                nwritten = 0;   /* and call write() again */
            else
                return -1; /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}