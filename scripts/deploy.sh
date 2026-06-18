#!/bin/bash
# deploy.sh — Cross-compile and deploy EtherCatDrone to target boards
#
# Cross-compiles for aarch64 (ARM64) and deploys via SSH to flyboards
# and/or the companion board.
#
# Usage:
#   ./scripts/deploy.sh --flyboard-a     # Build + deploy to Flyboard A
#   ./scripts/deploy.sh --flyboard-b     # Build + deploy to Flyboard B
#   ./scripts/deploy.sh --companion      # Build + deploy to Companion board
#   ./scripts/deploy.sh --all            # Build once, deploy to all targets
#   ./scripts/deploy.sh --dry-run        # Show what would happen (no deploy)
#   ./scripts/deploy.sh --clean          # Clean cross-compile build dir first
#   ./scripts/deploy.sh --test           # Run host tests before deploy
#   ./scripts/deploy.sh --help           # Show this help
#
# Prerequisites:
#   - aarch64 cross-compile toolchain installed:
#       sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#   - SSH keys configured for target boards (recommended over passwords):
#       ssh-copy-id vis@<target-ip>
#
# Target configuration:
#   Edit the TARGET_* variables below or pass overrides via environment.

set -euo pipefail

# ---- Paths ---------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

# ---- Target configuration (override via env vars) -----------------------
# Flyboard A — Primary RT master
FLYBOARD_A_HOST="${FLYBOARD_A_HOST:-192.168.254.192}"
FLYBOARD_A_USER="${FLYBOARD_A_USER:-vis}"
FLYBOARD_A_DEPLOY_DIR="${FLYBOARD_A_DEPLOY_DIR:-/home/vis/ethercatdrone}"

# Flyboard B — Hot-standby RT master
FLYBOARD_B_HOST="${FLYBOARD_B_HOST:-192.168.254.193}"
FLYBOARD_B_USER="${FLYBOARD_B_USER:-vis}"
FLYBOARD_B_DEPLOY_DIR="${FLYBOARD_B_DEPLOY_DIR:-/home/vis/ethercatdrone}"

# Companion — UI / Vision / Targeting
COMPANION_HOST="${COMPANION_HOST:-192.168.254.194}"
COMPANION_USER="${COMPANION_USER:-vis}"
COMPANION_DEPLOY_DIR="${COMPANION_DEPLOY_DIR:-/home/vis/ethercatdrone}"

# Deployment defaults
DEPLOY_DIR_NAME="${DEPLOY_DIR_NAME:-deploy}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
SKIP_TESTS="${SKIP_TESTS:-false}"

# ---- Flags ---------------------------------------------------------------
DEPLOY_FLYBOARD_A=false
DEPLOY_FLYBOARD_B=false
DEPLOY_COMPANION=false
DO_DRY_RUN=false
DO_CLEAN=false
DO_HOST_TESTS=false

# ---- Argument parsing ----------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        --flyboard-a)   DEPLOY_FLYBOARD_A=true ;;
        --flyboard-b)   DEPLOY_FLYBOARD_B=true ;;
        --companion)    DEPLOY_COMPANION=true ;;
        --all)          DEPLOY_FLYBOARD_A=true; DEPLOY_FLYBOARD_B=true; DEPLOY_COMPANION=true ;;
        --dry-run)      DO_DRY_RUN=true ;;
        --clean)        DO_CLEAN=true ;;
        --test)         DO_HOST_TESTS=true ;;
        --help|-h)
            sed -n '4,20p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            echo "Run '$0 --help' for usage." >&2
            exit 1
            ;;
    esac
done

# Default to --all if no target specified
if ! $DEPLOY_FLYBOARD_A && ! $DEPLOY_FLYBOARD_B && ! $DEPLOY_COMPANION; then
    DEPLOY_FLYBOARD_A=true
    DEPLOY_FLYBOARD_B=true
    DEPLOY_COMPANION=true
    echo "No target specified — defaulting to --all (flyboard-a + flyboard-b + companion)"
fi

# ---- Build configuration -------------------------------------------------
if [ "$BUILD_TYPE" = "Debug" ]; then
    CROSS_BUILD_DIR="build/debug-arm"
else
    CROSS_BUILD_DIR="build/release-arm"
fi

DEPLOY_STAGING="$ROOT_DIR/$DEPLOY_DIR_NAME"

# ---- Colours -------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
ok()   { echo -e "${GREEN}✓${NC} $*"; }
warn() { echo -e "${YELLOW}⚠${NC} $*"; }
err()  { echo -e "${RED}✗${NC} $*"; }
hdr()  { echo -e "\n${BLUE}══════════════════════════════════════════${NC}"
          echo -e "${BLUE}  $*${NC}"
          echo -e "${BLUE}══════════════════════════════════════════${NC}"; }

# ---- Helpers -------------------------------------------------------------
ssh_cmd() {
    local user="$1" host="$2" cmd="$3"
    if $DO_DRY_RUN; then
        echo "[dry-run] ssh $user@$host $cmd"
    else
        ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host" "$cmd"
    fi
}

scp_cmd() {
    local src="$1" user="$2" host="$3" dst="$4"
    if $DO_DRY_RUN; then
        echo "[dry-run] scp $src $user@$host:$dst"
    else
        scp -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$src" "$user@$host:$dst"
    fi
}

scp_dir_cmd() {
    local src="$1" user="$2" host="$3" dst="$4"
    if $DO_DRY_RUN; then
        echo "[dry-run] scp -r $src $user@$host:$dst"
    else
        scp -r -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$src" "$user@$host:$dst"
    fi
}

# ---- 0. Dependency checks ------------------------------------------------
hdr "Checking Dependencies"

check_cmd() {
    if ! command -v "$1" &>/dev/null; then
        err "$1 not found — run: ./build.sh --install"
        exit 1
    fi
}

check_cmd cmake
check_cmd ninja
check_cmd ssh
check_cmd scp

# Cross-compile toolchain
if ! command -v aarch64-linux-gnu-g++ &>/dev/null; then
    err "aarch64 cross-compiler not found"
    echo "Install: sudo apt install g++-aarch64-linux-gnu gcc-aarch64-linux-gnu"
    exit 1
fi
ok "Cross-compiler: aarch64-linux-gnu-g++"

# ---- 1. Clean ------------------------------------------------------------
if $DO_CLEAN; then
    hdr "Clean Cross-Build"
    rm -rf "$CROSS_BUILD_DIR"
    rm -rf "$DEPLOY_STAGING"
    ok "Cleaned $CROSS_BUILD_DIR and $DEPLOY_DIR_NAME/"
fi

# ---- 2. Cross-compile libethercat ----------------------------------------
# For cross-compilation, we build libethercat in stub mode on the host,
# or skip if the target has it pre-installed. The CMake configure step
# will fall back to stub mode gracefully.
ETHERCAT_DIR="$ROOT_DIR/thirdparty/ethercat"
if [ -d "$ETHERCAT_DIR" ]; then
    # Check if already cross-compiled (look in install dir)
    if [ -f "$ETHERCAT_DIR/install/lib/libethercat.so" ]; then
        ok "libethercat available (stub mode will be used for cross-compile)"
    else
        # Build user-space only — this is architecture-independent for stub mode
        # For real EtherCAT on target, the library should be pre-installed
        warn "libethercat not built — will use stub mode for cross-compile"
        warn "For real EtherCAT on target, install ethercat on the board first"
    fi
fi

# ---- 3. Cross-compile with CMake -----------------------------------------
hdr "Cross-Compile ($BUILD_TYPE, aarch64)"
mkdir -p "$CROSS_BUILD_DIR"

# Check for stale generator
if [ -f "$CROSS_BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_GENERATOR=$(grep -o 'CMAKE_GENERATOR:STRING=.*' "$CROSS_BUILD_DIR/CMakeCache.txt" \
        | cut -d= -f2- || true)
    if [ "$CACHED_GENERATOR" != "Ninja" ]; then
        warn "Stale generator in $CROSS_BUILD_DIR — cleaning"
        rm -rf "$CROSS_BUILD_DIR"
        mkdir -p "$CROSS_BUILD_DIR"
    fi
fi

JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

cmake -S . -B "$CROSS_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/aarch64-linux-gnu.cmake" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
    2>&1

ok "CMake cross-configure complete"

cmake --build "$CROSS_BUILD_DIR" --parallel "$JOBS" 2>&1
ok "Cross-compile complete → $CROSS_BUILD_DIR/"

# Verify binaries are actually aarch64
for exe in "$CROSS_BUILD_DIR"/src/main/drone_app \
           "$CROSS_BUILD_DIR"/src/main/bench_test \
           "$CROSS_BUILD_DIR"/src/main/mission_bench_test; do
    if [ -f "$exe" ]; then
        ARCH=$(file "$exe" 2>/dev/null | grep -o 'aarch64\|ARM' || echo "unknown")
        echo "  ✓ $exe ($ARCH)"
    fi
done

# ---- 4. Host tests (optional) --------------------------------------------
if $DO_HOST_TESTS && ! $SKIP_TESTS; then
    hdr "Host Tests (x86_64)"
    HOST_BUILD_DIR="build/debug-linux"

    if [ -d "$HOST_BUILD_DIR" ]; then
        cd "$HOST_BUILD_DIR"
        ctest --output-on-failure --parallel "$JOBS" 2>&1 || warn "Some tests failed"
        cd "$ROOT_DIR"
        ok "Host tests complete"
    else
        warn "No host build found at $HOST_BUILD_DIR — skipping tests"
        warn "Run: ./build.sh --test first"
    fi
fi

# ---- 5. Stage deployment -------------------------------------------------
hdr "Staging Deployment"
rm -rf "$DEPLOY_STAGING"
mkdir -p "$DEPLOY_STAGING"/{bin,config,scripts}

# Copy binaries
for exe in drone_app bench_test mission_bench_test; do
    if [ -f "$CROSS_BUILD_DIR/src/main/$exe" ]; then
        cp "$CROSS_BUILD_DIR/src/main/$exe" "$DEPLOY_STAGING/bin/"
        echo "  bin/$exe"
    fi
done

# Copy config files
if [ -d "$ROOT_DIR/config" ]; then
    cp -r "$ROOT_DIR/config/"* "$DEPLOY_STAGING/config/" 2>/dev/null || true
    echo "  config/*"
fi

# Copy deployment helper scripts
cat > "$DEPLOY_STAGING/scripts/setup.sh" << 'SETUP_EOF'
#!/bin/bash
# Post-deploy setup script — run on target board
set -euo pipefail

DEPLOY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$DEPLOY_DIR/bin"
CONFIG_DIR="$DEPLOY_DIR/config"

echo "=== EtherCatDrone Deploy Setup ==="
echo "Deploy dir: $DEPLOY_DIR"

# Make binaries executable
chmod +x "$BIN_DIR"/*
echo "✓ Binaries marked executable"

# Create log directory
mkdir -p "$DEPLOY_DIR/logs"
echo "✓ Log directory created"

# Verify binaries
if [ -f "$BIN_DIR/drone_app" ]; then
    ARCH=$(file "$BIN_DIR/drone_app" | grep -o 'aarch64\|x86-64\|ARM' || echo "unknown")
    echo "✓ drone_app architecture: $ARCH"
    
    # Quick smoke test
    if "$BIN_DIR/drone_app" --help >/dev/null 2>&1 || \
       "$BIN_DIR/drone_app" -h >/dev/null 2>&1 || \
       (timeout 2 "$BIN_DIR/drone_app" 2>&1 | head -1) >/dev/null 2>&1; then
        echo "✓ drone_app smoke test passed"
    else
        echo "  (binary exists but may need runtime deps)"
    fi
else
    echo "✗ drone_app not found"
    exit 1
fi

# Show config files
if [ -d "$CONFIG_DIR" ]; then
    echo ""
    echo "Config files:"
    find "$CONFIG_DIR" -type f -name "*.json" | while read -r f; do
        echo "  $f"
    done
fi

echo ""
echo "=== Ready to run ==="
echo "  $BIN_DIR/drone_app                                    # Default config"
echo "  $BIN_DIR/drone_app $CONFIG_DIR/default/hardware.json  # With config"
echo "  $BIN_DIR/bench_test                                    # Sim-only bench"
SETUP_EOF
chmod +x "$DEPLOY_STAGING/scripts/setup.sh"

# Create a systemd service template
cat > "$DEPLOY_STAGING/scripts/ethercatdrone.service" << 'SERVICE_EOF'
[Unit]
Description=EtherCatDrone Real-Time Flight Controller
After=network.target
Wants=network.target

[Service]
Type=simple
User=vis
Group=vis
WorkingDirectory=%h/ethercatdrone
ExecStart=%h/ethercatdrone/bin/drone_app %h/ethercatdrone/config/default/hardware.json
Restart=on-failure
RestartSec=3
StandardOutput=journal
StandardError=journal
SyslogIdentifier=ethercatdrone

# Real-time scheduling (requires capability)
# AmbientCapabilities=CAP_SYS_NICE
# LimitRTPRIO=99
# LimitRTTIME=infinity

# Memory lock for RT thread
# LockPersonality=yes

[Install]
WantedBy=multi-user.target
SERVICE_EOF

ok "Staging complete → $DEPLOY_STAGING/"
echo ""
echo "Staged contents:"
find "$DEPLOY_STAGING" -type f | while read -r f; do
    echo "  ${f#$ROOT_DIR/}"
done

# ---- 6. Deploy to targets ------------------------------------------------

deploy_to_host() {
    local label="$1" user="$2" host="$3" deploy_dir="$4"

    hdr "Deploy → $label ($user@$host)"

    # Test connectivity
    if ! $DO_DRY_RUN; then
        if ! ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=5 \
            "$user@$host" "echo ok" &>/dev/null; then
            err "Cannot reach $user@$host — check SSH/network"
            echo "  Fix: ssh $user@$host"
            return 1
        fi
        ok "Connected to $host"
    fi

    # Create remote directory
    ssh_cmd "$user" "$host" "mkdir -p '$deploy_dir'"

    # Upload deployment package
    scp_dir_cmd "$DEPLOY_STAGING/." "$user" "$host" "$deploy_dir/"

    # Run post-deploy setup
    ssh_cmd "$user" "$host" "bash '$deploy_dir/scripts/setup.sh'"

    ok "Deployed to $label → $deploy_dir"
}

if $DEPLOY_FLYBOARD_A; then
    deploy_to_host "Flyboard A" "$FLYBOARD_A_USER" "$FLYBOARD_A_HOST" "$FLYBOARD_A_DEPLOY_DIR"
fi

if $DEPLOY_FLYBOARD_B; then
    deploy_to_host "Flyboard B" "$FLYBOARD_B_USER" "$FLYBOARD_B_HOST" "$FLYBOARD_B_DEPLOY_DIR"
fi

if $DEPLOY_COMPANION; then
    deploy_to_host "Companion"  "$COMPANION_USER" "$COMPANION_HOST" "$COMPANION_DEPLOY_DIR"
fi

# ---- Done ----------------------------------------------------------------
hdr "Deploy Complete"
echo ""
echo "Summary:"
echo "  Build type:    $BUILD_TYPE"
echo "  Target arch:   aarch64"
echo "  Build dir:     $CROSS_BUILD_DIR/"
echo "  Deploy stage:  $DEPLOY_DIR_NAME/"
$DEPLOY_FLYBOARD_A && echo "  Flyboard A:    $FLYBOARD_A_USER@$FLYBOARD_A_HOST:$FLYBOARD_A_DEPLOY_DIR"
$DEPLOY_FLYBOARD_B && echo "  Flyboard B:    $FLYBOARD_B_USER@$FLYBOARD_B_HOST:$FLYBOARD_B_DEPLOY_DIR"
$DEPLOY_COMPANION  && echo "  Companion:     $COMPANION_USER@$COMPANION_HOST:$COMPANION_DEPLOY_DIR"
echo ""
echo "On target:"
echo "  cd $FLYBOARD_A_DEPLOY_DIR && bash scripts/setup.sh   # Verify deployment"
echo "  $FLYBOARD_A_DEPLOY_DIR/bin/drone_app                  # Run (default config)"
echo ""
echo "To enable systemd service:"
echo "  scp $DEPLOY_STAGING/scripts/ethercatdrone.service $FLYBOARD_A_USER@$FLYBOARD_A_HOST:/tmp/"
echo "  ssh $FLYBOARD_A_USER@$FLYBOARD_A_HOST 'sudo mv /tmp/ethercatdrone.service /etc/systemd/system/'"
echo "  ssh $FLYBOARD_A_USER@$FLYBOARD_A_HOST 'sudo systemctl daemon-reload && sudo systemctl enable --now ethercatdrone'"
echo ""
