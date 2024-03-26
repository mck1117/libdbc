add_executable(libdbc_test)
set_property(TARGET libdbc_test PROPERTY COMPILE_WARNING_AS_ERROR ON)
target_compile_options(libdbc_test PRIVATE -Werror -Wall -Wextra -pedantic)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

target_sources(libdbc_test
    PRIVATE
        TestParsing.cpp
        TestBitExtraction.cpp
        TestDataDecode.cpp
)

target_link_libraries(libdbc_test libdbc gmock_main)
