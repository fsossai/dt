add_llvm_library(
  StripNoInline
  MODULE
  StripNoInline.cpp
  Pass.cpp
)

if(TARGET gino)
  add_dependencies(StripNoInline gino)
endif()