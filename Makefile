compile: build
	cmake --build build

build:
	cmake -B build .

clean:
	rm -rf build
	rm -f enable

.PHONY: compile clean
