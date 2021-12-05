#include <algorithm>

#include "helper.hpp"
#include "general.hpp"
#include "scheduler.hpp"

void doit(int connfd)
{
    Rio_ptr rio_ptr(new Rio_t(connfd));
    Path servers[path_num];

    while (1) // keep-alive connection
    {
        vector<uint8_t> req_body, reply_body;
        Response_ptr response;
        Request_ptr request_ptr = Request::recv(rio_ptr.get(), req_body);
        if (!request_ptr)
            goto RET;

        if (need_mp_path(request_ptr.get()) && dns_lookup(servers, path_num, request_ptr->host) == path_num)
        {
            response = send_mp_request(servers, request_ptr.get(), reply_body); // assert req_body.empty() == true
            if (!response)
                goto RET;
        }
        else if (dns_lookup(servers, 1, request_ptr->host) == 1)
        {
            Path *active_server = std::find_if(servers, servers + path_num, [](const Path &server)
                                               { return !server.is_closed(); });
            response = send_normal_request(*active_server, request_ptr.get(), reply_body, req_body);
            if (!response)
                goto RET;
        }
        else
            goto RET;

        if (response->send(connfd, reply_body.data(), reply_body.size()) < 0)
            goto RET;
    }

RET:
    // may need send error message ?
    Close(connfd);
}