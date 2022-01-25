#ifndef __GENERAL_HPP
#define __GENERAL_HPP
#include <cstdint>

constexpr int listen_queue_len = 30;
constexpr int rio_buffer_size = 8192;
constexpr int path_num = 2;
constexpr int init_str_len = 64;
constexpr int max_line = 500;
constexpr uint64_t threshold = 100 * 1024; // 100 kB
constexpr uint64_t new_req_threshold = 10 * 1024;

#endif