include_directories(
  ${LLVM_INCLUDE_DIRS}
  ${NOELLE_INSTALL_DIR}/include
  ${NOELLE_INSTALL_DIR}/include/svf
  ${CMAKE_SOURCE_DIR}/compiler/include
)

add_subdirectory(analysis)
add_subdirectory(printer)
add_subdirectory(invoke_to_call)

list(APPEND DT_CONFIG_LOADS "-load ${DT_CONFIG_LIB}/Analysis.so")
list(APPEND DT_CONFIG_LOADS "-load ${DT_CONFIG_LIB}/Printer.so")
list(APPEND DT_CONFIG_LOADS "-load ${DT_CONFIG_LIB}/InvokeToCall.so")

string(REPLACE ";" " " DT_CONFIG_LOADS "${DT_CONFIG_LOADS}")
configure_file(../dt-config.in ${CMAKE_BINARY_DIR}/dt-config @ONLY)
configure_file(../dt-opt.in ${CMAKE_BINARY_DIR}/dt-opt @ONLY)
install(PROGRAMS ${CMAKE_BINARY_DIR}/dt-config DESTINATION ${DT_CONFIG_BIN})

