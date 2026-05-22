#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PACKAGE_SCRIPT="$SCRIPT_DIR/package.sh"

usage() {
  cat <<'EOF'
Usage: bash build.sh [package.sh options] [cache options]

Cache options:
  --from-cache ID      Restore build state from __build_cache_ID before building
  --to-cache ID        Save build state to __build_cache_ID after a successful build
  --list-caches        List available __build_cache_* directories
  --clear-cache ID     Remove __build_cache_ID

Examples:
  bash build.sh --deb --static --with-frontend --to-cache Foobar
  bash build.sh --deb --static --with-frontend --from-cache Foobar
  bash build.sh --deb --static --with-frontend --from-cache Foobar --to-cache Foobar
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

require_tool() {
  command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

restore_cache() {
  local cache_id="$1"
  local cache_dir="$SCRIPT_DIR/__build_cache_${cache_id}"
  [[ -d "$cache_dir" ]] || die "cache not found: $cache_dir"
  [[ -d "$cache_dir/build" ]] || die "cache is invalid (missing build/): $cache_dir"

  printf 'Restoring cache: %s\n' "$cache_dir"
  rm -rf "$SCRIPT_DIR/build/package"
  mkdir -p "$SCRIPT_DIR/build"
  cp -a "$cache_dir/build" "$SCRIPT_DIR/build/package"

  if [[ -d "$cache_dir/dist" ]]; then
    rm -rf "$SCRIPT_DIR/dist"
    cp -a "$cache_dir/dist" "$SCRIPT_DIR/dist"
  fi
}

save_cache() {
  local cache_id="$1"
  local cache_dir="$SCRIPT_DIR/__build_cache_${cache_id}"
  local tmp_dir="${cache_dir}.tmp.$$"
  [[ -d "$SCRIPT_DIR/build/package" ]] || die "build output missing: $SCRIPT_DIR/build/package"

  printf 'Saving cache: %s\n' "$cache_dir"
  rm -rf "$tmp_dir"
  mkdir -p "$tmp_dir"
  cp -a "$SCRIPT_DIR/build/package" "$tmp_dir/build"
  if [[ -d "$SCRIPT_DIR/dist" ]]; then
    cp -a "$SCRIPT_DIR/dist" "$tmp_dir/dist"
  fi
  rm -rf "$cache_dir"
  mv "$tmp_dir" "$cache_dir"
}

FROM_CACHE=""
TO_CACHE=""
LIST_CACHES="OFF"
CLEAR_CACHE=""
declare -a FORWARD_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from-cache)
      [[ $# -ge 2 ]] || die "--from-cache requires an ID"
      FROM_CACHE="$2"
      shift
      ;;
    --to-cache)
      [[ $# -ge 2 ]] || die "--to-cache requires an ID"
      TO_CACHE="$2"
      shift
      ;;
    --list-caches)
      LIST_CACHES="ON"
      ;;
    --clear-cache)
      [[ $# -ge 2 ]] || die "--clear-cache requires an ID"
      CLEAR_CACHE="$2"
      shift
      ;;
    -h|--help)
      usage
      [[ -x "$PACKAGE_SCRIPT" ]] && printf '\n'
      [[ -x "$PACKAGE_SCRIPT" ]] && "$PACKAGE_SCRIPT" --help || true
      exit 0
      ;;
    *)
      FORWARD_ARGS+=("$1")
      ;;
  esac
  shift
done

require_tool bash
[[ -f "$PACKAGE_SCRIPT" ]] || die "missing package script: $PACKAGE_SCRIPT"

if [[ "$LIST_CACHES" == "ON" ]]; then
  find "$SCRIPT_DIR" -maxdepth 1 -type d -name '__build_cache_*' -printf '%f\n' | sort || true
  exit 0
fi

if [[ -n "$CLEAR_CACHE" ]]; then
  rm -rf "$SCRIPT_DIR/__build_cache_${CLEAR_CACHE}"
  printf 'Cleared cache: __build_cache_%s\n' "$CLEAR_CACHE"
  exit 0
fi

if [[ -n "$FROM_CACHE" ]]; then
  restore_cache "$FROM_CACHE"
fi

bash "$PACKAGE_SCRIPT" "${FORWARD_ARGS[@]}"

if [[ -n "$TO_CACHE" ]]; then
  save_cache "$TO_CACHE"
fi
