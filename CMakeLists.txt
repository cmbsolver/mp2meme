cmake_minimum_required(VERSION 3.10)
project(mp2meme CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mp2meme src/main.cpp)

install(TARGETS mp2meme DESTINATION bin)
