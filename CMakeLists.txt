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

# Python bindings (KL19): the module kernlist. Needs Python 3 with its development files.
option(KERNLISTLIB_PYTHON "Build the Python bindings" ON)
if(KERNLISTLIB_PYTHON)
  set(PYBIND11_FINDPYTHON ON)
  # The container names its interpreter (KERNLISTLIB_PYTHON_EXECUTABLE), so that a Python found
  # first on the PATH passed in from the host, perhaps without its development files, is not used.
  # -DPython_EXECUTABLE=... on the command line still takes precedence.
  if(NOT Python_EXECUTABLE AND DEFINED ENV{KERNLISTLIB_PYTHON_EXECUTABLE})
    set(Python_EXECUTABLE "$ENV{KERNLISTLIB_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python for the bindings")
  endif()
  find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)
  message(STATUS "Python bindings use ${Python_EXECUTABLE} (${Python_VERSION})")
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6
  )
  FetchContent_MakeAvailable(pybind11)
  pybind11_add_module(kernlist python/kernlist_module.cpp)
  target_link_libraries(kernlist PRIVATE veriloglib)
  add_test(NAME python_bindings
           COMMAND ${Python_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/python/tests -v)
  set_tests_properties(python_bindings PROPERTIES
           ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:kernlist>;KERNLISTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/kernlists")
endif()
