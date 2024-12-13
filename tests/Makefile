CXX = clang++
STD = -std=c++20
INCLUDE =-I ../skynet -I $(shell noelle-config --include)
OUTDIR ?= .
OPT =
FLAGS=

TARGETS=$(patsubst %.cpp,%,$(wildcard *.cpp))
DIRS=

all: $(TARGETS)

bfs_tc:
	winchester_flags="$$winchester_flags -DBFS_IMPL=bfs_tc" winchester bfs.cpp $(OUTDIR)
	cp $(OUTDIR)/s7_exe.out $(OUTDIR)/$@.out

bfs_%:
	$(CXX) $(STD) -DBFS_IMPL=$@ -fopenmp $(INCLUDE) $(OPT) $(FLAGS) bfs.cpp -o $(OUTDIR)/$@.out

$(TARGETS): % : %.cpp
	$(CXX) $(STD) -fopenmp $(INCLUDE) $(OPT) $(FLAGS) $^ -o $@.out

clean:
	rm -f *.out *.bc *.ll
	@for dir in $(DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: all targets clean
