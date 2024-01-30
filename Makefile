compile: build
	cmake --build build

build:
	cmake -B build -S . -DCMAKE_INSTALL_PREFIX=install

install: build
	cmake --build build
	cmake --install build

clean:
	rm -rf build

.PHONY: compile clean
