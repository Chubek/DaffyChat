#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build/cmake}"
prefix="${2:-/opt/daffychat}"
config_dir="${DAFFY_CONFIG_DIR:-/etc/daffychat}"
systemd_dir="${DAFFY_SYSTEMD_DIR:-/etc/systemd/system}"
service_name="daffydmd.service"

if [[ ! -d "$build_dir" ]]; then
  echo "error: build directory not found: $build_dir" >&2
  exit 1
fi

echo "Installing DaffyChat from $build_dir into $prefix"
cmake --install "$build_dir" --prefix "$prefix"

mkdir -p "$config_dir"
if [[ ! -f "$config_dir/daffychat.json" ]]; then
  install -m 0644 config/daffychat.example.json "$config_dir/daffychat.json"
  echo "Installed sample config to $config_dir/daffychat.json"
else
  echo "Keeping existing config at $config_dir/daffychat.json"
fi

if [[ -d "$systemd_dir" ]]; then
  install -m 0644 scripts/$service_name "$systemd_dir/$service_name"
  echo "Installed systemd unit to $systemd_dir/$service_name"
  echo "Next steps:"
  echo "  systemctl daemon-reload"
  echo "  systemctl enable --now $service_name"
else
  echo "Skipped systemd install because $systemd_dir does not exist"
fi
