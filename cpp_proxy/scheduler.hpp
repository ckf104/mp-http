#ifndef __SCHEDULER_HPP
#define __SCHEDULER_HPP

#include <unordered_map>
#include "helper.hpp"

using Domain_Map = std::unordered_map<string, Path[path_num]>;

void doit(int connfd);
int dns_lookup(Domain_Map& domain_map);
int need_mp_path(Request* req);   // judge whether use mp-http for this http request
Response_ptr send_mp_request(Domain_Map& domain_map, Request* request);
Response_ptr send_normal_request(Domain_Map& domain_map, Request* request);

#endif