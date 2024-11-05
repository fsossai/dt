CXX=clang++
STD=-std=c++20
OPT=
INCLUDE_DIRS=-I../skynet -I $(shell noelle-config --include)
OPENMP=-fopenmp
OUTDIR?=.
FLAGS=

SINGLE_FILE_TARGETS=$(patsubst %.cpp,%,$(wildcard *.cpp))
DIRS=

all: targets $(SINGLE_FILE_TARGETS)

targets:
	@for dir in $(DIRS); do \
		make -C $$dir; \
	done

bfs_%:
	$(CXX) -D$(shell echo $@ | tr a-z A-Z) $(OPENMP) $(STD) $(INCLUDE_DIRS) $(OPT) $(FLAGS) bfs.cpp -o $(OUTDIR)/$@.out

$(SINGLE_FILE_TARGETS): % : %.cpp
	# $(CXX) $(OPENMP) $(STD) $(INCLUDE_DIRS) -fno-exceptions -O1 -Xclang -disable-llvm-passes $(FLAGS) -c -emit-llvm $^
	# opt --inline $@.bc -o $@.bc
	# noelle-norm $@.bc -o $@.bc
	$(CXX) $(OPENMP) $(STD) $(INCLUDE_DIRS) $(OPT) $(FLAGS) $^ -o $@.out

clean:
	rm -f *.out *.bc *.ll
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: all targets clean
