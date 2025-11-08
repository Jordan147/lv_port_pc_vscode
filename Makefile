# LVGL Project Makefile - Multi-threaded Build Support
# This is a convenience wrapper around CMake with multi-threading

.PHONY: all build clean debug release run help install

# Default target
all: build

# Build the project (Debug mode by default)
build:
	@echo "Building with multi-threading..."
	@./build.sh

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@./build.sh clean

# Debug build
debug:
	@echo "Building in Debug mode..."
	@./build.sh debug

# Release build
release:
	@echo "Building in Release mode..."
	@./build.sh release

# Build and run
run:
	@echo "Building and running..."
	@./build.sh run

# Install dependencies (macOS)
install:
	@echo "Installing dependencies..."
	@if command -v brew >/dev/null 2>&1; then \
		echo "Installing SDL2 via Homebrew..."; \
		brew install sdl2; \
	else \
		echo "Homebrew not found. Please install dependencies manually:"; \
		echo "  - SDL2 development libraries"; \
		echo "  - CMake (3.12.4 or newer)"; \
	fi

# Show help
help:
	@echo "LVGL Multi-threaded Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the project (default, Debug mode)"
	@echo "  build    - Build the project (Debug mode)"
	@echo "  clean    - Clean build directory"
	@echo "  debug    - Build in Debug mode"
	@echo "  release  - Build in Release mode"
	@echo "  run      - Build and run the application"
	@echo "  install  - Install dependencies (macOS with Homebrew)"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "For more options, use the build script directly:"
	@echo "  ./build.sh [clean|debug|release|run]"
