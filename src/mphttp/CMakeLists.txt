cmake_minimum_required(VERSION 3.14)

project(mphttp)

set(CMAKE_POSITION_INDEPENDENT_CODE on)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSION off)

add_library(mphttp
    STATIC
        Buffer.cc
        eventloop.cc
        utils.cc
        HttpClient.cc
        HttpRequest.cc
        HttpResponse.cc
        MpHttpClient.cc
        MpTask.cc
        helper.cpp
)

target_include_directories(mphttp
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)