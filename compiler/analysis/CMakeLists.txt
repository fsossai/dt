file(GLOB SRC "src/*.cpp")
add_llvm_library("Analysis" MODULE ${SRC})
