cmake_minimum_required(VERSION 3.7)
project(opcuatest)

set(CMAKE_CXX_STANDARD 14)

add_executable(opcuatest main.cpp open62541.cpp)