cmake_minimum_required(VERSION 3.20)
project(reverse_shell)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-lutil")

add_executable(reverse_shell main.cpp)

target_link_libraries(reverse_shell -lutil)
