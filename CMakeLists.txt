cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

set(CMAKE_CXX_STANDARD 17)
set(DT_CMAKE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM 14 REQUIRED CONFIG)
find_program(CLANG_PATH clang)
find_program(CLANGXX_PATH clang++)

set(NOELLE_INSTALL_DIR ${DT_CMAKE_ROOT}/noelle/install)
set(GINO_INSTALL_DIR ${DT_CMAKE_ROOT}/gino/install)

include(ExternalProject)
ExternalProject_Add(
    noelle
    GIT_REPOSITORY  "git@github.com:arcana-lab/noelle-dt.git"
    GIT_TAG         dt
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . -j16
    INSTALL_COMMAND ${CMAKE_COMMAND} --install .
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${NOELLE_INSTALL_DIR}
        -DNOELLE_BUILD_TYPE=Release
        -DNOELLE_SVF=ON
        -DNOELLE_SCAF=ON
)
include_directories(${NOELLE_INSTALL_DIR}/include)

ExternalProject_Add( 
    gino
    GIT_REPOSITORY      "git@github.com:arcana-lab/gino-dt.git"
    GIT_TAG             dt
    PREFIX              ${CMAKE_BINARY_DIR}/_deps/gino
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       NOELLE_INSTALL_DIR=${NOELLE_INSTALL_DIR} make
    BUILD_IN_SOURCE     ON
    INSTALL_COMMAND     ""
)
include_directories(${GINO_INSTALL_DIR}/include)

add_dependencies(gino noelle)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)
FetchContent_Declare(
  leptoinst
  GIT_REPOSITORY  "https://github.com/fsossai/lepto-inst"
)
FetchContent_MakeAvailable(leptoinst)
FetchContent_GetProperties(leptoinst)

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
