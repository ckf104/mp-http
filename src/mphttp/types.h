#ifndef TYPES_H
#define TYPES_H

#include <chrono>
#include <cstdint>

using timestamp_t = std::chrono::time_point<std::chrono::high_resolution_clock>;
using taskid_t = std::size_t;

#endif