#include "helper.hpp"
#include "general.hpp"
#include "scheduler.hpp"

void doit(int connfd)
{
    Rio_ptr rio_ptr(new Rio_t(connfd));
    Domain_Map domain_map;

    while (1)    // keep-alive connection
    {
        Request_ptr request_ptr = Request::recv(rio_ptr.get());
        if (!request_ptr)
            goto RET;
        if (dns_lookup(domain_map) < 0)
            goto RET;
        Response_ptr response;
        if (need_mp_path(request_ptr.get())){    
            response = send_mp_request(domain_map, request_ptr.get());
            if(!response)
                goto RET;
        }
        else{
            response = send_normal_request(domain_map, request_ptr.get());
            if(!response)
                goto RET;
        }
        if(response->send(connfd) < 0)
            goto RET;      
    }

RET:
    // may need send error message ?
    Close(connfd);
}