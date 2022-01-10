cmake_minimum_required(VERSION 3.16)
project(mphttp-proxy)

set(CMAKE_POSITION_INDEPENDENT_CODE on)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSION off)

add_subdirectory(src/mphttp)

add_executable(proxy 
    src/proxy.cpp
)

target_include_directories(proxy
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/mphttp
)

target_link_libraries(proxy
    PRIVATE
        mphttp
)