add_llvm_library(
  Terminator
  MODULE
  Analysis.cpp
  Pass.cpp
  TClause.cpp
  ${leptoinst_SOURCE_DIR}/src/LeptoInstVisitor.cpp
)

