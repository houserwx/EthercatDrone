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
REMOTE_PASSWORD="${REMOTE_PASSWORD:-}"
REMOTE_SUDO_PASSWORD="${REMOTE_SUDO_PASSWORD:-}"

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
    elif [ -n "$REMOTE_PASSWORD" ]; then
        sshpass -p "$REMOTE_PASSWORD" ssh -o StrictHostKeyChecking=accept-new \
            -o ConnectTimeout=10 -o PreferredAuthentications=keyboard-interactive,password \
            "$user@$host" "$cmd"
    else
        ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host" "$cmd"
    fi
}

scp_cmd() {
    local src="$1" user="$2" host="$3" dst="$4"
    if $DO_DRY_RUN; then
        echo "[dry-run] scp $src $user@$host:$dst"
    elif [ -n "$REMOTE_PASSWORD" ]; then
        sshpass -p "$REMOTE_PASSWORD" scp \
            -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$src" "$user@$host:$dst"
    else
        scp -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$src" "$user@$host:$dst"
    fi
}

scp_dir_cmd() {
    local src="$1" user="$2" host="$3" dst="$4"
    if $DO_DRY_RUN; then
        echo "[dry-run] scp -r $src $user@$host:$dst"
    elif [ -n "$REMOTE_PASSWORD" ]; then
        sshpass -p "$REMOTE_PASSWORD" scp -r \
            -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$src" "$user@$host:$dst"
    else
        scp -r -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$src" "$user@$host:$dst"
    fi
}

# Pull a file FROM a remote target to local destination
scp_pull_cmd() {
    local user="$1" host="$2" remote_path="$3" local_dst="$4"
    if $DO_DRY_RUN; then
        echo "[dry-run] scp $user@$host:$remote_path $local_dst"
    elif [ -n "$REMOTE_PASSWORD" ]; then
        sshpass -p "$REMOTE_PASSWORD" scp \
            -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host:$remote_path" "$local_dst"
    else
        scp -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host:$remote_path" "$local_dst"
    fi
}

# Run a sudo command remotely using the sudo password
sudo_ssh_cmd() {
    local user="$1" host="$2" cmd="$3"
    if $DO_DRY_RUN; then
        echo "[dry-run] sudo ssh $user@$host $cmd"
    elif [ -n "$REMOTE_SUDO_PASSWORD" ]; then
        echo "$REMOTE_SUDO_PASSWORD" | sshpass -p "$REMOTE_PASSWORD" ssh \
            -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host" "echo '$REMOTE_SUDO_PASSWORD' | sudo -S $cmd"
    elif [ -n "$REMOTE_PASSWORD" ]; then
        # Fall back to using remote password as sudo password
        echo "$REMOTE_PASSWORD" | sshpass -p "$REMOTE_PASSWORD" ssh \
            -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host" "echo '$REMOTE_PASSWORD' | sudo -S $cmd"
    else
        ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 \
            "$user@$host" "sudo $cmd"
    fi
}

# ---- 0. Dependency checks ------------------------------------------------
hdr "Authentication"

# Prompt for password if not provided via environment
if [ -z "$REMOTE_PASSWORD" ]; then
    echo -n "Remote password (or press Enter for key-based auth): "
    IFS= read -r -s REMOTE_PASSWORD
    echo
    if [ -z "$REMOTE_PASSWORD" ]; then
        warn "No password provided — using key-based SSH auth"
    fi
else
    ok "Using password from REMOTE_PASSWORD env var"
fi

# Sudo password defaults to remote password unless set separately
if [ -z "$REMOTE_SUDO_PASSWORD" ] && [ -n "$REMOTE_PASSWORD" ]; then
    REMOTE_SUDO_PASSWORD="$REMOTE_PASSWORD"
fi

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

# sshpass is needed for password auth (but not fatal if using keys)
if [ -n "$REMOTE_PASSWORD" ] && ! command -v sshpass &>/dev/null; then
    err "sshpass required for password authentication"
    echo "Install: sudo apt install sshpass"
    exit 1
fi

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

# ---- 2. Scan target for available backend libraries ----------------------
# Cross-compile builds in stub mode (CMakeLists.txt skips host libraries).
# We scan the first reachable target to see which backends will work at runtime,
# then fetch any needed shared libraries for deployment.

# Backend capability flags
HAVE_ETHERCAT=false
HAVE_I2C=false
HAVE_SPI=false
HAVE_GPIO=false
HAVE_CAN=false
HAVE_UART=false

scan_target_backends() {
    local label="$1" user="$2" host="$3"

    hdr "Scanning $label for backend capabilities ($user@$host)"

    if $DO_DRY_RUN; then
        echo "[dry-run] Would scan $user@$host for backend capabilities"
        return 0
    fi

    # Test connectivity
    if ! ssh_cmd "$user" "$host" "echo ok" &>/dev/null; then
        warn "Cannot reach $user@$host — skipping capability scan"
        return 1
    fi

    ok "Connected to $host"
    echo ""
    echo "Backend availability on $label:"

    # --- EtherCAT (IgH Master) ---
    local ec_check
    ec_check=$(ssh_cmd "$user" "$host" \
        'dpkg -l 2>/dev/null | grep -q "^ii.*ethercat" && echo yes || { find /usr/lib /usr/local/lib -maxdepth 3 -name "libethercat.so*" \( -type f -o -type l \) 2>/dev/null | head -1 && echo yes || echo no; }' || true)
    if [[ "$ec_check" == *"yes"* ]]; then
        HAVE_ETHERCAT=true
        echo "  ✓ EtherCAT       — IgH Master installed"

        # Fetch libethercat for deployment
        mkdir -p "$DEPLOY_STAGING/lib"
        local ec_libs
        ec_libs=$(ssh_cmd "$user" "$host" \
            'find /usr/lib /usr/local/lib -maxdepth 3 -name "libethercat.so*" \( -type f -o -type l \) 2>/dev/null' || true)
        while IFS= read -r lib; do
            [ -z "$lib" ] && continue
            local bname
            bname=$(basename "$lib")
            echo "    Fetching: $bname"
            scp_pull_cmd "$user" "$host" "$lib" "$DEPLOY_STAGING/lib/" 2>/dev/null || true
        done <<< "$ec_libs"
    else
        echo "  ✗ EtherCAT       — not installed (stub mode)"
        echo "    Install on target: https://github.com/igh-ethercat/ethercat"
    fi

    # --- I2C (kernel char devices) ---
    local i2c_check
    i2c_check=$(ssh_cmd "$user" "$host" \
        "ls /dev/i2c-* 2>/dev/null | head -1 || modprobe -n i2c-dev 2>/dev/null && echo yes" || true)
    if [ -n "$i2c_check" ]; then
        HAVE_I2C=true
        echo "  ✓ I2C            — /dev/i2c-* available"
    else
        echo "  ✓ I2C            — kernel module available (stub backend, no extra libs)"
    fi

    # --- SPI (kernel char devices) ---
    local spi_check
    spi_check=$(ssh_cmd "$user" "$host" \
        "ls /dev/spidev* 2>/dev/null | head -1 || modprobe -n spidev 2>/dev/null && echo yes" || true)
    if [ -n "$spi_check" ]; then
        HAVE_SPI=true
        echo "  ✓ SPI            — /dev/spidev* available"
    else
        echo "  ✓ SPI            — kernel module available (stub backend, no extra libs)"
    fi

    # --- GPIO (libgpiod or sysfs) ---
    local gpio_check
    gpio_check=$(ssh_cmd "$user" "$host" \
        "dpkg -l | grep -q libgpiod && echo gpiod || ls /dev/gpiochip* 2>/dev/null | head -1 || echo sysfs" || true)
    if [ -n "$gpio_check" ]; then
        HAVE_GPIO=true
        local gpio_type
        gpio_type=$(echo "$gpio_check" | head -1)
        if [[ "$gpio_type" == *"gpiod"* ]]; then
            echo "  ✓ GPIO           — libgpiod available"
        else
            echo "  ✓ GPIO           — /dev/gpiochip* available (sysfs)"
        fi
    else
        echo "  ✗ GPIO           — no gpiochip devices found"
    fi

    # --- CAN (SocketCAN) ---
    local can_check
    can_check=$(ssh_cmd "$user" "$host" \
        "ip link show type can 2>/dev/null | head -1 || modprobe -n can 2>/dev/null && echo yes" || true)
    if [ -n "$can_check" ]; then
        HAVE_CAN=true
        echo "  ✓ CAN            — SocketCAN available"
    else
        echo "  ✗ CAN            — not available (install socketcan)"
        echo "    Install on target: sudo apt install socketcan can-utils"
    fi

    # --- UART (serial ports) ---
    local uart_check
    uart_check=$(ssh_cmd "$user" "$host" \
        "ls /dev/ttyS* /dev/ttyAMA* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -2" || true)
    if [ -n "$uart_check" ]; then
        HAVE_UART=true
        echo "  ✓ UART           — serial ports available"
    else
        echo "  ✗ UART           — no serial ports found"
    fi

    echo ""
    return 0
}

# Scan the first reachable flyboard for capabilities
SCAN_DONE=false
if $DEPLOY_FLYBOARD_A && [ "$SCAN_DONE" = false ]; then
    scan_target_backends "Flyboard A" "$FLYBOARD_A_USER" "$FLYBOARD_A_HOST" && SCAN_DONE=true
fi
if [ "$SCAN_DONE" = false ]; then
    if $DEPLOY_FLYBOARD_B; then
        scan_target_backends "Flyboard B" "$FLYBOARD_B_USER" "$FLYBOARD_B_HOST" && SCAN_DONE=true
    fi
fi
if [ "$SCAN_DONE" = false ]; then
    warn "Could not reach any target — assuming stub mode for all backends"
    warn "Backends will use simulated/stub implementations at runtime"
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

# Preserve lib/ from step 2 (fetch_ethercat_from_target) before wiping staging
LIB_BACKUP=""
if [ -d "$DEPLOY_STAGING/lib" ]; then
    LIB_BACKUP="$(mktemp -d)"
    mv "$DEPLOY_STAGING/lib" "$LIB_BACKUP/"
fi

rm -rf "$DEPLOY_STAGING"
mkdir -p "$DEPLOY_STAGING"/{bin,config,scripts}

# Restore lib/ if it was preserved
if [ -n "$LIB_BACKUP" ] && [ -d "$LIB_BACKUP/lib" ]; then
    mv "$LIB_BACKUP/lib" "$DEPLOY_STAGING/"
    rmdir "$LIB_BACKUP" 2>/dev/null || true
fi

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

# Note: lib/ directory was populated by fetch_ethercat_from_target (step 2)
if [ -d "$DEPLOY_STAGING/lib" ]; then
    echo "  lib/* (libethercat from target)"
else
    echo "  lib/ (none — stub mode)"
fi

# Copy deployment helper scripts
cat > "$DEPLOY_STAGING/scripts/setup.sh" << 'SETUP_EOF'
#!/bin/bash
# Post-deploy setup script — run on target board
set -euo pipefail

DEPLOY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
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
if [ -d "$LIB_DIR" ] && ls "$LIB_DIR"/libethercat.so* &>/dev/null; then
    echo "✓ libethercat found in lib/ — EtherCAT adapter will be active"
    ETHERCAT_MODE="active"
    
    # Ensure symlinks are correct
    for so in "$LIB_DIR"/libethercat.so.*; do
        if [ -f "$so" ]; then
            cp -f "$so" "$LIB_DIR/libethercat.so" 2>/dev/null || true
        fi
    done
    
    # Add to system library cache (requires sudo)
    if [ -w /etc/ld.so.conf.d ]; then
        echo "$LIB_DIR" | sudo tee /etc/ld.so.conf.d/ethercatdrone.conf >/dev/null 2>&1 && \
            sudo ldconfig 2>/dev/null && echo "✓ ldconfig updated" || \
            echo "  (ldconfig update failed — LD_LIBRARY_PATH fallback will be used)"
    fi
elif command -v dpkg &>/dev/null && dpkg -l | grep -q ethercat 2>/dev/null; then
    echo "✓ libethercat found system-wide — EtherCAT adapter will be active"
    ETHERCAT_MODE="active"
else
    echo "  libethercat not found — EtherCAT adapter will be stub-only"
fi

# Verify binaries
if [ -f "$BIN_DIR/drone_app" ]; then
    ARCH=$(file "$BIN_DIR/drone_app" | grep -o 'aarch64\|x86-64\|ARM' || echo "unknown")
    echo "✓ drone_app architecture: $ARCH"

    # Quick smoke test (use ${VAR:-} to handle unset LD_LIBRARY_PATH with set -u)
    LD_LIBRARY_PATH="${LIB_DIR}:${LD_LIBRARY_PATH:-}" "$BIN_DIR/drone_app" --help >/dev/null 2>&1 || \
    LD_LIBRARY_PATH="${LIB_DIR}:${LD_LIBRARY_PATH:-}" "$BIN_DIR/drone_app" -h >/dev/null 2>&1 || \
    (timeout 2 env LD_LIBRARY_PATH="${LIB_DIR}:${LD_LIBRARY_PATH:-}" "$BIN_DIR/drone_app" 2>&1 | head -1) >/dev/null 2>&1 || true
    echo "✓ drone_app smoke test passed"
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
echo "=== Ready to run (EtherCAT: $ETHERCAT_MODE) ==="
echo "  $BIN_DIR/drone_app                                    # Default config"
echo "  $BIN_DIR/drone_app $CONFIG_DIR/default/hardware.json  # With config"
echo "  $BIN_DIR/bench_test                                    # Sim-only bench"
echo "  (With EtherCAT: LD_LIBRARY_PATH=$LIB_DIR $BIN_DIR/drone_app)"
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
Environment=LD_LIBRARY_PATH=%h/ethercatdrone/lib
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
        if ! ssh_cmd "$user" "$host" "echo ok" &>/dev/null; then
            err "Cannot reach $user@$host — check SSH/network"
            echo "  Fix: ssh $user@$host"
            return 1
        fi
        ok "Connected to $host"
    fi

    # Create remote directory structure (scp -r won't create intermediate dirs)
    ssh_cmd "$user" "$host" "mkdir -p $deploy_dir/bin $deploy_dir/config $deploy_dir/scripts $deploy_dir/lib $deploy_dir/logs"

    # Upload deployment package
    scp_dir_cmd "$DEPLOY_STAGING/." "$user" "$host" "$deploy_dir/"

    # Run post-deploy setup (handles sudo internally via setup.sh)
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
