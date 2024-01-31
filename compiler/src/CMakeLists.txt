include_directories(
  ${LLVM_INCLUDE_DIRS}
  ${NOELLE_INSTALL_DIR}/include
  ${NOELLE_INSTALL_DIR}/include/svf
)

add_subdirectory(analysis)
add_subdirectory(printer)
add_subdirectory(invoke_to_call)

list(APPEND DT_CONFIG_LOADS "-load ${DT_CONFIG_LIB}/DTAnalysis.so")
list(APPEND DT_CONFIG_LOADS "-load ${DT_CONFIG_LIB}/DTPrinter.so")
list(APPEND DT_CONFIG_LOADS "-load ${DT_CONFIG_LIB}/InvokeToCall.so")

string(REPLACE ";" " " DT_CONFIG_LOADS "${DT_CONFIG_LOADS}")
configure_file(../dt-config.in ${CMAKE_BINARY_DIR}/dt-config @ONLY)
configure_file(../dt-opt.in ${CMAKE_BINARY_DIR}/dt-opt @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/dt-config DESTINATION ${DT_CONFIG_BIN}
  PERMISSIONS
  OWNER_EXECUTE OWNER_WRITE OWNER_READ
  GROUP_EXECUTE GROUP_READ
  WORLD_EXECUTE WORLD_READ
)

