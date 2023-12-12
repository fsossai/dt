file(GLOB SRC "src/*.cpp")
set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS " -std=c++17")
add_llvm_library("Printer" MODULE ${SRC})
