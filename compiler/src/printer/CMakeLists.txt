add_llvm_library(
  Printer
  MODULE
  Pass.cpp
)

install(TARGETS Printer DESTINATION lib)
