add_llvm_library(
  Lifetime
  MODULE
  Lifetime.cpp
  Pass.cpp
)

if(TARGET gino)
  add_dependencies(Lifetime gino)
endif()