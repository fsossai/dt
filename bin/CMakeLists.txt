configure_file(winchester.in winchester @ONLY)

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/winchester
  DESTINATION bin
)