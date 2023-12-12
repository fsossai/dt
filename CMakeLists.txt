cmake_minimum_required(VERSION 3.13)

project(dependence-terminator)

configure_file(enable.in ${CMAKE_SOURCE_DIR}/enable @ONLY)

add_subdirectory(compiler)
