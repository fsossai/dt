cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

set(CMAKE_CXX_STANDARD 17)
set(DT_CMAKE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM 14 REQUIRED CONFIG)
find_program(CLANG_PATH clang)
find_program(CLANGXX_PATH clang++)

execute_process(
  COMMAND noelle-config --prefix
  OUTPUT_VARIABLE NOELLE_INSTALL_DIR
  RESULT_VARIABLE return_code
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
)
set(GINO_INSTALL_DIR "/nfs-scratch/fsv1684/gino-dt-14/install")

if(return_code AND (NOT return_code EQUAL 1))
  message(FATAL_ERROR "Please source NOELLE. 'noelle-config' must be available")
else()
  message(STATUS "Found NOELLE at: " ${NOELLE_INSTALL_DIR})
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

get_filename_component(DT_CONFIG_PREFIX ${CMAKE_INSTALL_PREFIX} ABSOLUTE)
set(DT_CONFIG_VERSION 0.1.0)
set(DT_CONFIG_LIB ${DT_CONFIG_PREFIX}/lib)
set(DT_CONFIG_BIN ${DT_CONFIG_PREFIX}/bin)
set(DT_CONFIG_INCLUDE ${DT_CONFIG_PREFIX}/include)

execute_process(
  COMMAND git log -1 --format=%H
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE DT_CONFIG_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
)
execute_process(
  COMMAND git remote get-url origin
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE DT_CONFIG_ORIGIN
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
)

configure_file(enable.in ${CMAKE_SOURCE_DIR}/enable @ONLY)

add_custom_target(
  CompileCommands ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${DT_CMAKE_ROOT}/compile_commands.json
)

add_subdirectory(compiler)
add_subdirectory(bin)
