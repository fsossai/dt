add_llvm_library(
  Dot
  MODULE
  Dot.cpp
  Pass.cpp
  ${leptoinst_SOURCE_DIR}/src/LeptoInstVisitor.cpp
)