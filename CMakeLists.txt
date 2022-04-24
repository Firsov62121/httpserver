cmake_minimum_required(VERSION 3.8)

set(This final)

project(${This} C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS " -std=c++11 -pthread")
add_executable(final main.cpp)