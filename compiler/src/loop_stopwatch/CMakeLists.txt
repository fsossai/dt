set(LLVM_OPTIONAL_SOURCES runtime.c)

add_llvm_library(
  LoopStopwatch
  MODULE
  LoopStopwatch.cpp
  Pass.cpp
)

if(TARGET gino)
  add_dependencies(LoopStopwatch gino)
endif()

add_library(dtsw SHARED runtime.c)

target_include_directories(dtsw PRIVATE
  ${timers_SOURCE_DIR}/include
)

install(TARGETS dtsw
  LIBRARY DESTINATION ${DT_INSTALL_DIR}/lib
)
