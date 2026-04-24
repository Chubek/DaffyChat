#!/bin/sh
# stdext-helper.sh – install DaffyChat standard extensions from the package tree
#
# Called by post-install.sh when DAFFY_INSTALL_STDEXT=1.
# Can also be run standalone after the package is installed.
#
# Extension types handled:
#   dssl/          → DAFFY_DSSL_DIR    (default /usr/share/daffychat/dssl)
#   daffyscript/   → broken into sub-roles:
#     frontend/    → DAFFY_DAFSCRIPT_FRONTEND_DIR  (/usr/share/daffychat/daffyscript/frontend)
#     programs/    → DAFFY_DAFSCRIPT_PROGRAMS_DIR  (/usr/share/daffychat/daffyscript/programs)
#     recipes/     → DAFFY_DAFSCRIPT_RECIPES_DIR   (/usr/share/daffychat/daffyscript/recipes)
#   lua/           → DAFFY_LUA_DIR     (default /usr/share/daffychat/lua)
#   shared/        → DAFFY_SHARED_PLUGIN_DIR (default /usr/share/daffychat/plugins)
#
# Env overrides (all optional):
#   DAFFY_STDEXT_SRC             source tree  (default /usr/share/daffychat/stdext)
#   DAFFY_DSSL_DIR               DSSL service contract destination
#   DAFFY_DAFSCRIPT_FRONTEND_DIR Daffyscript frontend module destination
#   DAFFY_DAFSCRIPT_PROGRAMS_DIR Daffyscript program/bot destination
#   DAFFY_DAFSCRIPT_RECIPES_DIR  Daffyscript recipe destination
#   DAFFY_LUA_DIR                Lua room script destination
#   DAFFY_SHARED_PLUGIN_DIR      Shared-library plugin destination

set -e

STDEXT_SRC="${DAFFY_STDEXT_SRC:-/usr/share/daffychat/stdext}"

DSSL_DIR="${DAFFY_DSSL_DIR:-/usr/share/daffychat/dssl}"
DAFSCRIPT_FRONTEND_DIR="${DAFFY_DAFSCRIPT_FRONTEND_DIR:-/usr/share/daffychat/daffyscript/frontend}"
DAFSCRIPT_PROGRAMS_DIR="${DAFFY_DAFSCRIPT_PROGRAMS_DIR:-/usr/share/daffychat/daffyscript/programs}"
DAFSCRIPT_RECIPES_DIR="${DAFFY_DAFSCRIPT_RECIPES_DIR:-/usr/share/daffychat/daffyscript/recipes}"
LUA_DIR="${DAFFY_LUA_DIR:-/usr/share/daffychat/lua}"
SHARED_PLUGIN_DIR="${DAFFY_SHARED_PLUGIN_DIR:-/usr/share/daffychat/plugins}"

log() {
    printf '[stdext-helper] %s\n' "$*"
}

warn() {
    printf '[stdext-helper] Warning: %s\n' "$*" >&2
}

# Install all items from a source directory into a destination directory.
# Does not overwrite existing files so that local customisations survive upgrades.
install_dir() {
    local src="$1"
    local dst="$2"

    if [ ! -d "$src" ]; then
        warn "source directory not found, skipping: $src"
        return
    fi

    if ! mkdir -p "$dst" 2>/dev/null; then
        warn "could not create destination directory: $dst"
        return
    fi

    find "$src" -mindepth 1 -maxdepth 1 | while IFS= read -r entry; do
        base="$(basename "$entry")"
        dest_entry="$dst/$base"

        if [ -d "$entry" ]; then
            install_dir "$entry" "$dest_entry"
        elif [ -f "$entry" ]; then
            if [ -e "$dest_entry" ]; then
                log "  already present, skipping: $dest_entry"
            else
                cp "$entry" "$dest_entry"
                chmod 644 "$dest_entry"
                log "  installed: $dest_entry"
            fi
        fi
    done
}

# ---------------------------------------------------------------------------
log "Source tree : $STDEXT_SRC"

if [ ! -d "$STDEXT_SRC" ]; then
    warn "stdext source tree not found at $STDEXT_SRC – nothing to install"
    exit 0
fi

# ---------------------------------------------------------------------------
# 1. DSSL service contracts  (stdext/dssl/**)
# ---------------------------------------------------------------------------
DSSL_SRC="$STDEXT_SRC/dssl"
if [ -d "$DSSL_SRC" ]; then
    log "Installing DSSL service contracts -> $DSSL_DIR"
    for contract_dir in "$DSSL_SRC"/*/; do
        [ -d "$contract_dir" ] || continue
        name="$(basename "$contract_dir")"
        install_dir "$contract_dir" "$DSSL_DIR/$name"
    done
else
    log "No dssl/ directory found in source tree, skipping"
fi

# ---------------------------------------------------------------------------
# 2. Daffyscript extensions  (stdext/daffyscript/{frontend,programs,recipes}/**)
# ---------------------------------------------------------------------------
DAFSCRIPT_SRC="$STDEXT_SRC/daffyscript"
if [ -d "$DAFSCRIPT_SRC" ]; then

    # frontend modules (.dfy)
    if [ -d "$DAFSCRIPT_SRC/frontend" ]; then
        log "Installing Daffyscript frontend modules -> $DAFSCRIPT_FRONTEND_DIR"
        for mod_dir in "$DAFSCRIPT_SRC/frontend"/*/; do
            [ -d "$mod_dir" ] || continue
            name="$(basename "$mod_dir")"
            install_dir "$mod_dir" "$DAFSCRIPT_FRONTEND_DIR/$name"
        done
    fi

    # programs / room bots (.dfyp)
    if [ -d "$DAFSCRIPT_SRC/programs" ]; then
        log "Installing Daffyscript programs -> $DAFSCRIPT_PROGRAMS_DIR"
        for prog_dir in "$DAFSCRIPT_SRC/programs"/*/; do
            [ -d "$prog_dir" ] || continue
            name="$(basename "$prog_dir")"
            install_dir "$prog_dir" "$DAFSCRIPT_PROGRAMS_DIR/$name"
        done
    fi

    # recipes (.dfyr)
    if [ -d "$DAFSCRIPT_SRC/recipes" ]; then
        log "Installing Daffyscript recipes -> $DAFSCRIPT_RECIPES_DIR"
        for recipe_dir in "$DAFSCRIPT_SRC/recipes"/*/; do
            [ -d "$recipe_dir" ] || continue
            name="$(basename "$recipe_dir")"
            install_dir "$recipe_dir" "$DAFSCRIPT_RECIPES_DIR/$name"
        done
    fi

else
    log "No daffyscript/ directory found in source tree, skipping"
fi

# ---------------------------------------------------------------------------
# 3. Lua room scripts  (stdext/lua/**)
# ---------------------------------------------------------------------------
LUA_SRC="$STDEXT_SRC/lua"
if [ -d "$LUA_SRC" ]; then
    log "Installing Lua room scripts -> $LUA_DIR"
    for script_dir in "$LUA_SRC"/*/; do
        [ -d "$script_dir" ] || continue
        name="$(basename "$script_dir")"
        install_dir "$script_dir" "$LUA_DIR/$name"
    done
else
    log "No lua/ directory found in source tree, skipping"
fi

# ---------------------------------------------------------------------------
# 4. Shared-library plugins  (stdext/shared/**)
# ---------------------------------------------------------------------------
SHARED_SRC="$STDEXT_SRC/shared"
if [ -d "$SHARED_SRC" ]; then
    log "Installing shared-library plugins -> $SHARED_PLUGIN_DIR"
    for plugin_dir in "$SHARED_SRC"/*/; do
        [ -d "$plugin_dir" ] || continue
        name="$(basename "$plugin_dir")"
        install_dir "$plugin_dir" "$SHARED_PLUGIN_DIR/$name"
    done
else
    log "No shared/ directory found in source tree, skipping"
fi

# ---------------------------------------------------------------------------
log "Done."
