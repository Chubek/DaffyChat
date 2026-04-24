#!/bin/bash
# install-frontend.sh – deploy DaffyChat frontend assets to the web root
#
# Called by post-install.sh when PACK_FRONTEND=1 (the default).
# Can also be run standalone after the package is installed.
#
# Env overrides:
#   DAFFY_FRONTEND_SRC   source of assets  (default /usr/share/daffychat/frontend)
#   DAFFY_WEB_ROOT       destination       (default /var/www/daffychat)
#   DAFFY_WEB_USER       owner             (default www-data, falls back to root)
#   DAFFY_WEB_GROUP      group             (default www-data, falls back to root)

set -e

FRONTEND_SRC="${DAFFY_FRONTEND_SRC:-/usr/share/daffychat/frontend}"
WEB_ROOT="${DAFFY_WEB_ROOT:-/var/www/daffychat}"

# Resolve web server user/group
if id "www-data" >/dev/null 2>&1; then
    WEB_USER="${DAFFY_WEB_USER:-www-data}"
    WEB_GROUP="${DAFFY_WEB_GROUP:-www-data}"
elif id "nginx" >/dev/null 2>&1; then
    WEB_USER="${DAFFY_WEB_USER:-nginx}"
    WEB_GROUP="${DAFFY_WEB_GROUP:-nginx}"
elif id "http" >/dev/null 2>&1; then
    WEB_USER="${DAFFY_WEB_USER:-http}"
    WEB_GROUP="${DAFFY_WEB_GROUP:-http}"
else
    WEB_USER="${DAFFY_WEB_USER:-root}"
    WEB_GROUP="${DAFFY_WEB_GROUP:-root}"
fi

echo "[install-frontend] Source : $FRONTEND_SRC"
echo "[install-frontend] Web root: $WEB_ROOT"
echo "[install-frontend] Owner  : $WEB_USER:$WEB_GROUP"

# -------------------------------------------------------------------------
# Guard: if the source directory doesn't exist we can't install
# -------------------------------------------------------------------------
if [ ! -d "$FRONTEND_SRC" ]; then
    echo "Warning: frontend source not found at $FRONTEND_SRC – skipping"
    exit 0
fi

# -------------------------------------------------------------------------
# Create destination and copy assets
# -------------------------------------------------------------------------
mkdir -p "$WEB_ROOT"

echo "[install-frontend] Copying frontend assets..."
cp -r "$FRONTEND_SRC/." "$WEB_ROOT/"

# -------------------------------------------------------------------------
# Fix ownership and permissions
# -------------------------------------------------------------------------
if chown -R "$WEB_USER:$WEB_GROUP" "$WEB_ROOT" 2>/dev/null; then
    echo "[install-frontend] Set ownership $WEB_USER:$WEB_GROUP on $WEB_ROOT"
else
    echo "Warning: could not set ownership (running as non-root?)"
fi

find "$WEB_ROOT" -type d -exec chmod 755 {} +
find "$WEB_ROOT" -type f -exec chmod 644 {} +

# -------------------------------------------------------------------------
# Write a basic nginx snippet if nginx is installed
# -------------------------------------------------------------------------
NGINX_SNIPPETS_DIR="/etc/nginx/snippets"
if [ -d "$NGINX_SNIPPETS_DIR" ] && [ ! -f "$NGINX_SNIPPETS_DIR/daffychat.conf" ]; then
    cat > "$NGINX_SNIPPETS_DIR/daffychat.conf" << 'NGINX'
# DaffyChat frontend – include this snippet in your server block:
#   include snippets/daffychat.conf;
root /var/www/daffychat;
index index.html;

location / {
    try_files $uri $uri/ /index.html;
}

# Route HTTP errors to the dynamic error page
error_page 400 401 403 404 405 408 409 410 413 414 415 418 422 429 500 501 502 503 504 505
    /dynerror.html;
location = /dynerror.html {
    internal;
    add_header Cache-Control "no-store";
}
NGINX
    echo "[install-frontend] Wrote nginx snippet: $NGINX_SNIPPETS_DIR/daffychat.conf"
fi

echo "[install-frontend] Done.  Frontend deployed to $WEB_ROOT"
