CMAKE_COMMON_FLAGS ?= -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
CMAKE_DEBUG_FLAGS ?= -DUSERVER_SANITIZE='addr ub'
CMAKE_RELEASE_FLAGS ?=
CMAKE_OS_FLAGS ?= -DUSERVER_FEATURE_CRYPTOPP_BLAKE2=0 -DUSERVER_FEATURE_REDIS_HI_MALLOC=1
NPROCS ?= $(shell nproc)
CLANG_FORMAT ?= clang-format
DOCKER_COMPOSE ?= docker-compose

# NOTE: use Makefile.local for customization
-include Makefile.local

.PHONY: all
all: test-debug test-release

# Debug cmake configuration
build_debug/Makefile:
	@git submodule update --init
	@mkdir -p build_debug
	@cd build_debug && \
      cmake -DCMAKE_BUILD_TYPE=Debug $(CMAKE_COMMON_FLAGS) $(CMAKE_DEBUG_FLAGS) $(CMAKE_OS_FLAGS) $(CMAKE_OPTIONS) ..

# Release cmake configuration
build_release/Makefile:
	@git submodule update --init
	@mkdir -p build_release
	@cd build_release && \
      cmake -DCMAKE_BUILD_TYPE=Release $(CMAKE_COMMON_FLAGS) $(CMAKE_RELEASE_FLAGS) $(CMAKE_OS_FLAGS) $(CMAKE_OPTIONS) ..

# Run cmake
.PHONY: cmake-debug cmake-release
cmake-debug cmake-release: cmake-%: build_%/Makefile

# Build using cmake
.PHONY: build-debug build-release
build-debug build-release: build-%: cmake-%
	@cmake --build build_$* -j $(NPROCS) --target telegram_bot

# Test
.PHONY: test-debug test-release
test-debug test-release: test-%: build-%
	@cmake --build build_$* -j $(NPROCS) --target telegram_bot_unittest
	# @cmake --build build_$* -j $(NPROCS) --target telegram_bot_benchmark
	@cd build_$* && ((test -t 1 && GTEST_COLOR=1 PYTEST_ADDOPTS="--color=yes" ctest -V) || ctest -V)
	# @cd build_$* && (test -t 1 && GTEST_COLOR=1 PYTEST_ADDOPTS="--color=yes -x" ctest -V)
	@pep8 tests

# Start the service (via testsuite service runner)
.PHONY: service-start-debug service-start-release
service-start-debug service-start-release: service-start-%: build-%
	@cd ./build_$* && $(MAKE) start-telegram_bot

# Cleanup data
.PHONY: clean-debug clean-release
clean-debug clean-release: clean-%:
	cd build_$* && $(MAKE) clean

.PHONY: dist-clean
dist-clean:
	@rm -rf build_*
	@rm -rf tests/__pycache__/
	@rm -rf tests/.pytest_cache/
	@rm -rf ./debian/telegram-bot

# Install
.PHONY: install-debug install-release
install-debug install-release: install-%: build-%
	@cd build_$* && \
		cmake --install . -v --component telegram_bot

.PHONY: install
install: install-release

# Format the sources
.PHONY: format
format:
	@find src -name '*pp' -type f | xargs $(CLANG_FORMAT) -i
	@find tests -name '*.py' -type f | xargs autopep8 -i

# Internal hidden targets that are used only in docker environment
.PHONY: --in-docker-start-debug --in-docker-start-release
--in-docker-start-debug --in-docker-start-release: --in-docker-start-%: install-%
	@psql 'postgresql://user:password@service-postgres:5432/telegram_bot_pg_birthday'
	@/home/user/.local/bin/telegram_bot \
		--config /home/user/.local/etc/telegram_bot/static_config.yaml \
		--config_vars /home/user/.local/etc/pg_service_template/config_vars.docker.yaml

# Build and run service in docker environment
.PHONY: docker-start-service-debug docker-start-service-release
docker-start-service-debug docker-start-service-release: docker-start-service-%:
	@$(DOCKER_COMPOSE) run -p 8080:8080 --rm telegram_bot-container make -- --in-docker-start-$*

# Start specific target in docker environment
.PHONY: docker-cmake-debug docker-build-debug docker-test-debug docker-clean-debug docker-install-debug docker-cmake-release docker-build-release docker-test-release docker-clean-release docker-install-release
docker-cmake-debug docker-build-debug docker-test-debug docker-clean-debug docker-install-debug docker-cmake-release docker-build-release docker-test-release docker-clean-release docker-install-release: docker-%:
	$(DOCKER_COMPOSE) run --rm telegram_bot-container make $*

# Stop docker container and cleanup data
.PHONY: docker-clean-data
docker-clean-data:
	@$(DOCKER_COMPOSE) down -v
	@rm -rf ./.pgdata
	@rm -rf ./.cores

.PHONY: deb
deb:
	dpkg-buildpackage -b -uc -us
