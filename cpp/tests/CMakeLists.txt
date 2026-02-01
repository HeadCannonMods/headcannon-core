cmake_minimum_required(VERSION 3.20)

# Simple test executable
add_executable(headcannon_tests
    test_main.cpp
    math_tests.cpp
    protocol_tests.cpp
)

target_link_libraries(headcannon_tests PRIVATE headcannon)

add_test(NAME headcannon_tests COMMAND headcannon_tests)
