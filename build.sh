#!/bin/bash
# build.sh — Fresh-pull-ready build script for EtherCatDrone
#
# Handles: dependency checks, git submodules, ethercat library build,
#          CMake configure, compilation, tests, and optional static analysis.
#
# Usage:
#   ./build.sh                          # Standard debug build
#   ./build.sh --release                # Release build
#   ./build.sh --dev                    # Build + static analysis (clang-tidy, cppcheck)
#   ./build.sh --test                   # Build + run CTest suite
#   ./build.sh --clean                  # Clean build dir first
#   ./build.sh --install                # Install system dependencies first
#   ./build.sh --analysis-only          # Run static analysis only (no rebuild)
#   ./build.sh --skip-ethercat          # Skip building libethercat (stub mode)
#   ./build.sh --fix                    # Apply clang-tidy auto-fixes
#   ./build.sh --build-dir=DIR          # Override build output directory
#   ./build.sh --help                   # Show this help
#
# Examples:
#   ./build.sh --install --dev --test   # Install deps, build with analysis, run tests
#   ./build.sh --clean --release        # Clean then build release

set -euo pipefail

# ---- Paths ---------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---- Defaults ------------------------------------------------------------
BUILD_TYPE="Debug"
BUILD_DIR=""                              # Auto-set from BUILD_TYPE unless overridden
DO_BUILD=true
DO_ANALYSIS=false
DO_CLEAN=false
DO_INSTALL=false
DO_TESTS=false
SKIP_ETHERCAT=false
CLANG_TIDY_FIX=""

# ---- Argument parsing ----------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        --release)          BUILD_TYPE="Release" ;;
        --dev)              DO_ANALYSIS=true ;;
        --test)             DO_TESTS=true ;;
        --clean)            DO_CLEAN=true ;;
        --install)          DO_INSTALL=true ;;
        --analysis-only)    DO_BUILD=false; DO_ANALYSIS=true ;;
        --skip-ethercat)    SKIP_ETHERCAT=true ;;
        --fix)              CLANG_TIDY_FIX="--fix" ;;
        --build-dir=*)      BUILD_DIR="${arg#--build-dir=}" ;;
        --help|-h)
            sed -n '4,28p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            echo "Run '$0 --help' for usage." >&2
            exit 1
            ;;
    esac
done

# Set build directory (override takes precedence)
if [ -z "$BUILD_DIR" ]; then
    if [ "$BUILD_TYPE" = "Debug" ]; then
        BUILD_DIR="build/debug-linux"
    else
        BUILD_DIR="build/release-linux"
    fi
fi

# ---- Colours -------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
ok()   { echo -e "${GREEN}✓${NC} $*"; }
warn() { echo -e "${YELLOW}⚠${NC} $*"; }
err()  { echo -e "${RED}✗${NC} $*"; }
hdr()  { echo -e "\n${BLUE}══════════════════════════════════════════${NC}"
          echo -e "${BLUE}  $*${NC}"
          echo -e "${BLUE}══════════════════════════════════════════${NC}"; }

# ---- 0. Install system dependencies ------------------------------------
if [ "$DO_INSTALL" = true ]; then
    hdr "Installing Dependencies"
    INSTALL_SCRIPT="$SCRIPT_DIR/scripts/linux/install-deps.sh"
    if [ ! -f "$INSTALL_SCRIPT" ]; then
        err "Install script not found: $INSTALL_SCRIPT"
        exit 1
    fi
    chmod +x "$INSTALL_SCRIPT"
    INSTALL_ARGS=()
    [ "$DO_ANALYSIS" = true ] && INSTALL_ARGS+=("--dev")
    bash "$INSTALL_SCRIPT" "${INSTALL_ARGS[@]}"
fi

# ---- Dependency checks ---------------------------------------------------
check_cmd() {
    if ! command -v "$1" &>/dev/null; then
        err "$1 not found — run: ./build.sh --install"
        exit 1
    fi
}

hdr "Checking Dependencies"
check_cmd cmake
check_cmd ninja
check_cmd g++
check_cmd git
ok "Core build tools: cmake, ninja, g++, git"

# gRPC toolchain (optional — stub mode fallback)
if command -v protoc &>/dev/null && command -v grpc_cpp_plugin &>/dev/null; then
    ok "gRPC toolchain: protoc + grpc_cpp_plugin"
else
    warn "gRPC toolchain missing — gRPC server will be stub-only"
    warn "Fix: ./build.sh --install"
fi

# ---- 1. Git submodules ---------------------------------------------------
hdr "Git Submodules"
if [ -f .gitmodules ]; then
    if [ ! -d thirdparty/ethercat/.git ] && [ ! -f thirdparty/ethercat/.git ]; then
        echo "Initializing submodules..."
        git submodule update --init --recursive
        ok "Submodules initialized"
    else
        ok "Submodules already initialized"
    fi
else
    warn "No .gitmodules file found"
fi

# ---- 2. Build libethercat (if needed) ------------------------------------
if [ "$SKIP_ETHERCAT" = false ] && [ "$DO_BUILD" = true ]; then
    ETHERCAT_DIR="$SCRIPT_DIR/thirdparty/ethercat"

    if [ -d "$ETHERCAT_DIR" ]; then
        LIB_EXISTS=false
        for lib in "$ETHERCAT_DIR/lib/.libs/libethercat.so" \
                   "$ETHERCAT_DIR/lib/.libs/libethercat.a" \
                   "$ETHERCAT_DIR/install/lib/libethercat.so"; do
            if [ -f "$lib" ]; then
                LIB_EXISTS=true
                break
            fi
        done

        if [ "$LIB_EXISTS" = true ]; then
            ok "libethercat already built"
        else
            hdr "Building libethercat"
            cd "$ETHERCAT_DIR"

            # Bootstrap if configure script is missing (fresh checkout)
            if [ ! -f configure ]; then
                echo "Running bootstrap..."
                ./bootstrap
                ok "Bootstrap complete"
            fi

            # Configure: user-space library only, no kernel modules
            echo "Configuring libethercat..."
            ./configure --disable-kernel --disable-tool --disable-tty \
                        --prefix="$ETHERCAT_DIR/install" 2>&1

            # Build and install locally
            JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
            echo "Building with $JOBS jobs..."
            make -j"$JOBS" 2>&1
            make install 2>&1
            ok "libethercat built → $ETHERCAT_DIR/install/"

            cd "$SCRIPT_DIR"
        fi
    else
        warn "thirdparty/ethercat not found — EtherCAT adapter will be stub-only"
        warn "Fix: git submodule update --init --recursive"
    fi
fi

# ---- 3. Clean ------------------------------------------------------------
if [ "$DO_CLEAN" = true ]; then
    hdr "Clean"
    rm -rf "$BUILD_DIR"
    ok "Removed $BUILD_DIR/"
fi

# ---- 4. CMake configure --------------------------------------------------
if [ "$DO_BUILD" = true ]; then
    hdr "CMake Configure ($BUILD_TYPE)"
    mkdir -p "$BUILD_DIR"

    # Auto-detect generator mismatch (e.g. old "Unix Makefiles" vs current "Ninja")
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        CACHED_GENERATOR=$(grep -o 'CMAKE_GENERATOR:STRING=.*' "$BUILD_DIR/CMakeCache.txt" \
            | cut -d= -f2- || true)
        if [ "$CACHED_GENERATOR" != "Ninja" ]; then
            warn "Build dir has stale generator ('$CACHED_GENERATOR') — cleaning $BUILD_DIR"
            rm -rf "$BUILD_DIR"
            mkdir -p "$BUILD_DIR"
        fi
    fi

    JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G Ninja \
        -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
        2>&1

    ok "CMake configured"
fi

# ---- 5. Compile ----------------------------------------------------------
if [ "$DO_BUILD" = true ]; then
    hdr "Compile"
    cmake --build "$BUILD_DIR" --parallel "$JOBS" 2>&1
    ok "Build complete → $BUILD_DIR/"

    # Show built executables
    echo ""
    echo "Executables:"
    for exe in "$BUILD_DIR"/src/main/drone_app \
               "$BUILD_DIR"/src/main/bench_test \
               "$BUILD_DIR"/src/main/mission_bench_test; do
        if [ -f "$exe" ]; then
            echo "  ✓ $exe"
        fi
    done
fi

# ---- 6. Tests ------------------------------------------------------------
if [ "$DO_TESTS" = true ]; then
    hdr "Tests"
    cd "$BUILD_DIR"
    ctest --output-on-failure --parallel "$JOBS" 2>&1
    TEST_EXIT=$?
    cd "$SCRIPT_DIR"

    if [ $TEST_EXIT -eq 0 ]; then
        ok "All tests passed"
    else
        err "Tests failed (exit code $TEST_EXIT)"
        exit 1
    fi
fi

# ---- 7. Static analysis --------------------------------------------------
if [ "$DO_ANALYSIS" = true ]; then
    hdr "Static Analysis"

    if command -v cppcheck &>/dev/null; then
        echo "Running cppcheck..."
        cppcheck --enable=warning,performance,portability,style \
                 --std=c++20 \
                 --error-exitcode=1 \
                 --quiet \
                 --template="{file}:{line}: {severity}: {message}" \
                 --suppress=missingIncludeSystem \
                 src/ 2>&1 || warn "cppcheck found issues"
        ok "cppcheck complete"
    else
        warn "cppcheck not found — install: ./build.sh --install --dev"
    fi

    if command -v clang-tidy &>/dev/null; then
        echo "Running clang-tidy..."
        run-clang-tidy -p "$BUILD_DIR" $CLANG_TIDY_FIX 2>&1 || warn "clang-tidy found issues"
        ok "clang-tidy complete"
    else
        warn "clang-tidy not found — install: ./build.sh --install --dev"
    fi
fi

# ---- Done ----------------------------------------------------------------
hdr "Done"
echo ""
echo "Summary:  type=$BUILD_TYPE  dir=$BUILD_DIR"
echo "          analysis=$([ "$DO_ANALYSIS" = true ] && echo yes || echo no)  tests=$([ "$DO_TESTS" = true ] && echo yes || echo no)"
echo ""
if [ "$DO_BUILD" = true ]; then
    echo "Run:"
    echo "  $BUILD_DIR/src/main/drone_app              # Default config"
    echo "  $BUILD_DIR/src/main/bench_test             # Sim-only bench"
    echo "  cd $BUILD_DIR && ctest --output-on-failure  # Run tests"
fi
