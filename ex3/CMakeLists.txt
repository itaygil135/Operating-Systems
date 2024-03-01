cmake_minimum_required(VERSION 3.25)
project(ex3)

set(CMAKE_CXX_STANDARD 17)

add_executable(ex3 MapReduceFramework.cpp Barrier.cpp Barrier.h  SampleClient.cpp SampleClient.cpp)
