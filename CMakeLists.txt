cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

include(ExternalProject)
include(FetchContent)

include(config.default.cmake)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.cmake)
  include(config.cmake)
endif()

if(DEFINED DT_INSTALL_DIR)
  get_filename_component(DT_INSTALL_DIR ${DT_INSTALL_DIR} ABSOLUTE)
  set(CMAKE_INSTALL_PREFIX ${DT_INSTALL_DIR})
else()
  get_filename_component(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} ABSOLUTE)
  set(DT_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

set(GINO_SRC_DIR ${CMAKE_BINARY_DIR}/_deps/gino)
set(DT_ENABLE_FILE ${CMAKE_SOURCE_DIR}/enable)

string(ASCII 27 Esc)
set(Purple "${Esc}[35m")
set(Green "${Esc}[32m")
set(Red "${Esc}[31m")
set(ColorReset "${Esc}[m")

function(dt_acquire_option OPT)
  if(${OPT} MATCHES "ON|On|on|1")
    set(${OPT} ON PARENT_SCOPE)
    message(STATUS "${Purple}Option${ColorReset} ${OPT} ${Green}ON${ColorReset}")
  elseif(${OPT} MATCHES "OFF|Off|off|0")
    set(${OPT} OFF PARENT_SCOPE)
    message(STATUS "${Purple}Option${ColorReset} ${OPT} ${Red}OFF${ColorReset}")
  elseif(${OPT} MATCHES "OFF|Off|off|0")
  else()
    message(FATAL_ERROR "${OPT} must be either ON or OFF")
  endif()
endfunction()

if(NOT DEFINED GINO_INSTALL_DIR OR (GINO_INSTALL_DIR STREQUAL ""))
  message(STATUS "${Purple}GINO_INSTALL_DIR${ColorReset} is not defined")
  dt_acquire_option(NOELLE_SVF)
  dt_acquire_option(NOELLE_SCAF)
  message(STATUS "${Purple}Install directory${ColorReset} is ${CMAKE_INSTALL_PREFIX}")

  ExternalProject_Add(gino
    GIT_REPOSITORY  "git@github.com:arcana-lab/gino-dt.git"
    GIT_TAG         dt
    CMAKE_ARGS      -DCMAKE_INSTALL_PREFIX=${DT_INSTALL_DIR}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DNOELLE_SVF=${NOELLE_SVF}
                    -DNOELLE_SCAF=${NOELLE_SCAF}
    INSTALL_DIR     ${CMAKE_BINARY_DIR}
  )

  ExternalProject_Get_Property(gino SOURCE_DIR)
  set(GINO_SRC_DIR ${SOURCE_DIR})
  include_directories(${DT_INSTALL_DIR}/include)
else()
  message(STATUS "${Purple}GINO_INSTALL_DIR${ColorReset} is ${GINO_INSTALL_DIR}")
  execute_process(
    COMMAND ${GINO_INSTALL_DIR}/bin/gino-config --include-dirs
    OUTPUT_VARIABLE INCLUDE_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GINO_CONFIG_RESULT
  )
  if(GINO_CONFIG_RESULT EQUAL 0)
    string(REPLACE "\n" ";" INCLUDE_DIRS ${INCLUDE_DIRS})
    foreach(INCLUDE_DIR ${INCLUDE_DIRS})
      include_directories(${INCLUDE_DIR})
    endforeach()
  else()
    message(FATAL_ERROR "Failed to get include directories from gino-config")
  endif()
endif()

FetchContent_Declare(
  leptoinst
  GIT_REPOSITORY  "https://github.com/fsossai/lepto-inst.git"
)

FetchContent_GetProperties(leptoinst)
if(NOT leptoinst_POPULATED)
  FetchContent_MakeAvailable(leptoinst)
endif()

find_package(LLVM 14 REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
)

add_subdirectory(compiler)
add_subdirectory(include)
add_subdirectory(bin)
