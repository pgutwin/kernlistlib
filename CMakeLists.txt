cmake_minimum_required(VERSION 3.20)
project(verilog_parser_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
  pegtl
  GIT_REPOSITORY https://github.com/taocpp/PEGTL.git
  GIT_TAG 3.2.7
)
FetchContent_MakeAvailable(pegtl)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(veriloglib
  src/veriloglib.cpp
  src/verilog_builder.cpp
  src/verilog_writer.cpp
  src/verilog_validator.cpp
  src/verilog_checker.cpp
  src/verilog_groups.cpp
)
target_include_directories(veriloglib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(veriloglib PUBLIC taocpp::pegtl)

add_executable(vparse src/main.cpp)
target_link_libraries(vparse PRIVATE veriloglib)

enable_testing()
add_executable(verilog_tests tests/test_verilog.cpp tests/test_kernlists.cpp)
target_compile_definitions(verilog_tests PRIVATE KERNLISTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/kernlists")
target_link_libraries(verilog_tests PRIVATE veriloglib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(verilog_tests)
