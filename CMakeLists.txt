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

include_directories(${INCLUDE_DIR})

set (FRONT_SRC
    ${SRC_DIR}/lexer.cpp
    ${SRC_DIR}/parser.cpp
)

add_library(front ${FRONT_SRC})
target_include_directories(front PUBLIC ${INCLUDE_DIR})

add_executable(front_with_dump ${SRC_DIR}/main.cpp ${SRC_DIR}/dump_tree.cpp)
target_link_libraries(front_with_dump PRIVATE front)


add_subdirectory(tests)
