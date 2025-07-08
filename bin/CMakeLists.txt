configure_file(winchester.in winchester @ONLY)
install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/winchester
  DESTINATION bin
)
configure_file(dt-config.in dt-config @ONLY)
install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/dt-config
  DESTINATION bin
)