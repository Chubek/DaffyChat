#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"

usage() {
  cat <<'EOF'
Usage: scripts/externlib_service.sh [options]

Build vendored external libraries with their own native build systems.

Options:
  --lib NAME             Build one library: nng, usrsctp, libdatachannel, spdlog, all
  --mode MODE            Build mode: dynamic, static, pack (default: static)
  --build-root PATH      Root build directory (default: build/externlib)
  --install-prefix PATH  Install prefix for libraries that support install
  --jobs N               Parallel jobs for cmake/make
  --clean                Remove the per-library build directory first
  --dry-run              Print commands without running them
  --list                 Show supported libraries
  -h, --help             Show this help

Examples:
  scripts/externlib_service.sh --lib nng --mode static
  scripts/externlib_service.sh --lib libdatachannel --mode dynamic --jobs 8
  scripts/externlib_service.sh --lib all --mode pack --install-prefix /usr/local
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

build_with_cmake() {
  local name="$1"
  local source_dir="$2"
  shift 2
  local build_dir="$BUILD_ROOT/$name"

  if [[ "$CLEAN" == "ON" && -d "$build_dir" ]]; then
    run_cmd rm -rf "$build_dir"
  fi

  run_cmd mkdir -p "$build_dir"
  run_cmd cmake -S "$source_dir" -B "$build_dir" "$@"

  local build_cmd=(cmake --build "$build_dir")
  if [[ -n "$JOBS" ]]; then
    build_cmd+=(--parallel "$JOBS")
  fi
  run_cmd "${build_cmd[@]}"
}

build_nng() {
  require_tool cmake
  local source_dir="$REPO_ROOT/third_party/nng"
  [[ -d "$source_dir" ]] || die "missing vendored source: $source_dir"

  local shared="OFF"
  if [[ "$MODE" != "static" ]]; then
    shared="ON"
  fi

  build_with_cmake nng "$source_dir" \
    -DBUILD_SHARED_LIBS="$shared" \
    -DNNG_ENABLE_TLS=OFF \
    -DNNG_ENABLE_HTTP=OFF \
    -DNNG_ENABLE_NNGCAT=OFF \
    -DNNG_TESTS=OFF \
    -DNNG_TOOLS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
}

build_spdlog() {
  require_tool cmake
  local source_dir="$REPO_ROOT/third_party/spdlog"
  [[ -d "$source_dir" ]] || die "missing vendored source: $source_dir"

  local shared="OFF"
  if [[ "$MODE" != "static" ]]; then
    shared="ON"
  fi

  build_with_cmake spdlog "$source_dir" \
    -DSPDLOG_BUILD_SHARED="$shared" \
    -DSPDLOG_BUILD_EXAMPLE=OFF \
    -DSPDLOG_BUILD_TESTS=OFF \
    -DSPDLOG_BUILD_BENCH=OFF \
    -DSPDLOG_BUILD_WARNINGS=OFF \
    -DSPDLOG_INSTALL=ON \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
}

build_usrsctp() {
  require_tool cmake
  local source_dir="$REPO_ROOT/third_party/usrsctp"
  [[ -d "$source_dir" ]] || die "missing vendored source: $source_dir"

  local shared="OFF"
  if [[ "$MODE" != "static" ]]; then
    shared="ON"
  fi

  build_with_cmake usrsctp "$source_dir" \
    -Dsctp_build_shared_lib="$shared" \
    -Dsctp_build_programs=OFF \
    -Dsctp_inet=OFF \
    -Dsctp_inet6=OFF \
    -Dsctp_werror=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

  local install_cmd=(cmake --install "$BUILD_ROOT/usrsctp")
  run_cmd "${install_cmd[@]}"
}

build_libdatachannel() {
  require_tool cmake
  local source_dir="$REPO_ROOT/third_party/libdatachannel"
  [[ -d "$source_dir" ]] || die "missing vendored source: $source_dir"
  local shared="OFF"
  local build_target="datachannel-static"
  local use_system_juice="OFF"
  if [[ "$MODE" != "static" ]]; then
    shared="ON"
    build_target="datachannel"
  fi
  if [[ ! -f "$REPO_ROOT/third_party/libjuice/CMakeLists.txt" ]]; then
    use_system_juice="ON"
  fi

  build_with_cmake libdatachannel "$source_dir" \
    -DBUILD_SHARED_LIBS="$shared" \
    -DUSE_SYSTEM_USRSCTP=OFF \
    -DUSE_SYSTEM_PLOG=OFF \
    -DUSE_SYSTEM_SRTP=OFF \
    -DUSE_SYSTEM_JUICE="$use_system_juice" \
    -DNO_EXAMPLES=ON \
    -DNO_TESTS=ON \
    -DNO_MEDIA=OFF \
    -DNO_WEBSOCKET=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

  local build_cmd=(cmake --build "$BUILD_ROOT/libdatachannel" --target "$build_target")
  if [[ -n "$JOBS" ]]; then
    build_cmd+=(--parallel "$JOBS")
  fi
  run_cmd "${build_cmd[@]}"
  run_cmd cmake --install "$BUILD_ROOT/libdatachannel"
}

list_libs() {
  printf 'Supported libraries:\n'
  printf '  nng\n'
  printf '  usrsctp\n'
  printf '  libdatachannel\n'
  printf '  spdlog\n'
  printf '  all\n'
}

MODE="static"
BUILD_ROOT="$REPO_ROOT/build/externlib"
INSTALL_PREFIX="$BUILD_ROOT/prefix"
JOBS=""
CLEAN="OFF"
DRY_RUN="OFF"
TARGET_LIB=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --lib)
      [[ $# -ge 2 ]] || die "--lib requires a value"
      TARGET_LIB="$2"
      shift
      ;;
    --mode)
      [[ $# -ge 2 ]] || die "--mode requires a value"
      MODE="$2"
      shift
      ;;
    --build-root)
      [[ $# -ge 2 ]] || die "--build-root requires a path"
      BUILD_ROOT="$2"
      shift
      ;;
    --install-prefix)
      [[ $# -ge 2 ]] || die "--install-prefix requires a path"
      INSTALL_PREFIX="$2"
      shift
      ;;
    --jobs)
      [[ $# -ge 2 ]] || die "--jobs requires a value"
      JOBS="$2"
      shift
      ;;
    --clean)
      CLEAN="ON"
      ;;
    --dry-run)
      DRY_RUN="ON"
      ;;
    --list)
      list_libs
      exit 0
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

[[ -n "$TARGET_LIB" ]] || die "--lib is required"

case "$MODE" in
  static|dynamic|pack) ;;
  *) die "unsupported mode: $MODE" ;;
esac

if [[ -n "$JOBS" && ! "$JOBS" =~ ^[0-9]+$ ]]; then
  die "--jobs expects a positive integer"
fi

BUILD_ROOT="${BUILD_ROOT/#\~/$HOME}"
INSTALL_PREFIX="${INSTALL_PREFIX/#\~/$HOME}"

printf 'External library build\n'
printf '  library:        %s\n' "$TARGET_LIB"
printf '  mode:           %s\n' "$MODE"
printf '  build root:     %s\n' "$BUILD_ROOT"
printf '  install prefix: %s\n' "$INSTALL_PREFIX"
printf '  clean:          %s\n' "$CLEAN"
printf '  dry run:        %s\n' "$DRY_RUN"
printf '\n'

if [[ "$DRY_RUN" == "OFF" ]]; then
  mkdir -p "$BUILD_ROOT" "$INSTALL_PREFIX"
fi

case "$TARGET_LIB" in
  nng)
    build_nng
    ;;
  usrsctp)
    build_usrsctp
    ;;
  libdatachannel)
    build_usrsctp
    build_libdatachannel
    ;;
  spdlog)
    build_spdlog
    ;;
  all)
    build_nng
    build_usrsctp
    build_libdatachannel
    build_spdlog
    ;;
  *)
    die "unsupported library: $TARGET_LIB"
    ;;
esac

printf 'Done.\n'
