include_directories(
  ${LLVM_INCLUDE_DIRS}
  ${NOELLE_INSTALL_DIR}/include
  ${NOELLE_INSTALL_DIR}/include/svf
)

add_subdirectory(analysis)
# add_subdirectory(invoke_to_call)
# add_subdirectory(printer)
