#!/bin/bash
# linking-service.sh – helper for DaffyChat's LINKING build option
#
# Usage:
#   linking-service.sh STATIC  <build_dir> <source_dir>
#   linking-service.sh PACK    <build_dir> <source_dir> <stage_dir>
#
# STATIC mode:
#   Verifies that the built executables have no dangling libnng.so dependency.
#   If ldd finds libnng in the ELF, the build used dynamic linkage – exits 1.
#
# PACK mode:
#   Collects all vendored shared libraries from <build_dir> and copies them
#   into <stage_dir>/usr/share/daffychat/lib so they are included in the
#   package archive.  Also writes <stage_dir>/etc/ld.so.conf.d/daffychat.conf
#   and <stage_dir>/DEBIAN/postinst (or appends to it) so that ldconfig is
#   run after installation.

set -e

MODE="${1:-}"
BUILD_DIR="${2:-}"
SOURCE_DIR="${3:-}"
STAGE_DIR="${4:-}"

usage() {
    echo "Usage:"
    echo "  $0 STATIC <build_dir> <source_dir>"
    echo "  $0 PACK   <build_dir> <source_dir> <stage_dir>"
    exit 1
}

require_arg() {
    if [ -z "$1" ]; then
        echo "Error: missing argument: $2"
        usage
    fi
}

# ---------------------------------------------------------------------------
case "$MODE" in
STATIC)
    require_arg "$BUILD_DIR" "build_dir"
    require_arg "$SOURCE_DIR" "source_dir"

    echo "[linking-service] MODE=STATIC – verifying no dynamic nng dependency"

    TARGETS=(daffy-backend daffy-signaling daffydmd)
    FAILED=0
    for tgt in "${TARGETS[@]}"; do
        bin="$BUILD_DIR/$tgt"
        [ -f "$bin" ] || { echo "  skip (not built): $bin"; continue; }

        if command -v ldd >/dev/null 2>&1; then
            if ldd "$bin" 2>/dev/null | grep -q "libnng"; then
                echo "  FAIL: $tgt still links libnng dynamically"
                FAILED=1
            else
                echo "  OK  : $tgt – no libnng.so dependency"
            fi
        elif command -v objdump >/dev/null 2>&1; then
            if objdump -p "$bin" 2>/dev/null | grep -q "libnng"; then
                echo "  FAIL: $tgt still links libnng dynamically"
                FAILED=1
            else
                echo "  OK  : $tgt – no libnng.so dependency (objdump)"
            fi
        else
            echo "  SKIP: no ldd/objdump available – cannot verify $tgt"
        fi
    done

    if [ "$FAILED" -ne 0 ]; then
        echo ""
        echo "Static link verification failed."
        echo "Re-run cmake with -DLINKING=STATIC and rebuild all targets."
        exit 1
    fi
    echo "[linking-service] Static verification passed."
    ;;

PACK)
    require_arg "$BUILD_DIR" "build_dir"
    require_arg "$SOURCE_DIR" "source_dir"
    require_arg "$STAGE_DIR"  "stage_dir"

    echo "[linking-service] MODE=PACK – collecting vendored shared libraries"

    PACK_LIB_DIR="$STAGE_DIR/usr/share/daffychat/lib"
    mkdir -p "$PACK_LIB_DIR"

    # -----------------------------------------------------------------------
    # Find .so files produced under the build directory that originate from
    # vendored third_party sources.
    # We look for nng specifically; extend this list as more vendored
    # shared libraries are added.
    # -----------------------------------------------------------------------
    VENDORED_PATTERNS=(
        "libnng.so*"
        "libnng-*.so*"
    )

    FOUND=0
    for pat in "${VENDORED_PATTERNS[@]}"; do
        while IFS= read -r -d '' lib; do
            dest="$PACK_LIB_DIR/$(basename "$lib")"
            if [ ! -e "$dest" ]; then
                echo "  pack: $(basename "$lib") ($(du -sh "$lib" | cut -f1))"
                cp "$lib" "$dest"
                FOUND=$((FOUND+1))
            fi
        done < <(find "$BUILD_DIR/third_party" -name "$pat" -print0 2>/dev/null)
    done

    # Also collect any .so the executables depend on that live in BUILD_DIR
    for tgt in daffy-backend daffy-signaling daffydmd; do
        bin="$BUILD_DIR/$tgt"
        [ -f "$bin" ] || continue
        if command -v ldd >/dev/null 2>&1; then
            ldd "$bin" 2>/dev/null \
                | awk '/=>/ { print $3 }' \
                | grep -E "^$BUILD_DIR" \
            | while read -r lib; do
                dest="$PACK_LIB_DIR/$(basename "$lib")"
                if [ -f "$lib" ] && [ ! -e "$dest" ]; then
                    echo "  pack (ldd): $(basename "$lib")"
                    cp "$lib" "$dest"
                    FOUND=$((FOUND+1))
                fi
            done
        fi
    done

    if [ "$FOUND" -eq 0 ]; then
        echo "  (no vendored shared libraries found to pack)"
    fi

    # -----------------------------------------------------------------------
    # Write ld.so.conf.d snippet
    # -----------------------------------------------------------------------
    LDCONF_DIR="$STAGE_DIR/etc/ld.so.conf.d"
    mkdir -p "$LDCONF_DIR"
    echo "/usr/share/daffychat/lib" > "$LDCONF_DIR/daffychat.conf"
    echo "  wrote: /etc/ld.so.conf.d/daffychat.conf"

    # -----------------------------------------------------------------------
    # Append ldconfig call to the DEB postinst if it exists
    # -----------------------------------------------------------------------
    POSTINST="$STAGE_DIR/DEBIAN/postinst"
    if [ -f "$POSTINST" ]; then
        if ! grep -q "ldconfig" "$POSTINST"; then
            echo "" >> "$POSTINST"
            echo "# Refresh shared-library cache for packed libs" >> "$POSTINST"
            echo "ldconfig 2>/dev/null || true" >> "$POSTINST"
        fi
    fi

    echo "[linking-service] PACK collection complete. Libraries in: $PACK_LIB_DIR"
    ;;

*)
    echo "Error: unknown mode '$MODE'"
    usage
    ;;
esac
