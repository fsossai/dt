add_llvm_library(
  LoopBlocker
  MODULE
  LoopBlocker.cpp
  Pass.cpp
)

if(TARGET gino)
  add_dependencies(LoopBlocker gino)
endif()