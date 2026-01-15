BUILD_DIR := build
JOBS := $(shell nproc)

.PHONY: release debug clean rebuild

release:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release && $(MAKE) -j$(JOBS)

debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG_LOG=ON && $(MAKE) -j$(JOBS)

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean release
