BUILD_DIR = build
MAKEFLAGS += --no-print-directory
JOBS ?=

all: install

compile: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j $(JOBS)

$(BUILD_DIR):
	cmake -B $(BUILD_DIR) -S . \
		-DCMAKE_INSTALL_MESSAGE=LAZY \
		-DCMAKE_INSTALL_PREFIX=install \
		-DCMAKE_BUILD_TYPE=Debug

format:
	find ./compiler ./skynet ./tests -regex '.*\.[c|h]pp' | xargs clang-format -i

install: compile
	cmake --install $(BUILD_DIR)

menuconfig:
	@python3 bin/menuconfig.py

clean:
	rm -rf $(BUILD_DIR) install
	rm -f compile_commands.json config.cmake .config

uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f
	rm -f enable

.PHONY: compile clean format menuconfig install uninstall
