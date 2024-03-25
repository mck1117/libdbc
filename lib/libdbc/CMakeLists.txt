add_library(libdbc STATIC)
target_sources(libdbc
    PRIVATE
        src/libdbc.cpp
)

target_include_directories(libdbc
    PUBLIC
        include
)
