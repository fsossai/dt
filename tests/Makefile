CXX=clang++
STD=-std=c++20
OPT=-O3
INCLUDE_DIRS=-I../skynet -I../compiler
OPENMP=-fopenmp
FLAGS=

SINGLE_FILE_TARGETS=$(patsubst %.cpp,%,$(wildcard *.cpp))
DIRS=

all: targets $(SINGLE_FILE_TARGETS)

targets:
	@for dir in $(DIRS); do \
		make -C $$dir; \
	done

$(SINGLE_FILE_TARGETS): % : %.cpp
	$(CXX) $(OPENMP) $(STD) $(INCLUDE_DIRS) -fno-exceptions -DDT_ENABLE -O0 -g -o $@.debug.out $^
	$(CXX) $(OPENMP) $(STD) $(INCLUDE_DIRS) -fno-exceptions -DDT_ENABLE -O1 -Xclang -disable-llvm-passes $(FLAGS) -c -emit-llvm $^
	opt --inline $@.bc -o $@.bc
	noelle-norm $@.bc -o $@.bc
	$(CXX) $(OPENMP) $(OPT) $@.bc -o $@.out

clean:
	rm -f *.out *.bc *.ll
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: all targets clean
