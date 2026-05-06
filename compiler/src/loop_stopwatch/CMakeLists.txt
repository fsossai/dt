add_llvm_library(
  LoopStopwatch
  MODULE
  LoopStopwatch.cpp
  Pass.cpp
)

add_library(
  dttimers
  SHARED
  runtime.c
)

target_include_directories(dttimers PRIVATE
  ${timers_SOURCE_DIR}/include
)

install(TARGETS dttimers
  LIBRARY DESTINATION ${DT_CONFIG_LIB}
)
