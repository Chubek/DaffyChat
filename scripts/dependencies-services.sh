#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"

usage() {
  cat <<'EOF'
Usage: scripts/dependencies-services.sh [options]

Actions (pick one):
  --build-socketio-server      Build third_party/Socket.IO.Server.CPP
  --install-socketio-server    Install a compiled socketio-server binary + service

Common options:
  --source-dir PATH            Socket.IO source directory
                               (default: third_party/Socket.IO.Server.CPP)
  --output-binary PATH         Output binary path
                               (default: build/socketio-server/socketio-server)
  --service-file PATH          Systemd service file path
                               (default: scripts/socketio-server.service)
  --jobs N                     Parallel jobs for build
  --clean                      Remove previous output before building
  --dry-run                    Print commands without running
  -h, --help                   Show this help

Install options:
  --prefix PATH                Install prefix for binaries (default: /usr)
  --systemd-dir PATH           Service install directory
                               (default: /etc/systemd/system)
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

require_tool() {
  command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

run_cmd() {
  if [[ "$DRY_RUN" == "ON" ]]; then
    printf '[dry-run]'
    for arg in "$@"; do
      printf ' %q' "$arg"
    done
    printf '\n'
  else
    "$@"
  fi
}

build_socketio_server() {
  require_tool cmake
  [[ -d "$SOURCE_DIR" ]] || die "missing Socket.IO source directory: $SOURCE_DIR"

  local output_dir
  output_dir="$(dirname -- "$OUTPUT_BINARY")"
  if [[ "$CLEAN" == "ON" ]]; then
    run_cmd rm -rf "$output_dir"
  fi
  run_cmd mkdir -p "$output_dir"

  local tmpdir
  if [[ "$DRY_RUN" == "ON" ]]; then
    tmpdir="/tmp/daffy-socketio-build-dry-run"
    printf '[dry-run] mktemp -d %q\n' "/tmp/daffy-socketio-build-XXXXXX"
  else
    tmpdir="$(mktemp -d /tmp/daffy-socketio-build-XXXXXX)"
  fi

  cleanup() {
    if [[ "$DRY_RUN" == "OFF" && -n "${tmpdir:-}" && -d "$tmpdir" ]]; then
      rm -rf "$tmpdir"
    fi
  }
  trap cleanup EXIT

  local cmake_args=(
    -S "$SOURCE_DIR"
    -B "$tmpdir/build"
    -DCMAKE_BUILD_TYPE=Release
  )
  run_cmd cmake "${cmake_args[@]}"

  local build_cmd=(cmake --build "$tmpdir/build" --target socketio-server)
  if [[ -n "$JOBS" ]]; then
    build_cmd+=(--parallel "$JOBS")
  fi
  run_cmd "${build_cmd[@]}"

  local built_binary="$tmpdir/build/socketio-server"
  [[ "$DRY_RUN" == "ON" ]] || [[ -f "$built_binary" ]] || die "Socket.IO build succeeded but output binary was not found: $built_binary"
  run_cmd cp -f "$built_binary" "$OUTPUT_BINARY"
  run_cmd chmod 755 "$OUTPUT_BINARY"
}

install_socketio_server() {
  [[ -f "$OUTPUT_BINARY" ]] || die "missing compiled socketio-server binary: $OUTPUT_BINARY"
  [[ -f "$SERVICE_FILE" ]] || die "missing service file: $SERVICE_FILE"

  local bindir="$PREFIX/lib/daffychat"
  local target_bin="$bindir/socketio-server"
  local target_service="$SYSTEMD_DIR/$(basename -- "$SERVICE_FILE")"

  run_cmd mkdir -p "$bindir"
  run_cmd cp -f "$OUTPUT_BINARY" "$target_bin"
  run_cmd chmod 755 "$target_bin"

  run_cmd mkdir -p "$SYSTEMD_DIR"
  run_cmd cp -f "$SERVICE_FILE" "$target_service"
  run_cmd chmod 644 "$target_service"

  if command -v systemctl >/dev/null 2>&1; then
    run_cmd systemctl daemon-reload
  fi
}

ACTION=""
SOURCE_DIR="$REPO_ROOT/third_party/Socket.IO.Server.CPP"
OUTPUT_BINARY="$REPO_ROOT/build/socketio-server/socketio-server"
SERVICE_FILE="$REPO_ROOT/scripts/socketio-server.service"
PREFIX="/usr"
SYSTEMD_DIR="/etc/systemd/system"
JOBS=""
CLEAN="OFF"
DRY_RUN="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-socketio-server)
      ACTION="build"
      ;;
    --install-socketio-server)
      ACTION="install"
      ;;
    --source-dir)
      [[ $# -ge 2 ]] || die "--source-dir requires a path"
      SOURCE_DIR="$2"
      shift
      ;;
    --output-binary)
      [[ $# -ge 2 ]] || die "--output-binary requires a path"
      OUTPUT_BINARY="$2"
      shift
      ;;
    --service-file)
      [[ $# -ge 2 ]] || die "--service-file requires a path"
      SERVICE_FILE="$2"
      shift
      ;;
    --jobs)
      [[ $# -ge 2 ]] || die "--jobs requires a value"
      JOBS="$2"
      shift
      ;;
    --prefix)
      [[ $# -ge 2 ]] || die "--prefix requires a path"
      PREFIX="$2"
      shift
      ;;
    --systemd-dir)
      [[ $# -ge 2 ]] || die "--systemd-dir requires a path"
      SYSTEMD_DIR="$2"
      shift
      ;;
    --clean)
      CLEAN="ON"
      ;;
    --dry-run)
      DRY_RUN="ON"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown option: $1"
      ;;
  esac
  shift
done

[[ -n "$ACTION" ]] || die "choose one action: --build-socketio-server or --install-socketio-server"

if [[ -n "$JOBS" && ! "$JOBS" =~ ^[0-9]+$ ]]; then
  die "--jobs expects a positive integer"
fi

SOURCE_DIR="${SOURCE_DIR/#\~/$HOME}"
OUTPUT_BINARY="${OUTPUT_BINARY/#\~/$HOME}"
SERVICE_FILE="${SERVICE_FILE/#\~/$HOME}"
PREFIX="${PREFIX/#\~/$HOME}"
SYSTEMD_DIR="${SYSTEMD_DIR/#\~/$HOME}"

case "$ACTION" in
  build)
    build_socketio_server
    ;;
  install)
    install_socketio_server
    ;;
  *)
    die "unsupported action: $ACTION"
    ;;
esac
