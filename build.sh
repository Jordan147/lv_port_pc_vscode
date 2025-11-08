#!/bin/bash

# LVGL Project Multi-threaded Build Script
# Usage: ./build.sh [clean|release|debug|run]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get number of CPU cores
if command -v nproc &> /dev/null; then
    CORES=$(nproc)
elif command -v sysctl &> /dev/null; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BIN_DIR="${PROJECT_DIR}/bin"

echo -e "${BLUE}LVGL Multi-threaded Build Script${NC}"
echo -e "${BLUE}Using ${CORES} parallel jobs${NC}"
echo ""

# Function to clean build
clean_build() {
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
    rm -rf "${BIN_DIR}"
    echo -e "${GREEN}Clean complete${NC}"
}

# Function to configure cmake
configure_cmake() {
    local build_type="$1"
    echo -e "${YELLOW}Configuring CMake (${build_type})...${NC}"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    cmake .. -DCMAKE_BUILD_TYPE="${build_type}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
}

# Function to build project
build_project() {
    echo -e "${YELLOW}Building project with ${CORES} parallel jobs...${NC}"
    cd "${BUILD_DIR}"
    cmake --build . -j${CORES}
    echo -e "${GREEN}Build complete${NC}"
}

# Function to run the executable
run_project() {
    echo -e "${YELLOW}Running the application...${NC}"
    if [ -f "${BIN_DIR}/main" ]; then
        "${BIN_DIR}/main"
    else
        echo -e "${RED}Executable not found. Please build first.${NC}"
        exit 1
    fi
}

# Main logic
case "$1" in
    "clean")
        clean_build
        ;;
    "release")
        configure_cmake "Release"
        build_project
        ;;
    "debug")
        configure_cmake "Debug"
        build_project
        ;;
    "run")
        if [ ! -d "${BUILD_DIR}" ]; then
            echo -e "${YELLOW}Build directory not found. Configuring and building...${NC}"
            configure_cmake "Debug"
        fi
        build_project
        run_project
        ;;
    "")
        # Default: debug build
        if [ ! -d "${BUILD_DIR}" ]; then
            configure_cmake "Debug"
        fi
        build_project
        ;;
    *)
        echo "Usage: $0 [clean|release|debug|run]"
        echo "  clean    - Clean build directory"
        echo "  release  - Configure and build in Release mode"
        echo "  debug    - Configure and build in Debug mode"
        echo "  run      - Build (if needed) and run the application"
        echo "  (no arg) - Build with current configuration"
        exit 1
        ;;
esac

echo -e "${GREEN}Script completed successfully${NC}"
