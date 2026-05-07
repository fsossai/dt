configure_file(winchester.in winchester @ONLY)
configure_file(stopwatches.in stopwatches @ONLY)

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/winchester
  DESTINATION bin
)

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/stopwatches
  DESTINATION bin
)