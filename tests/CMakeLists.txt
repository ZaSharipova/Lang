find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(unit_tests
    lexer_test.cpp
    parser_test.cpp
)

target_link_libraries(unit_tests PRIVATE
    front
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(unit_tests)