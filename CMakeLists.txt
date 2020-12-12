cmake_minimum_required(VERSION 3.17)
project(MySocket C)

set(CMAKE_C_STANDARD 99)

add_executable(server src/server.c)
add_executable(client src/client.c)
