add_llvm_library(
  DTAnalysis
  MODULE
  Pass.cpp
)

install(TARGETS DTAnalysis DESTINATION lib)
