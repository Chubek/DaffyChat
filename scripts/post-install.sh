#!/bin/sh
# DaffyChat post-installation script
# Installs service files, config, directories.  Called by dpkg/rpm postinst
# and can also be run manually.
#
# Env overrides:
#   DAFFY_PREFIX         installation prefix  (default /usr/local)
#   DAFFY_CONFIG_FORMAT  json | conf          (default json)
#   DAFFY_CLIENT_ONLY    1 | 0                (default 0)
#   DAFFY_PACK_FRONTEND  1 | 0 – if 1 also run install-frontend.sh (default 1)
#   DAFFY_INSTALL_STDEXT 1 | 0 – if 1 also run stdext-helper.sh (default 0)

set -e

# POSIX-compatible way to get script directory
if [ -n "${BASH_SOURCE:-}" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fi

PREFIX="${DAFFY_PREFIX:-/usr/local}"
CONFIG_FORMAT="${DAFFY_CONFIG_FORMAT:-json}"
CLIENT_ONLY="${DAFFY_CLIENT_ONLY:-0}"
PACK_FRONTEND="${DAFFY_PACK_FRONTEND:-1}"
INSTALL_STDEXT="${DAFFY_INSTALL_STDEXT:-0}"

# ---------------------------------------------------------------------------
# Detect OS-specific paths via os-service.pl if available
# ---------------------------------------------------------------------------
if [ -x "$SCRIPT_DIR/os-service.pl" ]; then
    if [ "$CLIENT_ONLY" = "1" ]; then
        eval "$("$SCRIPT_DIR/os-service.pl" --prefix "$PREFIX" --config-format "$CONFIG_FORMAT" --client-only)"
    else
        eval "$("$SCRIPT_DIR/os-service.pl" --prefix "$PREFIX" --config-format "$CONFIG_FORMAT")"
    fi
else
    echo "Warning: os-service.pl not found – using hardcoded defaults"
    DAFFY_CONFIGDIR="/etc/daffychat"
    DAFFY_SYSTEMDDIR="/etc/systemd/system"
    DAFFY_LOGDIR="/var/log/daffychat"
    DAFFY_RUNDIR="/var/run/daffychat"
    DAFFY_STATEDIR="/var/lib/daffychat"
    DAFFY_DATADIR="${PREFIX}/share/daffychat"
    DAFFY_DOCDIR="${PREFIX}/share/doc/daffychat"
    DAFFY_CONFIGFILE="daffychat.${CONFIG_FORMAT}"
fi

# Package installations put data under /usr/share/daffychat, not prefix/share
[ -d "/usr/share/daffychat" ] && DAFFY_DATADIR="/usr/share/daffychat"

echo "DaffyChat Post-Installation"
echo "============================"
echo "Prefix         : $PREFIX"
echo "Data dir       : $DAFFY_DATADIR"
echo "Config dir     : $DAFFY_CONFIGDIR"
echo "Systemd dir    : $DAFFY_SYSTEMDDIR"
echo "Client-only    : $CLIENT_ONLY"
echo "Pack frontend  : $PACK_FRONTEND"
echo "Install stdext : $INSTALL_STDEXT"
echo ""

# ---------------------------------------------------------------------------
# Helper: create a directory if it doesn't exist
# ---------------------------------------------------------------------------
ensure_dir() {
    for dir in "$@"; do
        [ -z "$dir" ] && continue
        if [ ! -d "$dir" ]; then
            echo "  mkdir $dir"
            mkdir -p "$dir" || { echo "Warning: could not create $dir"; }
        fi
    done
}

# ---------------------------------------------------------------------------
# 1. Runtime directories (server only)
# ---------------------------------------------------------------------------
if [ "$CLIENT_ONLY" != "1" ]; then
    ensure_dir "$DAFFY_CONFIGDIR" "$DAFFY_LOGDIR" "$DAFFY_RUNDIR" "$DAFFY_STATEDIR"
    chmod 755 "$DAFFY_LOGDIR"  2>/dev/null || true
    chmod 755 "$DAFFY_RUNDIR"  2>/dev/null || true
    chmod 750 "$DAFFY_STATEDIR" 2>/dev/null || true
fi

# ---------------------------------------------------------------------------
# 2. Config file (server only, never overwrite existing config)
# ---------------------------------------------------------------------------
if [ "$CLIENT_ONLY" != "1" ] && [ -n "$DAFFY_CONFIGDIR" ]; then
    CONFIG_FILE="$DAFFY_CONFIGDIR/$DAFFY_CONFIGFILE"
    if [ ! -f "$CONFIG_FILE" ]; then
        EXAMPLE="$DAFFY_DATADIR/config/daffychat.example.${CONFIG_FORMAT}"
        if [ -f "$EXAMPLE" ]; then
            echo "Installing default config: $CONFIG_FILE"
            cp "$EXAMPLE" "$CONFIG_FILE"
            chmod 640 "$CONFIG_FILE"
        else
            echo "Warning: example config not found at $EXAMPLE"
        fi
    else
        echo "Config already present, skipping: $CONFIG_FILE"
    fi
fi

# ---------------------------------------------------------------------------
# 3. systemd service files (server only)
# ---------------------------------------------------------------------------
install_service() {
    local name="$1"
    local src="$DAFFY_DATADIR/scripts/${name}"
    local dst="$DAFFY_SYSTEMDDIR/${name}"

    if [ ! -f "$src" ]; then
        echo "Warning: service file not found: $src"
        return
    fi
    if [ -f "$dst" ]; then
        echo "Service already present, skipping: $dst"
        return
    fi
    echo "Installing systemd service: $dst"
    cp "$src" "$dst"
    chmod 644 "$dst"
}

if [ "$CLIENT_ONLY" != "1" ] && [ -n "$DAFFY_SYSTEMDDIR" ]; then
    ensure_dir "$DAFFY_SYSTEMDDIR"
    install_service "daffydmd.service"
    install_service "daffybackend.service"
    install_service "daffysignaling.service"

    if command -v systemctl >/dev/null 2>&1; then
        echo "Reloading systemd daemon..."
        systemctl daemon-reload 2>/dev/null \
            || echo "Note: run 'sudo systemctl daemon-reload' to activate the new services"
    fi
fi

# ---------------------------------------------------------------------------
# 4. Packed shared libraries (LINKING=PACK mode)
# ---------------------------------------------------------------------------
PACKED_LIBS_DIR="$DAFFY_DATADIR/lib"
if [ -d "$PACKED_LIBS_DIR" ] && [ "$(ls -A "$PACKED_LIBS_DIR" 2>/dev/null)" ]; then
    SYSTEM_LIBDIR="/usr/local/lib"
    echo "Installing packed shared libraries from $PACKED_LIBS_DIR → $SYSTEM_LIBDIR"
    ensure_dir "$SYSTEM_LIBDIR"
    find "$PACKED_LIBS_DIR" -name "*.so*" | while read -r lib; do
        dest="$SYSTEM_LIBDIR/$(basename "$lib")"
        if [ ! -e "$dest" ]; then
            echo "  install $(basename "$lib")"
            cp "$lib" "$dest"
        fi
    done
    if command -v ldconfig >/dev/null 2>&1; then
        echo "Running ldconfig..."
        ldconfig "$SYSTEM_LIBDIR" 2>/dev/null || ldconfig 2>/dev/null || true
    fi
fi

# ---------------------------------------------------------------------------
# 5. Frontend (gated on PACK_FRONTEND)
# ---------------------------------------------------------------------------
if [ "$PACK_FRONTEND" = "1" ]; then
    FRONTEND_SCRIPT="$SCRIPT_DIR/install-frontend.sh"
    if [ -x "$FRONTEND_SCRIPT" ]; then
        echo "Installing frontend assets..."
        sh "$FRONTEND_SCRIPT"
    else
        echo "Warning: install-frontend.sh not found or not executable"
    fi
fi

# ---------------------------------------------------------------------------
# 6. Standard extensions (gated on INSTALL_STDEXT)
# ---------------------------------------------------------------------------
if [ "$INSTALL_STDEXT" = "1" ]; then
    STDEXT_SCRIPT="$SCRIPT_DIR/stdext-helper.sh"
    if [ -x "$STDEXT_SCRIPT" ]; then
        echo "Installing standard extensions..."
        sh "$STDEXT_SCRIPT"
    else
        echo "Warning: stdext-helper.sh not found or not executable"
    fi
fi

# ---------------------------------------------------------------------------
# 7. Summary
# ---------------------------------------------------------------------------
echo ""
echo "Installation complete!"
echo ""
if [ "$CLIENT_ONLY" = "1" ]; then
    echo "  daffyscript is ready – run 'daffyscript --help' to get started."
else
    echo "  Binaries    : ${PREFIX}/bin/{daffy-backend,daffy-signaling,daffydmd}"
    echo "  Config      : ${DAFFY_CONFIGDIR}/${DAFFY_CONFIGFILE}"
    echo "  Services    : daffydmd  daffybackend  daffysignaling"
    echo ""
    echo "  Quick start:"
    echo "    sudo systemctl enable --now daffydmd daffybackend daffysignaling"
fi
echo ""
echo "  Docs : $DAFFY_DOCDIR"
echo "  Repo : https://github.com/Chubek/DaffyChat"
