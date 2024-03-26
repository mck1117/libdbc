add_library(libdbc STATIC)
set_property(TARGET libdbc PROPERTY COMPILE_WARNING_AS_ERROR ON)
target_compile_options(libdbc PRIVATE -Werror -Wall -Wextra -pedantic)

target_sources(libdbc
    PRIVATE
        src/libdbc.cpp
)

target_include_directories(libdbc
    PUBLIC
        include
)
