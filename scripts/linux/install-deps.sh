#!/bin/bash
# install-deps.sh — Install build, dev, and optional runtime dependencies.
#
# Usage:
#   ./install-deps.sh                  # Build dependencies only
#   ./install-deps.sh --dev            # Build + dev (clang-tidy, cppcheck)
#   ./install-deps.sh --ethercat       # Build + IgH EtherCAT Master runtime
#   ./install-deps.sh --all            # Everything (--dev + --ethercat)

set -euo pipefail

INSTALL_DEV=false
INSTALL_ETHERCAT=false
for arg in "$@"; do
    case "$arg" in
        --dev)      INSTALL_DEV=true ;;
        --ethercat) INSTALL_ETHERCAT=true ;;
        --all)      INSTALL_DEV=true; INSTALL_ETHERCAT=true ;;
        *)
            echo "Usage: $0 [--dev] [--ethercat] [--all]" >&2
            exit 1
            ;;
    esac
done

sudo apt-get update

# ---------------------------------------------------------------------------
# 1. Build dependencies (always installed)
# ---------------------------------------------------------------------------
# Core build tools, JSON, UUID, gRPC/Protobuf, and autotools (for ethercat)
echo "=== Installing build dependencies ==="
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    nlohmann-json3-dev \
    uuid-dev \
    libgrpc-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    autoconf \
    automake \
    libtool

# ---------------------------------------------------------------------------
# 2. Dev dependencies (clang-tidy, cppcheck, debuggers)
# ---------------------------------------------------------------------------
if [ "$INSTALL_DEV" = true ]; then
    echo "=== Installing dev dependencies ==="
    sudo apt-get install -y \
        clang-tidy \
        clang-tools \
        cppcheck \
        gdb \
        strace \
        valgrind
fi

# ---------------------------------------------------------------------------
# 3. IgH EtherCAT Master runtime (optional — real hardware only)
# ---------------------------------------------------------------------------
if [ "$INSTALL_ETHERCAT" = true ]; then
    echo "=== Installing EtherCAT dependencies ==="

    # Install kernel headers (needed for EtherCAT kernel module)
    ETHERCAT_HEADERS="linux-headers-$(uname -r)"
    if sudo apt-get install -y "$ETHERCAT_HEADERS" 2>/dev/null; then
        echo "Kernel headers installed: $ETHERCAT_HEADERS"
    else
        echo "Warning: Could not install $ETHERCAT_HEADERS"
        echo "  You may need to install manually for your kernel version"
        echo "  Run: sudo apt install linux-headers-\$(uname -r)"
    fi

    sudo apt-get install -y udev
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
echo "=== Dependencies installed ==="
[ "$INSTALL_DEV" = true ] && echo "  [x] Build + dev tools" || echo "  [x] Build only"
[ "$INSTALL_ETHERCAT" = true ] && echo "  [x] EtherCAT runtime" || echo "  [ ] EtherCAT runtime (run with --ethercat)"
echo ""
echo "Next steps:"
echo "  ./build.sh                  # Build the project"
echo "  ./build.sh --dev            # Build + static analysis"
echo "  cd build/debug-linux && ctest --output-on-failure  # Run tests"
