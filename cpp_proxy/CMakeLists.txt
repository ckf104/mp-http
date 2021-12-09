cmake_minimum_required(VERSION 3.10)

project(lab3)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -lpthread -O0 -Wall -Wno-sign-compare -ggdb3 -DZDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -lpthread -O2 -Wall -Wno-sign-compare")

include_directories(PUBLIC "${PROJECT_SOURCE_DIR}")

add_library(helper
    helper.cpp
    scheduler.cpp
)
target_link_libraries(helper PUBLIC  ${PCAP_LIBRARY} Threads::Threads)

add_executable(proxy proxy.cpp)
target_link_libraries(proxy PUBLIC helper)