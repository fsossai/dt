file(GLOB SRC "src/*.cpp")
add_llvm_library("InvokeToCall" MODULE ${SRC})
