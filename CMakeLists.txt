cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(libdbc)

set(CMAKE_CXX_STANDARD 20)

enable_testing()

add_subdirectory(lib/libdbc)
add_subdirectory(test)
add_subdirectory(lib/googletest)
add_subdirectory(dbcconvert)

# SocketCAN is Linux only
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(tuican)
endif()
