add_llvm_library(
  InvokeToCall
  MODULE
  Pass.cpp
)

install(TARGETS InvokeToCall DESTINATION lib)
