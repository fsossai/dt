add_llvm_library(
  LoopSquasher
  MODULE
  LoopSquasher.cpp
  Pass.cpp
)

if(TARGET gino)
  add_dependencies(LoopSquasher gino)
endif()