include_directories(
  ${LLVM_INCLUDE_DIRS}
  ${NOELLE_INSTALL_DIR}/include
  ${GINO_INSTALL_DIR}/include
  include
)
add_subdirectory(src)
