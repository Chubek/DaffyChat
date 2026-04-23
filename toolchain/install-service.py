#!/usr/bin/env python3
"""
Install Service - Install a DSSL-generated service daemon

Installs service binaries, systemd units, and configuration.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Install a DSSL-generated service daemon",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Install a service binary
  %(prog)s --binary ./build/daffy-echo-service --name echo
  
  # Install with systemd unit
  %(prog)s --binary ./build/daffy-auth-service --name auth --systemd
        """
    )
    
    parser.add_argument("--binary", "-b", required=True,
                       help="Path to service binary")
    parser.add_argument("--name", "-n", required=True,
                       help="Service name")
    parser.add_argument("--prefix", default="/usr/local",
                       help="Installation prefix (default: /usr/local)")
    parser.add_argument("--systemd", action="store_true",
                       help="Install systemd service unit")
    parser.add_argument("--dry-run", action="store_true",
                       help="Show what would be done without doing it")
    
    args = parser.parse_args()
    
    # Validate binary
    binary_path = Path(args.binary)
    if not binary_path.exists():
        print(f"Error: Binary not found: {args.binary}", file=sys.stderr)
        return 1
    
    if not binary_path.is_file():
        print(f"Error: Not a file: {args.binary}", file=sys.stderr)
        return 1
    
    # Check if running as root for system installation
    if args.prefix.startswith("/usr") and os.geteuid() != 0:
        print("Warning: Installing to system directory requires root privileges", 
              file=sys.stderr)
        print("Consider using sudo or --prefix ~/.local", file=sys.stderr)
    
    # Determine installation paths
    bin_dir = Path(args.prefix) / "bin"
    service_name = f"daffy-{args.name}-service"
    install_path = bin_dir / service_name
    
    # Install binary
    print(f"Installing {binary_path} -> {install_path}")
    if not args.dry_run:
        bin_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(binary_path, install_path)
        install_path.chmod(0o755)
    
    # Install systemd unit if requested
    if args.systemd:
        systemd_dir = Path("/etc/systemd/system")
        unit_name = f"daffychat-{args.name}.service"
        unit_path = systemd_dir / unit_name
        
        unit_content = f"""[Unit]
Description=DaffyChat {args.name} Service
After=network.target daffydmd.service
Requires=daffydmd.service

[Service]
Type=simple
ExecStart={install_path}
Restart=on-failure
RestartSec=5s
User=daffychat
Group=daffychat

[Install]
WantedBy=multi-user.target
"""
        
        print(f"Installing systemd unit -> {unit_path}")
        if not args.dry_run:
            if os.geteuid() != 0:
                print("Error: Installing systemd units requires root", file=sys.stderr)
                return 1
            unit_path.write_text(unit_content)
            subprocess.run(["systemctl", "daemon-reload"], check=True)
            print(f"Systemd unit installed. Enable with: systemctl enable {unit_name}")
    
    print("Installation complete!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
