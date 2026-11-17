add_executable(unit_tests
    lexer_test.cpp
    parser_test.cpp
)

target_link_libraries(unit_tests PRIVATE
    front
    GTest::gtest
    GTest::gtest_main
)

add_executable(interpreter_unit_tests
    test_interpreter.cpp
)

target_link_libraries(interpreter_unit_tests PRIVATE
    interpreter
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(unit_tests)
gtest_discover_tests(interpreter_unit_tests)
