cmake_minimum_required(VERSION 3.13)

#project(compiler C CXX ASM)

set(NOELLE_HOME $ENV{NOELLE_HOME})
if(NOT NOELLE_HOME)
  message(FATAL_ERROR "Please set the environment variable NOELLE_HOME")
endif()
message(STATUS "NOELLE: " ${NOELLE_HOME})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_PATH clang)
find_program(CLANGXX_PATH clang++)

set(CMAKE_C_COMPILER ${CLANG_PATH})
set(CMAKE_CXX_COMPILER ${CLANGXX_PATH})
set(CMAKE_ASM_COMPILER ${CLANG_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM 9 REQUIRED CONFIG)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

include_directories(
  ${LLVM_INCLUDE_DIRS}
  ${NOELLE_HOME}/install/include
  ${NOELLE_HOME}/install/include/svf)

configure_file(dt-opt.in dt-opt @ONLY)
configure_file(enable.in enable @ONLY)

add_subdirectory(printer)
