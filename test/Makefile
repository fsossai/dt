-include Makefile.defaults

SINGLE_FILE_TARGETS= $(patsubst %.cpp,%,$(wildcard *.cpp))
DIRS=

all: targets $(SINGLE_FILE_TARGETS)

targets:
	@for dir in $(DIRS); do \
		make -C $$dir; \
	done

$(SINGLE_FILE_TARGETS): % : %.cpp
	$(CC) $(STD) $(INCLUDE_DIRS) -DDT_ENABLE -O1 -Xclang -disable-llvm-passes $(FLAGS) -c -emit-llvm $^
	opt --inline $@.bc -o $@.bc
	noelle-norm $@.bc -o $@.bc
	llvm-dis $@.bc
	$(CC) $(OPT) $(FLAGS) $@.bc -o $@.out

clean:
	rm -f *.out *.bc *.ll
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: all targets clean
