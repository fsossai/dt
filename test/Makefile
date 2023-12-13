-include Makefile.defaults

SINGLE_FILE_TARGETS= $(patsubst %.cpp,%,$(wildcard *.cpp))
DIRS=

all: targets $(SINGLE_FILE_TARGETS)

dti.o: ../compiler/dti.cpp
	$(CC) $(FLAGS) -c $^

targets:
	@for dir in $(DIRS); do \
		make -C $$dir; \
	done

$(SINGLE_FILE_TARGETS): % : %.cpp dti.o
	$(CC) $(STD) $(INCLUDE_DIRS) -O1 -Xclang -disable-llvm-passes $(FLAGS) -c -emit-llvm $^
	noelle-norm $@.bc -o $@.norm.bc
	llvm-dis $@.norm.bc
	$(CC) $(OPT) $(FLAGS) -DDT_ENABLE $@.bc -o $@.out

clean:
	rm -f *.out *.bc *.ll
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: all targets clean
