configure_file(winchester.in winchester @ONLY)
configure_file(dt-config.in dt-config @ONLY)

install(
  PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/winchester
    ${CMAKE_CURRENT_BINARY_DIR}/dt-config
  DESTINATION bin
)