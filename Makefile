BUILD_DIR := build
JOBS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.PHONY: release debug clean rebuild release-ci

release:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_OPENCL=OFF -DWITH_CUDA=OFF && $(MAKE) -j$(JOBS)

OPENSSL_ROOT := $(shell brew --prefix openssl 2>/dev/null)
OPENSSL_FLAG := $(if $(OPENSSL_ROOT),-DOPENSSL_ROOT_DIR=$(OPENSSL_ROOT),)

release-ci:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_OPENCL=OFF -DWITH_CUDA=OFF $(OPENSSL_FLAG) && $(MAKE) -j$(JOBS)
	@strip $(BUILD_DIR)/vltrig || true

debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG_LOG=ON && $(MAKE) -j$(JOBS)

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean release
