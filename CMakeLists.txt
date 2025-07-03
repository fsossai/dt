cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

set(DT_CMAKE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

include(FetchContent)
include(ExternalProject)

set(DT_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
set(GINO_SRC_DIR ${CMAKE_BINARY_DIR}/_deps/gino)
set(ENABLE_FILE ${CMAKE_CURRENT_LIST_DIR}/enable)

ExternalProject_Add(gino
  GIT_REPOSITORY  "git@github.com:arcana-lab/gino-dt.git"
  GIT_TAG         dt-refactored
  CMAKE_ARGS      -DCMAKE_INSTALL_PREFIX=${DT_INSTALL_DIR}
                  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  INSTALL_DIR     ${CMAKE_BINARY_DIR}
)

ExternalProject_Get_Property(gino SOURCE_DIR)
set(GINO_SRC_DIR ${SOURCE_DIR})

FetchContent_Declare(
  leptoinst
  GIT_REPOSITORY  "https://github.com/fsossai/lepto-inst.git"
)

FetchContent_GetProperties(leptoinst)
if(NOT leptoinst_POPULATED)
  FetchContent_MakeAvailable(leptoinst)
endif()

file(GLOB_RECURSE GINO_CORE_DIRS LIST_DIRECTORIES true "${GINO_SRC_DIR}/src/core/*")
foreach(dir ${GINO_CORE_DIRS})
  if(IS_DIRECTORY ${dir} AND EXISTS "${dir}/include")
    include_directories(${dir}/include)
  endif()
endforeach()

find_package(LLVM 14 REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

include_directories(${DT_INSTALL_DIR}/include)

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
