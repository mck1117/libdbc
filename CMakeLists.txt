cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(libdbc)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(lib/libdbc)
add_subdirectory(test)
add_subdirectory(lib/googletest)
add_subdirectory(dbcconvert)
