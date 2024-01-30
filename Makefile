compile: build
	cmake --build build

build:
	cmake -B build -S . # -DALL=1

clean:
	rm -rf build
	rm -f enable

.PHONY: compile clean
