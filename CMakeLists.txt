cmake_minimum_required(VERSION 3.13)
project(arcdps-timer-server)

set(CMAKE_CXX_STANDARD 23)

add_executable(arcdps-timer-server main.cpp group.cpp server.cpp session.cpp)
