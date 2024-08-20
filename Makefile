BUILD_DIR = build
export JOBS ?= 16
export MAKEFLAGS += --no-print-directory

all: install

compile: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j$(JOBS)

$(BUILD_DIR):
	cmake -B $(BUILD_DIR) -S . -DCMAKE_INSTALL_PREFIX=install

format:
	find ./compiler ./skynet ./tests -regex '.*\.[c|h]pp' | xargs clang-format -i

install: compile
	cmake --install $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f
	rm -f enable

.PHONY: compile clean format install uninstall
