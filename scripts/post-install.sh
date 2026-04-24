#!/bin/bash
# DaffyChat post-installation script
# Handles service setup, config deployment, and directory creation

set -e

# Detect installation paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${DAFFY_PREFIX:-/usr/local}"
CONFIG_FORMAT="${DAFFY_CONFIG_FORMAT:-json}"
CLIENT_ONLY="${DAFFY_CLIENT_ONLY:-0}"

# Source OS-specific paths
if [ -x "$SCRIPT_DIR/os-service.pl" ]; then
    eval "$("$SCRIPT_DIR/os-service.pl" --prefix "$PREFIX" --config-format "$CONFIG_FORMAT" ${CLIENT_ONLY:+--client-only})"
else
    echo "Warning: os-service.pl not found, using defaults"
    DAFFY_CONFIGDIR="${PREFIX}/etc/daffychat"
    DAFFY_SYSTEMDDIR="/etc/systemd/system"
    DAFFY_LOGDIR="/var/log/daffychat"
    DAFFY_RUNDIR="/var/run/daffychat"
    DAFFY_STATEDIR="/var/lib/daffychat"
fi

echo "DaffyChat Post-Installation"
echo "============================"
echo "Prefix: $PREFIX"
echo "Config dir: $DAFFY_CONFIGDIR"
echo "Client-only: $CLIENT_ONLY"
echo ""

# Create necessary directories
create_dirs() {
    local dirs=("$@")
    for dir in "${dirs[@]}"; do
        if [ -n "$dir" ] && [ ! -d "$dir" ]; then
            echo "Creating directory: $dir"
            mkdir -p "$dir" || {
                echo "Warning: Failed to create $dir (may need sudo)"
            }
        fi
    done
}

# Server-side directories
if [ "$CLIENT_ONLY" != "1" ]; then
    create_dirs "$DAFFY_CONFIGDIR" "$DAFFY_LOGDIR" "$DAFFY_RUNDIR" "$DAFFY_STATEDIR"
    
    # Set permissions for server directories
    if [ -d "$DAFFY_LOGDIR" ]; then
        chmod 755 "$DAFFY_LOGDIR" 2>/dev/null || true
    fi
    if [ -d "$DAFFY_RUNDIR" ]; then
        chmod 755 "$DAFFY_RUNDIR" 2>/dev/null || true
    fi
    if [ -d "$DAFFY_STATEDIR" ]; then
        chmod 755 "$DAFFY_STATEDIR" 2>/dev/null || true
    fi
fi

# Install config file if it doesn't exist
if [ "$CLIENT_ONLY" != "1" ] && [ -n "$DAFFY_CONFIGDIR" ]; then
    CONFIG_FILE="$DAFFY_CONFIGDIR/$DAFFY_CONFIGFILE"
    if [ ! -f "$CONFIG_FILE" ]; then
        EXAMPLE_CONFIG="$DAFFY_DATADIR/config/daffychat.example.$CONFIG_FORMAT"
        if [ -f "$EXAMPLE_CONFIG" ]; then
            echo "Installing default config: $CONFIG_FILE"
            cp "$EXAMPLE_CONFIG" "$CONFIG_FILE" || {
                echo "Warning: Failed to install config (may need sudo)"
            }
        else
            echo "Warning: Example config not found at $EXAMPLE_CONFIG"
        fi
    else
        echo "Config file already exists: $CONFIG_FILE"
    fi
fi

# Install systemd service
if [ "$CLIENT_ONLY" != "1" ] && [ -n "$DAFFY_SYSTEMDDIR" ] && [ -d "$(dirname "$DAFFY_SYSTEMDDIR")" ]; then
    SERVICE_FILE="$DAFFY_SYSTEMDDIR/daffydmd.service"
    EXAMPLE_SERVICE="$DAFFY_DATADIR/scripts/daffydmd.service"
    
    if [ -f "$EXAMPLE_SERVICE" ]; then
        if [ ! -f "$SERVICE_FILE" ]; then
            echo "Installing systemd service: $SERVICE_FILE"
            cp "$EXAMPLE_SERVICE" "$SERVICE_FILE" || {
                echo "Warning: Failed to install systemd service (may need sudo)"
            }
            
            # Reload systemd if possible
            if command -v systemctl >/dev/null 2>&1; then
                systemctl daemon-reload 2>/dev/null || {
                    echo "Note: Run 'sudo systemctl daemon-reload' to load the service"
                }
            fi
        else
            echo "Systemd service already exists: $SERVICE_FILE"
        fi
    fi
fi

# Print next steps
echo ""
echo "Installation complete!"
echo ""

if [ "$CLIENT_ONLY" = "1" ]; then
    echo "Client-side tools installed:"
    echo "  - daffyscript (Daffyscript interpreter)"
    echo ""
    echo "Run 'daffyscript --help' to get started."
else
    echo "Server-side components installed:"
    echo "  - daffydmd (daemon manager)"
    echo "  - daffy-backend (chat server)"
    echo "  - daffy-signaling (signaling server)"
    echo ""
    echo "Configuration:"
    echo "  - Config file: $DAFFY_CONFIGDIR/$DAFFY_CONFIGFILE"
    echo "  - Edit the config file to customize your installation"
    echo ""
    
    if [ -n "$DAFFY_SYSTEMDDIR" ]; then
        echo "To start DaffyChat as a service:"
        echo "  sudo systemctl enable daffydmd"
        echo "  sudo systemctl start daffydmd"
        echo ""
    fi
    
    echo "To run manually:"
    echo "  daffydmd --config $DAFFY_CONFIGDIR/$DAFFY_CONFIGFILE"
    echo ""
fi

echo "Documentation: $DAFFY_DOCDIR"
echo "For more information, visit: https://github.com/Chubek/DaffyChat"
