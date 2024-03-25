add_executable(libdbc_test)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

target_sources(libdbc_test
    PRIVATE
        TestParsing.cpp
        TestBitExtraction.cpp
        TestDataDecode.cpp
)

target_link_libraries(libdbc_test libdbc gtest gtest_main)
