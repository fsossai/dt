BUILD_DIR = build
export JOBS ?= 8
export MAKEFLAGS += --no-print-directory

all: install

compile: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j$(JOBS)

$(BUILD_DIR):
	cmake -B $(BUILD_DIR) -S . -DCMAKE_INSTALL_PREFIX=install

install: compile
	cmake --install $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f
	rm -f enable

.PHONY: compile clean install uninstall
