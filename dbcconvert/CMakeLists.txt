add_executable(dbcconvert)
set_property(TARGET dbcconvert PROPERTY COMPILE_WARNING_AS_ERROR ON)

if (WIN32)

else()
    target_compile_options(dbcconvert PRIVATE -Werror -Wall -Wextra -pedantic)
endif()

target_sources(dbcconvert
    PRIVATE
        dbcconvert.cpp
)

target_link_libraries(dbcconvert libdbc)
