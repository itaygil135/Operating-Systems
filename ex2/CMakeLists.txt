cmake_minimum_required(VERSION 3.25)
project(ex2)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ex2
        main.cpp
        uthreads.cpp
        uthreads.h)
