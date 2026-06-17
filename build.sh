#!/bin/bash
# build.sh — Build and optional static analysis for EtherCatDrone
#
# Usage:
#   ./build.sh                   # Standard build
#   ./build.sh --dev             # Build + cppcheck + clang-tidy
#   ./build.sh --clean           # Clean build directory first
#   ./build.sh --install         # Install build (+ dev if --dev) dependencies
#   ./build.sh --build-dir=DIR   # Override build output directory
#   ./build.sh --analysis-only   # Skip build; run analysis on existing build dir
#   ./build.sh --fix             # Pass --fix to clang-tidy (applies safe fixes)
#   ./build.sh --help            # Show this help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---- Defaults --------------------------------------------------------
BUILD_DIR="build/debug-linux"
DO_BUILD=true
DO_ANALYSIS=false
DO_CLEAN=false
DO_INSTALL=false
CLANG_TIDY_FIX=""

# ---- Argument parsing ------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        --dev)           DO_ANALYSIS=true ;;
        --analysis-only) DO_BUILD=false; DO_ANALYSIS=true ;;
        --clean)         DO_CLEAN=true ;;
        --install)       DO_INSTALL=true ;;
        --fix)           CLANG_TIDY_FIX="--fix" ;;
        --build-dir=*)   BUILD_DIR="${arg#--build-dir=}" ;;
        --help|-h)
            sed -n '3,11p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 1
            ;;
    esac
done

# ---- Colours ---------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
ok()   { echo -e "${GREEN}✓${NC} $*"; }
warn() { echo -e "${YELLOW}⚠${NC} $*"; }
err()  { echo -e "${RED}✗${NC} $*"; }
hdr()  { echo -e "\n${BLUE}══════════════════════════════════════════${NC}"
          echo -e "${BLUE}  $*${NC}"
          echo -e "${BLUE}══════════════════════════════════════════${NC}"; }

# ---- 0. Install dependencies -----------------------------------------
if [ "$DO_INSTALL" = true ]; then
    INSTALL_SCRIPT="$SCRIPT_DIR/scripts/linux/install-deps.sh"
    if [ ! -f "$INSTALL_SCRIPT" ]; then
        echo "Install script not found: $INSTALL_SCRIPT" >&2
        exit 1
    fi
    chmod +x "$INSTALL_SCRIPT"
    INSTALL_ARGS=()
    [ "$DO_ANALYSIS" = true ] && INSTALL_ARGS+=("--dev")
    bash "$INSTALL_SCRIPT" "${INSTALL_ARGS[@]}"
fi

# ---- 1. Clean --------------------------------------------------------
if [ "$DO_CLEAN" = true ]; then
    hdr "Clean"
    rm -rf "$BUILD_DIR"
    ok "Removed $BUILD_DIR/"
fi

# ---- 2. Build --------------------------------------------------------
if [ "$DO_BUILD" = true ]; then
    hdr "Build"
    mkdir -p "$BUILD_DIR"

    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G Ninja 2>&1

    cmake --build "$BUILD_DIR" --parallel $(nproc) 2>&1

    ok "Build complete → $BUILD_DIR/"
fi

# ---- 3. Static analysis ----------------------------------------------
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
        warn "cppcheck not found — skipping"
    fi

    if command -v clang-tidy &>/dev/null; then
        echo "Running clang-tidy..."
        run-clang-tidy -p "$BUILD_DIR" $CLANG_TIDY_FIX 2>&1 || warn "clang-tidy found issues"
        ok "clang-tidy complete"
    else
        warn "clang-tidy not found — skipping"
    fi
fi

hdr "Done"
