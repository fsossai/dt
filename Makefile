BUILD_DIR = build
export JOBS ?= 16
export NOELLE_INSTALL_DIR=$(abspath deps/noelle/install)
export GINO_INSTALL_DIR=$(abspath deps/gino/install)
export MAKEFLAGS += --no-print-directory

all: deps install

deps:
	git clone --depth 1 git@github.com:arcana-lab/noelle-dt.git deps/noelle
	cmake -B deps/noelle/build -S deps/noelle \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(NOELLE_INSTALL_DIR) \
		-DNOELLE_SVF=ON \
		-DNOELLE_SCAF=ON
	cmake --build deps/noelle/build -j$(JOBS)
	cmake --install deps/noelle/build
	git clone git@github.com:arcana-lab/gino-dt.git deps/gino
	make -C deps/gino
	make -C deps/gino/tests download

compile: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j$(JOBS)

$(BUILD_DIR):
	cmake -B $(BUILD_DIR) -S . \
		-DCMAKE_INSTALL_PREFIX=install \
		-DCMAKE_BUILD_TYPE=Release

format:
	find ./compiler ./skynet ./tests -regex '.*\.[c|h]pp' | xargs clang-format -i

install: compile
	cmake --install $(BUILD_DIR)

clean:
	rm -rf deps $(BUILD_DIR) 
	rm -f compile_commands.json

uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f
	rm -f enable

.PHONY: compile clean format install uninstall
