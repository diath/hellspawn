#!/usr/bin/env bash
#
# build.sh — Single-entry-point build orchestration for the Hellspawn project.
#
# The script interactively prompts for:
#   1. Build type   — release or debug
#   2. vcpkg usage  — yes or no
#
# Requirements: cmake, clang++ or g++, and optionally vcpkg (with VCPKG_ROOT set or vcpkg in PATH)

set -euo pipefail

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"
TARGET_NAME="hellspawn"

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log_info()  { printf "${CYAN}[INFO]${NC}  %s\n" "$*"; }
log_ok()    { printf "${GREEN}[OK]${NC}    %s\n" "$*"; }
log_warn()  { printf "${YELLOW}[WARN]${NC}  %s\n" "$*"; }
log_error() { printf "${RED}[ERROR]${NC} %s\n" "$*" >&2; }

die() {
    log_error "$@"
    exit 1
}

# ---------------------------------------------------------------------------
# Interactive prompt: Build type (release/debug)
# ---------------------------------------------------------------------------
# Repeatedly asks the user until a valid answer is provided.
# Accepts "release" or "debug" (case-insensitive).
# ---------------------------------------------------------------------------

while true; do
    printf "${CYAN}[INPUT]${NC} Enter build type (release/debug): "
    read -r BUILD_TYPE_INPUT

    # Normalize to lowercase for comparison
    BUILD_TYPE_INPUT="$(echo "$BUILD_TYPE_INPUT" | tr '[:upper:]' '[:lower:]')"

    if [[ "$BUILD_TYPE_INPUT" == "release" ]]; then
        BUILD_TYPE="Release"
        break
    elif [[ "$BUILD_TYPE_INPUT" == "debug" ]]; then
        BUILD_TYPE="Debug"
        break
    else
        log_warn "Invalid input '$BUILD_TYPE_INPUT'. Please enter 'release' or 'debug'."
    fi
done

log_info "Build type: $BUILD_TYPE"

# ---------------------------------------------------------------------------
# Interactive prompt: Use vcpkg? (yes/no)
# ---------------------------------------------------------------------------
# Repeatedly asks the user until a valid answer is provided.
# Accepts "yes" or "no" (case-insensitive).
# ---------------------------------------------------------------------------

while true; do
    printf "${CYAN}[INPUT]${NC} Use vcpkg? (yes/no): "
    read -r USE_VCPKG_INPUT

    # Normalize to lowercase for comparison
    USE_VCPKG_INPUT="$(echo "$USE_VCPKG_INPUT" | tr '[:upper:]' '[:lower:]')"

    if [[ "$USE_VCPKG_INPUT" == "yes" ]]; then
        USE_VCPKG="yes"
        break
    elif [[ "$USE_VCPKG_INPUT" == "no" ]]; then
        USE_VCPKG="no"
        break
    else
        log_warn "Invalid input '$USE_VCPKG_INPUT'. Please enter 'yes' or 'no'."
    fi
done

log_info "Use vcpkg: $USE_VCPKG"

# ---------------------------------------------------------------------------
# Dependency validation: CMake
# ---------------------------------------------------------------------------

if ! command -v cmake &>/dev/null; then
    die "CMake is not installed or not in PATH. Please install CMake >= 3.21."
fi

CMAKE_BIN="$(command -v cmake)"
CMAKE_VERSION="$(cmake --version | head -n1)"
log_ok "CMake found: $CMAKE_BIN ($CMAKE_VERSION)"

# ---------------------------------------------------------------------------
# Dependency validation: C++ compiler (prefer clang++ over g++)
# ---------------------------------------------------------------------------

CXX_COMPILER=""
CC_COMPILER=""

if command -v clang++ &>/dev/null; then
    CXX_COMPILER="$(command -v clang++)"
    CC_COMPILER="$(command -v clang)"
    COMPILER_ID="clang"
elif command -v g++ &>/dev/null; then
    CXX_COMPILER="$(command -v g++)"
    CC_COMPILER="$(command -v gcc)"
    COMPILER_ID="gcc"
else
    die "No suitable C++ compiler found. Install clang or gcc."
fi

COMPILER_VERSION="$($CXX_COMPILER --version | head -n1)"
log_ok "C++ compiler found ($COMPILER_ID): $CXX_COMPILER ($COMPILER_VERSION)"

# Verify the C compiler counterpart actually exists
if [[ -z "$CC_COMPILER" ]] || ! command -v "$CC_COMPILER" &>/dev/null; then
    log_warn "C compiler counterpart for $COMPILER_ID not found; CMake will auto-detect."
    CC_COMPILER=""
fi

# ---------------------------------------------------------------------------
# Dependency validation: vcpkg (only when the user opted in)
# ---------------------------------------------------------------------------

VCPKG_TOOLCHAIN=""

if [[ "$USE_VCPKG" == "yes" ]]; then

    resolve_vcpkg_root() {
        # 1. Explicit VCPKG_ROOT environment variable
        if [[ -n ${VCPKG_ROOT:-} ]] && [[ -d "$VCPKG_ROOT" ]]; then
            echo "$VCPKG_ROOT"
            return 0
        fi

        # 2. vcpkg binary in PATH — derive root from its location
        if command -v vcpkg &>/dev/null; then
            local vcpkg_bin
            vcpkg_bin="$(command -v vcpkg)"
            local vcpkg_dir
            vcpkg_dir="$(dirname "$(realpath "$vcpkg_bin")")"
            if [[ -f "$vcpkg_dir/scripts/buildsystems/vcpkg.cmake" ]]; then
                echo "$vcpkg_dir"
                return 0
            fi
        fi

        # 3. Common installation directories
        local common_paths=(
            "/opt/vcpkg"
            "$HOME/vcpkg"
            "$HOME/.vcpkg"
            "/usr/local/vcpkg"
            "/usr/local/share/vcpkg"
        )
        for candidate in "${common_paths[@]}"; do
            if [[ -f "$candidate/scripts/buildsystems/vcpkg.cmake" ]]; then
                echo "$candidate"
                return 0
            fi
        done

        return 1
    }

    VCPKG_ROOT="$(resolve_vcpkg_root)" \
        || die "vcpkg not found. Set the VCPKG_ROOT environment variable or install vcpkg."

    VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

    if [[ ! -f "$VCPKG_TOOLCHAIN" ]]; then
        die "vcpkg toolchain file not found at: $VCPKG_TOOLCHAIN"
    fi

    log_ok "vcpkg root: $VCPKG_ROOT"
    log_ok "vcpkg toolchain: $VCPKG_TOOLCHAIN"

else
    log_info "Skipping vcpkg — dependencies must be available via system packages."
fi

# ---------------------------------------------------------------------------
# Determine parallel job count
# ---------------------------------------------------------------------------

if command -v nproc &>/dev/null; then
    PARALLEL_JOBS="$(nproc)"
elif command -v sysctl &>/dev/null && sysctl -n hw.ncpu &>/dev/null 2>&1; then
    PARALLEL_JOBS="$(sysctl -n hw.ncpu)"
else
    PARALLEL_JOBS=4
fi

log_info "Parallel jobs: $PARALLEL_JOBS"

# ---------------------------------------------------------------------------
# Create build directory
# ---------------------------------------------------------------------------

if [[ ! -d "$BUILD_DIR" ]]; then
    log_info "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR" || die "Failed to create build directory: $BUILD_DIR"
fi

# ---------------------------------------------------------------------------
# CMake configure
# ---------------------------------------------------------------------------
# When vcpkg is enabled the toolchain file is passed so that manifest
# dependencies are installed automatically during this step.
# ---------------------------------------------------------------------------

log_info "Configuring CMake..."

CMAKE_ARGS=(
    -S "$PROJECT_ROOT"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

# Only pass CC if we resolved it
if [[ -n "$CC_COMPILER" ]]; then
    CMAKE_ARGS+=(-DCMAKE_C_COMPILER="$CC_COMPILER")
fi

# Only pass the vcpkg toolchain file when the user opted in
if [[ -n "$VCPKG_TOOLCHAIN" ]]; then
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN")
fi

cmake "${CMAKE_ARGS[@]}" || die "CMake configuration failed."

log_ok "CMake configuration complete."

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

log_info "Building $TARGET_NAME ($BUILD_TYPE) with $PARALLEL_JOBS parallel jobs..."

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target "$TARGET_NAME" -j "$PARALLEL_JOBS" \
    || die "Build failed."

# ---------------------------------------------------------------------------
# Success
# ---------------------------------------------------------------------------

BINARY_PATH="$BUILD_DIR/$TARGET_NAME"
if [[ -f "$BINARY_PATH" ]]; then
    log_ok "Build succeeded: $BINARY_PATH"
else
    # Some generators place binaries under a config subdirectory
    log_ok "Build succeeded. Binary located in: $BUILD_DIR"
fi

exit 0
