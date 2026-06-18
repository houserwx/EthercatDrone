#!/bin/bash
# Post-deploy setup script — run on target board
set -euo pipefail

DEPLOY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$DEPLOY_DIR/bin"
CONFIG_DIR="$DEPLOY_DIR/config"
LIB_DIR="$DEPLOY_DIR/lib"

echo "=== EtherCatDrone Deploy Setup ==="
echo "Deploy dir: $DEPLOY_DIR"

# Make binaries executable
chmod +x "$BIN_DIR"/*
echo "✓ Binaries marked executable"

# Create log directory
mkdir -p "$DEPLOY_DIR/logs"
echo "✓ Log directory created"

# Configure libethercat library path
ETHERCAT_MODE="stub"

# Check bundled lib/ first (from deploy fetch)
if [ -d "$LIB_DIR" ] && ls "$LIB_DIR"/libethercat.so* &>/dev/null; then
    echo "✓ libethercat found in lib/ — EtherCAT adapter will be active"
    ETHERCAT_MODE="active"
    for so in "$LIB_DIR"/libethercat.so.*; do
        [ -f "$so" ] && cp -f "$so" "$LIB_DIR/libethercat.so" 2>/dev/null || true
    done
# Check system-wide (IgH Master installed on target)
elif ldconfig -p 2>/dev/null | grep -q libethercat; then
    echo "✓ libethercat found system-wide (ldconfig) — EtherCAT adapter will be active"
    ETHERCAT_MODE="active"
elif find /usr/lib /usr/local/lib -maxdepth 4 -name "libethercat.so" 2>/dev/null | grep -q .; then
    echo "✓ libethercat found on disk — EtherCAT adapter will be active"
    ETHERCAT_MODE="active"
else
    echo "✗ libethercat not found — EtherCAT adapter will be stub-only"
fi

# Verify binaries
if [ -f "$BIN_DIR/drone_app" ]; then
    ARCH=$(file "$BIN_DIR/drone_app" | grep -oE 'aarch64|x86-64|ARM' | head -1 || echo "unknown")
    echo "✓ drone_app architecture: $ARCH"
    if [ ! -x "$BIN_DIR/drone_app" ]; then
        echo "✗ drone_app is not executable"
        exit 1
    fi
    echo "✓ drone_app is executable"
else
    echo "✗ drone_app not found in $BIN_DIR"
    echo "  Contents of bin/:"
    ls -la "$BIN_DIR/" 2>/dev/null || echo "  (bin/ directory not found or empty)"
    echo "  Deploy dir contents:"
    ls -la "$DEPLOY_DIR/" 2>/dev/null || echo "  (deploy dir not found)"
    exit 1
fi

# Create launcher wrapper that sets CWD and LD_LIBRARY_PATH
cat > "$BIN_DIR/run.sh" << 'RUNNER_EOF'
#!/bin/bash
# Launcher: sets CWD to deploy root and LD_LIBRARY_PATH before running
declare -a ALL_ARGS=("$@")

# Figure out deploy root (two levels up: bin/run.sh → ../.. → deploy root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LIB_DIR="$DEPLOY_DIR/lib"

# Set LD_LIBRARY_PATH if lib/ exists and has .so files
if [ -d "$LIB_DIR" ] && ls "$LIB_DIR"/*.so* &>/dev/null; then
    export LD_LIBRARY_PATH="$LIB_DIR:${LD_LIBRARY_PATH:-}"
fi

# Change to deploy root so relative config paths work
cd "$DEPLOY_DIR" || exit 1

# Execute the binary with all passed arguments
exec "$SCRIPT_DIR/$1" "${ALL_ARGS[@]:1}"
RUNNER_EOF
chmod +x "$BIN_DIR/run.sh"
echo "✓ Launcher wrapper created at bin/run.sh"

# Show config files
if [ -d "$CONFIG_DIR" ]; then
    echo ""
    echo "Config files:"
    find "$CONFIG_DIR" -type f -name "*.json" | while read -r f; do
        echo "  $f"
    done
fi

echo ""
echo "=== Ready to run (EtherCAT: $ETHERCAT_MODE) ==="
echo "  $BIN_DIR/run.sh drone_app                                 # Default config"
echo "  $BIN_DIR/run.sh drone_app config/default/hardware.json   # With config"
echo "  $BIN_DIR/run.sh bench_test                                # Sim-only bench"
