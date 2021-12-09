#include <thread>

#include <sys/socket.h>
#include "general.hpp"
#include "helper.hpp"
#include "scheduler.hpp"

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t  clientlen;
    char hostname[max_line], port[max_line];

    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    if(argc != 3) {
        fprintf(stderr,"usage :%s <addr> <port> \n",argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1], argv[2]);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(sockaddr *)&clientaddr, &clientlen);

        std::thread t(doit, connfd);
        t.detach();
    }
    return 0;
}