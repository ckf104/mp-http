#include <algorithm>

#include "helper.hpp"
#include "general.hpp"
#include "scheduler.hpp"

void doit(int connfd)
{
    Rio_ptr rio_ptr(new Rio_t(connfd));
    Path servers[path_num];
    // Keep-alive connection
    while (1)
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

/**
 * @param goal number tcp connections which hopes to setup
 * @param domain_name domain_name
 * @param servers result path
 * @return number of built connection
 */
int dns_lookup(Path (&servers)[path_num], int goal, const string &domain_name) {

}

// judge whether to use mp-http for this http request
bool need_mp_path(Request *req){
    return false;
}

Response_ptr send_mp_request(Path (&servers)[path_num], Request *request, vector<uint8_t> &reply_body) {
    exit(-1); 
}

Response_ptr send_normal_request(Path &server, Request *request, vector<uint8_t> &reply_body, const vector<uint8_t> &req_body) {
    if (server.is_closed()) {
        return nullptr;
    }
    request->send(server.rio_ptr, req_body.data(), req_body.size());
}

bool Path::is_closed() const {
    return false;
}