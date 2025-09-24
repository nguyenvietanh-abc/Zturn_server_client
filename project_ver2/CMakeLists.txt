cmake_minimum_required(VERSION 3.10)
project(ZturnProject)

set(CMAKE_CXX_STANDARD 11)

include_directories(include)

add_executable(client
    sources/Client.cpp
    sources/main_client.cpp
)

add_executable(server
    sources/Server.cpp
    sources/main_server.cpp
)

target_link_libraries(client pthread)
target_link_libraries(server pthread)
