add_library(libdbc STATIC)
set_property(TARGET libdbc PROPERTY COMPILE_WARNING_AS_ERROR ON)


if (WIN32)

else()
    target_compile_options(libdbc PRIVATE -Werror -Wall -Wextra -pedantic)
endif()

target_sources(libdbc
    PRIVATE
        src/libdbc.cpp
)

target_include_directories(libdbc
    PUBLIC
        include
)
