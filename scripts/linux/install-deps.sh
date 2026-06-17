#!/bin/bash
# install-deps.sh — Install build and optional dev dependencies.
#
# Usage:
#   ./install-deps.sh          # Build dependencies only
#   ./install-deps.sh --dev    # Build + dev (clang-tidy, cppcheck) dependencies

set -euo pipefail

INSTALL_DEV=false
for arg in "$@"; do
    case "$arg" in
        --dev) INSTALL_DEV=true ;;
    esac
done

echo "Installing build dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    nlohmann-json3-dev \
    libgrpc-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    uuid-dev

if [ "$INSTALL_DEV" = true ]; then
    echo "Installing dev dependencies..."
    sudo apt-get install -y \
        clang-tidy \
        cppcheck \
        gdb \
        strace \
        valgrind
fi

echo "Dependencies installed."
