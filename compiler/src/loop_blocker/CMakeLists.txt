add_llvm_library(
  LoopBlocker
  MODULE
  LoopBlocker.cpp
  Pass.cpp
)
add_dependencies(LoopBlocker noelle gino)
