file(GLOB SRC "src/*.cpp")
add_llvm_library("DTAnalysis" MODULE ${SRC})
