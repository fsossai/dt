cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

set(CMAKE_CXX_STANDARD 17)
set(DT_CMAKE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM 14 REQUIRED CONFIG)
find_program(CLANG_PATH clang)
find_program(CLANGXX_PATH clang++)

set(DT_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
set(NOELLE_INSTALL_DIR ${DT_CMAKE_ROOT}/deps/noelle/install)
set(NOELLE_SRC ${DT_CMAKE_ROOT}/deps/noelle)
set(GINO_INSTALL_DIR ${DT_CMAKE_ROOT}/deps/gino/install)
set(GINO_SRC ${DT_CMAKE_ROOT}/deps/gino)

include_directories(${NOELLE_INSTALL_DIR}/include)
include_directories(${GINO_INSTALL_DIR}/include)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

FetchContent_Declare(
    noelle
    GIT_REPOSITORY  "git@github.com:arcana-lab/noelle-dt.git"
    GIT_TAG         dt
)
set(NOELLE_SVF OFF)
set(NOELLE_SCAF OFF)
FetchContent_MakeAvailable(noelle)
FetchContent_GetProperties(noelle)

FetchContent_Declare(
  leptoinst
  GIT_REPOSITORY  "https://github.com/fsossai/lepto-inst.git"
)

FetchContent_GetProperties(leptoinst)
if(NOT leptoinst_POPULATED)
  FetchContent_MakeAvailable(leptoinst)
endif()

file(GLOB_RECURSE NOELLE_CORE_DIRS LIST_DIRECTORIES true "${noelle_SOURCE_DIR}/src/core/*")
foreach(dir ${NOELLE_CORE_DIRS})
  if(IS_DIRECTORY ${dir} AND EXISTS "${dir}/include")
    include_directories(${dir}/include)
  endif()
endforeach()

FetchContent_Declare(
    gino
    GIT_REPOSITORY  "git@github.com:arcana-lab/gino-dt.git"
    GIT_TAG         dt-refactored
)
FetchContent_MakeAvailable(gino)
FetchContent_GetProperties(gino)

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
  DtCompileCommands ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${DT_CMAKE_ROOT}/compile_commands.json
)

add_subdirectory(compiler)
add_subdirectory(bin)
add_subdirectory(skynet)
