cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM 9 REQUIRED CONFIG)
find_program(CLANG_PATH clang)
find_program(CLANGXX_PATH clang++)

execute_process(
    COMMAND noelle-config --prefix
    OUTPUT_VARIABLE NOELLE_INSTALL_DIR
    RESULT_VARIABLE return_code
    ERROR_QUIET
)

if(return_code AND (NOT return_code EQUAL 1))
  message(FATAL_ERROR "Please source NOELLE. 'noelle-config' must be available")
else()
  string(STRIP ${NOELLE_INSTALL_DIR} NOELLE_INSTALL_DIR)
  message(STATUS "Found NOELLE at: " ${NOELLE_INSTALL_DIR})
endif()

set(CMAKE_C_COMPILER ${CLANG_PATH})
set(CMAKE_CXX_COMPILER ${CLANGXX_PATH})
set(CMAKE_ASM_COMPILER ${CLANG_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

configure_file(enable.in ${CMAKE_SOURCE_DIR}/enable @ONLY)

add_subdirectory(compiler)
