#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

usage() {
  cat <<'EOF'
Usage: ./package.sh [scope] [format...] [linking] [options]

Scope:
  --server                Build a server package (default)
  --client                Build a client-only package

Package formats:
  --deb                   Build a DEB package
  --rpm                   Build an RPM package
  --pacman                Build a Pacman package
  --tarball               Build a tarball package
  --gzip                  Build a gzip-compressed archive
  --bzip2                 Build a bzip2-compressed archive
  --zlib                  Build a zlib-compressed archive

Linking:
  --dynamic               Use dynamic linking (default)
  --static                Use static linking
  --pack                  Bundle shared libraries in the package

Packaging options:
  --with-frontend         Include frontend assets
  --no-frontend           Exclude frontend assets
  --with-services         Install default services
  --no-services           Skip default services
  --with-docs             Build and install documentation
  --no-docs               Disable documentation packaging
  --with-plugins          Install example plugins
  --no-plugins            Skip example plugins
  --with-standard-extensions  Include and install stdext standard extensions
  --no-standard-extensions    Exclude stdext standard extensions (default)
  --with-toolchain        Install toolchain helpers
  --no-toolchain          Skip toolchain helpers
  --with-coturn           Include and install vendored coturn
  --no-coturn             Skip vendored coturn (default)
  --with-manpages         Install manpages
  --no-manpages           Skip manpages
  --config-json           Install JSON config sample (default)
  --config-conf           Install CONF config sample
  --with-tests            Build tests (default)
  --no-tests              Disable test targets
  --werror                Treat warnings as errors
  --sanitize TYPE         Enable sanitizer: address|undefined|thread|leak
  --prefix PATH           Set installation prefix for packaging
  --build-dir PATH        Build directory to use (default: build/package)
  --output-dir PATH       Final artifact directory (default: dist)
  --version VERSION       Override package version
  --release N             Set package release number (default: 1)
  --jobs N                Parallel build jobs
  --clean                 Remove the build directory before configuring
  --dry-run               Print commands without running them
  --verbose               Use verbose CMake builds
  -h, --help              Show this help

Examples:
  ./package.sh --server --deb --static
  ./package.sh --client --dynamic --tarball
  ./package.sh --server --with-frontend --dynamic --rpm
  ./package.sh --server --deb --static --with-services --with-frontend
  ./package.sh --server --deb --rpm --pack --with-docs --jobs 8
EOF
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

require_tool() {
  command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

append_path_var() {
  local var_name="$1"
  local value="$2"
  local current="${!var_name:-}"
  if [[ -n "$current" ]]; then
    printf -v "$var_name" '%s:%s' "$value" "$current"
  else
    printf -v "$var_name" '%s' "$value"
  fi
}

quote_args() {
  local arg
  for arg in "$@"; do
    printf '%q ' "$arg"
  done
  printf '\n'
}

run_cmd() {
  if [[ "$DRY_RUN" == "ON" ]]; then
    printf '[dry-run] '
    quote_args "$@"
  else
    "$@"
  fi
}

bool_word() {
  if [[ "$1" == "ON" ]]; then
    printf 'yes'
  else
    printf 'no'
  fi
}

extract_project_version() {
  local line version
  line="$(grep -E '^[[:space:]]*project\(DaffyChat[[:space:]]+VERSION[[:space:]]+' "$SCRIPT_DIR/CMakeLists.txt" | head -n1 || true)"
  version="${line#*VERSION }"
  version="${version%% *}"
  [[ -n "$version" ]] || die "could not determine default version from CMakeLists.txt"
  printf '%s\n' "$version"
}

SCOPE="server"
LINKING="DYNAMIC"
PACK_FRONTEND="ON"
INSTALL_SERVICES="ON"
BUILD_DOCS="OFF"
INSTALL_DOCS="OFF"
INSTALL_PLUGINS="ON"
INSTALL_STDEXT="OFF"
INSTALL_TOOLCHAIN="ON"
INSTALL_COTURN="OFF"
INSTALL_MAN="ON"
ENABLE_TESTS="ON"
ENABLE_WERROR="OFF"
SANITIZER=""
CONFIG_FORMAT="JSON"
BUILD_DIR="$SCRIPT_DIR/build/package"
OUTPUT_DIR="$SCRIPT_DIR/dist"
PREFIX=""
VERSION=""
RELEASE="1"
JOBS=""
CLEAN="OFF"
DRY_RUN="OFF"
VERBOSE="OFF"
EXTERNLIB_SCRIPT="$SCRIPT_DIR/scripts/externlib_service.sh"
EXTERNLIB_BUILD_ROOT=""
EXTERNLIB_INSTALL_PREFIX=""

declare -a FORMATS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --server)
      SCOPE="server"
      ;;
    --client)
      SCOPE="client"
      ;;
    --deb|--rpm|--pacman|--tarball|--gzip|--bzip2|--zlib)
      FORMATS+=("${1#--}")
      ;;
    --dynamic)
      LINKING="DYNAMIC"
      ;;
    --static)
      LINKING="STATIC"
      ;;
    --pack)
      LINKING="PACK"
      ;;
    --with-frontend)
      PACK_FRONTEND="ON"
      ;;
    --no-frontend)
      PACK_FRONTEND="OFF"
      ;;
    --with-services)
      INSTALL_SERVICES="ON"
      ;;
    --no-services)
      INSTALL_SERVICES="OFF"
      ;;
    --with-docs)
      BUILD_DOCS="ON"
      INSTALL_DOCS="ON"
      ;;
    --no-docs)
      BUILD_DOCS="OFF"
      INSTALL_DOCS="OFF"
      ;;
    --with-plugins)
      INSTALL_PLUGINS="ON"
      ;;
    --no-plugins)
      INSTALL_PLUGINS="OFF"
      ;;
    --with-standard-extensions)
      INSTALL_STDEXT="ON"
      ;;
    --no-standard-extensions)
      INSTALL_STDEXT="OFF"
      ;;
    --with-toolchain)
      INSTALL_TOOLCHAIN="ON"
      ;;
    --no-toolchain)
      INSTALL_TOOLCHAIN="OFF"
      ;;
    --with-manpages)
      INSTALL_MAN="ON"
      ;;
    --no-manpages)
      INSTALL_MAN="OFF"
      ;;
    --with-coturn)
      INSTALL_COTURN="ON"
      ;;
    --no-coturn)
      INSTALL_COTURN="OFF"
      ;;
    --config-json)
      CONFIG_FORMAT="JSON"
      ;;
    --config-conf)
      CONFIG_FORMAT="CONF"
      ;;
    --with-tests)
      ENABLE_TESTS="ON"
      ;;
    --no-tests)
      ENABLE_TESTS="OFF"
      ;;
    --werror)
      ENABLE_WERROR="ON"
      ;;
    --sanitize)
      [[ $# -ge 2 ]] || die "--sanitize requires a value"
      SANITIZER="$2"
      shift
      ;;
    --prefix)
      [[ $# -ge 2 ]] || die "--prefix requires a path"
      PREFIX="$2"
      shift
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || die "--build-dir requires a path"
      BUILD_DIR="$2"
      shift
      ;;
    --output-dir)
      [[ $# -ge 2 ]] || die "--output-dir requires a path"
      OUTPUT_DIR="$2"
      shift
      ;;
    --version)
      [[ $# -ge 2 ]] || die "--version requires a value"
      VERSION="$2"
      shift
      ;;
    --release)
      [[ $# -ge 2 ]] || die "--release requires a value"
      RELEASE="$2"
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
    --verbose)
      VERBOSE="ON"
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

[[ ${#FORMATS[@]} -gt 0 ]] || die "at least one package format is required"

if [[ -n "$SANITIZER" ]]; then
  case "$SANITIZER" in
    address|undefined|thread|leak)
      ;;
    *)
      die "unsupported sanitizer: $SANITIZER"
      ;;
  esac
fi

if [[ -n "$JOBS" && ! "$JOBS" =~ ^[0-9]+$ ]]; then
  die "--jobs expects a positive integer"
fi

if [[ ! "$RELEASE" =~ ^[0-9A-Za-z._+-]+$ ]]; then
  die "--release contains unsupported characters: $RELEASE"
fi

VERSION="${VERSION:-$(extract_project_version)}"
BUILD_DIR="${BUILD_DIR/#\~/$HOME}"
OUTPUT_DIR="${OUTPUT_DIR/#\~/$HOME}"
if [[ -n "$PREFIX" ]]; then
  PREFIX="${PREFIX/#\~/$HOME}"
fi

EXTERNLIB_BUILD_ROOT="$BUILD_DIR/externlib"
EXTERNLIB_INSTALL_PREFIX="$EXTERNLIB_BUILD_ROOT/prefix"

case "$SCOPE" in
  client)
    CLIENT_ONLY="ON"
    INSTALL_SERVICES="OFF"
    PACK_FRONTEND="${PACK_FRONTEND:-OFF}"
    ;;
  server)
    CLIENT_ONLY="OFF"
    ;;
esac

declare -A TARGET_FLAGS=(
  [deb]=BUILD_DEB
  [rpm]=BUILD_RPM
  [pacman]=BUILD_PACMAN
  [tarball]=BUILD_TARBALL
  [gzip]=BUILD_GZIP
  [bzip2]=BUILD_BZIP2
  [zlib]=BUILD_ZLIB
)

declare -A TARGET_NAMES=(
  [deb]=deb
  [rpm]=rpm
  [pacman]=pacman
  [tarball]=tarball
  [gzip]=gzip
  [bzip2]=bzip2
  [zlib]=zlib
)

declare -A FORMAT_LABELS=(
  [deb]="DEB"
  [rpm]="RPM"
  [pacman]="Pacman"
  [tarball]="tarball"
  [gzip]="gzip archive"
  [bzip2]="bzip2 archive"
  [zlib]="zlib archive"
)

require_tool cmake

run_external_libs() {
  [[ "$SCOPE" == "server" ]] || return 0

  case "$LINKING" in
    STATIC|PACK)
      [[ -x "$EXTERNLIB_SCRIPT" ]] || die "missing helper script: $EXTERNLIB_SCRIPT"
      printf 'Preparing external libraries...\n'
      local helper_cmd=(
        "$EXTERNLIB_SCRIPT"
        --lib all
        --mode "$(printf '%s' "$LINKING" | tr '[:upper:]' '[:lower:]')"
        --build-root "$EXTERNLIB_BUILD_ROOT"
        --install-prefix "$EXTERNLIB_INSTALL_PREFIX"
      )
      if [[ -n "$JOBS" ]]; then
        helper_cmd+=(--jobs "$JOBS")
      fi
      if [[ "$CLEAN" == "ON" ]]; then
        helper_cmd+=(--clean)
      fi
      if [[ "$DRY_RUN" == "ON" ]]; then
        helper_cmd+=(--dry-run)
      fi
      run_cmd "${helper_cmd[@]}"

      append_path_var CMAKE_PREFIX_PATH "$EXTERNLIB_INSTALL_PREFIX"
      append_path_var CMAKE_LIBRARY_PATH "$EXTERNLIB_INSTALL_PREFIX/lib"
      append_path_var CMAKE_LIBRARY_PATH "$EXTERNLIB_INSTALL_PREFIX/lib64"
      append_path_var CMAKE_INCLUDE_PATH "$EXTERNLIB_INSTALL_PREFIX/include"
      export CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH CMAKE_INCLUDE_PATH
      ;;
  esac
}

if [[ "$DRY_RUN" == "OFF" ]]; then
  mkdir -p "$OUTPUT_DIR"
fi

if [[ "$CLEAN" == "ON" && -d "$BUILD_DIR" ]]; then
  if [[ "$DRY_RUN" == "ON" ]]; then
    printf '[dry-run] rm -rf %q\n' "$BUILD_DIR"
  else
    rm -rf "$BUILD_DIR"
  fi
fi

mkdir_args=("$BUILD_DIR" "$OUTPUT_DIR")
if [[ "$DRY_RUN" == "ON" ]]; then
  printf '[dry-run] mkdir -p '
  quote_args "${mkdir_args[@]}"
else
  mkdir -p "${mkdir_args[@]}"
fi

declare -a cmake_args=(
  -S "$SCRIPT_DIR"
  -B "$BUILD_DIR"
  -DLINKING="$LINKING"
  -DPACK_FRONTEND="$PACK_FRONTEND"
  -DDAFFY_CLIENT_SIDE_ONLY="$CLIENT_ONLY"
  -DDAFFY_INSTALL_DEFAULT_SERVICES="$INSTALL_SERVICES"
  -DDAFFY_BUILD_DOCS="$BUILD_DOCS"
  -DDAFFY_INSTALL_DOCS="$INSTALL_DOCS"
  -DDAFFY_CONFIG_FORMAT="$CONFIG_FORMAT"
  -DDAFFY_INSTALL_EXAMPLE_PLUGINS="$INSTALL_PLUGINS"
  -DDAFFY_INSTALL_TOOLCHAIN="$INSTALL_TOOLCHAIN"
  -DDAFFY_INSTALL_STDEXT="$INSTALL_STDEXT"
  -DDAFFY_INSTALL_COTURN="$INSTALL_COTURN"
  -DDAFFY_ENABLE_WERROR="$ENABLE_WERROR"
  -DDAFFY_ENABLE_TESTS="$ENABLE_TESTS"
  -DNO_INSTALL_MAN="$([[ "$INSTALL_MAN" == "ON" ]] && printf 'OFF' || printf 'ON')"
)

if [[ -n "$SANITIZER" ]]; then
  cmake_args+=(-DDAFFY_SANITIZER="$SANITIZER")
fi

if [[ -n "$PREFIX" ]]; then
  cmake_args+=(-DCMAKE_INSTALL_PREFIX="$PREFIX")
fi

for format in "${FORMATS[@]}"; do
  cmake_args+=("-D${TARGET_FLAGS[$format]}=ON")
done

printf 'Packaging DaffyChat\n'
printf '  scope:          %s\n' "$SCOPE"
printf '  formats:        %s\n' "${FORMATS[*]}"
printf '  linking:        %s\n' "$LINKING"
printf '  frontend:       %s\n' "$(bool_word "$PACK_FRONTEND")"
printf '  services:       %s\n' "$(bool_word "$INSTALL_SERVICES")"
printf '  docs:           %s\n' "$(bool_word "$INSTALL_DOCS")"
  printf '  plugins:        %s\n' "$(bool_word "$INSTALL_PLUGINS")"
  printf '  stdext:         %s\n' "$(bool_word "$INSTALL_STDEXT")"
  printf '  coturn:         %s\n' "$(bool_word "$INSTALL_COTURN")"
  printf '  toolchain:      %s\n' "$(bool_word "$INSTALL_TOOLCHAIN")"
printf '  manpages:       %s\n' "$(bool_word "$INSTALL_MAN")"
printf '  config format:  %s\n' "$CONFIG_FORMAT"
printf '  tests:          %s\n' "$(bool_word "$ENABLE_TESTS")"
printf '  werror:         %s\n' "$(bool_word "$ENABLE_WERROR")"
printf '  sanitizer:      %s\n' "${SANITIZER:-none}"
printf '  version:        %s\n' "$VERSION"
printf '  release:        %s\n' "$RELEASE"
printf '  build dir:      %s\n' "$BUILD_DIR"
printf '  output dir:     %s\n' "$OUTPUT_DIR"
if [[ "$LINKING" == "STATIC" || "$LINKING" == "PACK" ]]; then
  printf '  externlib dir:  %s\n' "$EXTERNLIB_BUILD_ROOT"
fi
if [[ -n "$PREFIX" ]]; then
  printf '  install prefix: %s\n' "$PREFIX"
fi
printf '\n'

run_external_libs
run_cmd cmake "${cmake_args[@]}"

declare -a package_paths=()
declare -a build_base=(cmake --build "$BUILD_DIR")
if [[ -n "$JOBS" ]]; then
  build_base+=(--parallel "$JOBS")
fi
if [[ "$VERBOSE" == "ON" ]]; then
  build_base+=(--verbose)
fi

for format in "${FORMATS[@]}"; do
  printf 'Building %s...\n' "${FORMAT_LABELS[$format]}"
  run_cmd "${build_base[@]}" --target "${TARGET_NAMES[$format]}"

  if [[ "$DRY_RUN" == "OFF" ]]; then
    while IFS= read -r path; do
      package_paths+=("$path")
    done < <(find "$BUILD_DIR/packages" -maxdepth 1 -type f -newermt "@0" \
      \( -name '*.deb' -o -name '*.rpm' -o -name '*.pkg.tar*' -o -name '*.tar' -o -name '*.tgz' -o -name '*.gz' -o -name '*.bz2' -o -name '*.zlib' \) | sort)
  fi
done

if [[ "$DRY_RUN" == "ON" ]]; then
  printf '\nDry run complete.\n'
  exit 0
fi

shopt -s nullglob
declare -A seen_paths=()
for artifact in "$BUILD_DIR"/packages/*; do
  [[ -f "$artifact" ]] || continue
  seen_paths["$artifact"]=1
done
shopt -u nullglob

for artifact in "${!seen_paths[@]}"; do
  dest="$OUTPUT_DIR/$(basename "$artifact")"
  cp -f "$artifact" "$dest"
done

if [[ ${#seen_paths[@]} -eq 0 ]]; then
  die "no package artifacts were produced under $BUILD_DIR/packages"
fi

printf '\nArtifacts:\n'
for artifact in "$OUTPUT_DIR"/*; do
  [[ -f "$artifact" ]] || continue
  base="$(basename "$artifact")"
  if [[ -n "${seen_paths[$BUILD_DIR/packages/$base]:-}" ]]; then
    printf '  %s\n' "$artifact"
  fi
done
