cmake_minimum_required(VERSION 3.14)
project(lang CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_NDEBUG OFF)
if(ENABLE_NDEBUG)
  add_compile_definitions(NDEBUG)
endif()

add_compile_options(-Wall -Wextra)

find_package(GTest REQUIRED)
enable_testing()
include(GoogleTest)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

set(FRONT_DIR ${SRC_DIR}/front_end)
set(INTERPRETER_DIR ${SRC_DIR}/interpreter)

include_directories(${INCLUDE_DIR} ${INCLUDE_DIR}/front_end ${INCLUDE_DIR}/interpreter)

add_subdirectory(tests)

add_library(front ${FRONT_DIR}/lexer.cpp ${FRONT_DIR}/parser.cpp)
add_library(dump ${FRONT_DIR}/dump_tree.cpp)
add_library(interpreter ${INTERPRETER_DIR}/interpreter.cpp)

# add_executable(front_with_dump ${SRC_DIR}/main.cpp)
# target_link_libraries(front_with_dump PRIVATE front dump interpreter)

add_executable(interp ${SRC_DIR}/main.cpp)
target_link_libraries(interp PRIVATE front interpreter dump)
