add_executable(tuican)
set_property(TARGET tuican PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_compile_options(tuican PRIVATE -Werror -Wall -Wextra -pedantic)

target_sources(tuican
    PRIVATE
        tuican.cpp
        terminal.cpp
        socketcan.cpp
)

target_link_libraries(tuican libdbc)
