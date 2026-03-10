cmake_minimum_required(VERSION 4.1)
project(heap C)

set(CMAKE_C_STANDARD 11)

add_library(heap STATIC library.c)
