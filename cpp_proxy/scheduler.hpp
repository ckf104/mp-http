#ifndef __SCHEDULER_HPP
#define __SCHEDULER_HPP

#include <unordered_map>
#include <vector>
#include "helper.hpp"

using std::vector;
using Domain_Map = std::unordered_map<string, Path[path_num]>;

void doit(int connfd);

/**
 * @param goal number tcp connections which hopes to setup
 * @param domain_name domain_name
 * @param servers result path
 * @return number of built connection
 */

int dns_lookup(Path (&servers)[path_num], int goal, const string &domain_name); 
// judge whether to use mp-http for this http request
bool need_mp_path(Request *req);

Response_ptr send_mp_request(Path (&servers)[path_num], Request *request, vector<uint8_t> &reply_body);

Response_ptr send_normal_request(Path &server, Request *request, vector<uint8_t> &reply_body, const vector<uint8_t> &req_body) ;

#endif