DT_INSTALL_DIR ?= $(shell grep -m1 'export PATH' ../../enable | sed 's|.*PATH=||;s|/bin:.*||')
ifeq ($(DT_INSTALL_DIR),)
  $(error Could not detect DT_INSTALL_DIR; source ../../enable or set it manually)
endif

NOELLE_INCLUDE := $(shell noelle-config --include)
DT_INCLUDE     := $(DT_INSTALL_DIR)/include
DT_LIB         := $(DT_INSTALL_DIR)/lib

CXX     := clang++
STD     := -std=c++17
CXXFLAGS := $(STD) -O1 -Xclang -disable-llvm-passes -fno-exceptions \
            -I$(NOELLE_INCLUDE) -I$(DT_INCLUDE)

PASS_SO := $(DT_LIB)/LoopStopwatch.so
RT_SO   := $(DT_LIB)/libdtsw.so

SRC  := main.cpp
BC   := main.bc
NORM := main.norm.bc
INST := main.inst.bc
BIN  := main.out

.PHONY: all run clean

all: $(BIN)

$(BC): $(SRC)
	$(CXX) $(CXXFLAGS) -c -emit-llvm $< -o $@

$(NORM): $(BC)
	noelle-norm $< -o $@

$(INST): $(NORM)
	noelle-load \
	  -load $(PASS_SO) \
	  -LoopStopwatch \
	  --noelle-verbose=0 \
	  $< -o $@

$(BIN): $(INST)
	clang++ -fopenmp $(STD) \
	  -L$(DT_LIB) -Wl,-rpath,$(DT_LIB) \
	  -ldtsw \
	  $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BC) $(NORM) $(INST) $(BIN)
