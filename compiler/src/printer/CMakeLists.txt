set(pass_name DTPrinter)

add_llvm_library(
  ${pass_name}
  MODULE
  Pass.cpp
)

install(TARGETS ${pass_name} DESTINATION ${DT_CONFIG_LIB})
