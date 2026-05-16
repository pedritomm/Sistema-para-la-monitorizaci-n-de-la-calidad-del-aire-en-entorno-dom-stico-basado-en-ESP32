cmake_minimum_required(VERSION 3.16.0)
project(Analizador)

set(CMAKE_C_STANDARD 11)

include_directories(include)

file(GLOB SOURCES "src/*.c")