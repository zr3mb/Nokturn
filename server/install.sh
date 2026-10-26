#!/usr/bin/env bash
# Installs (or updates) the Nokturn online server on Ubuntu 22.04 / 24.04:
#   Python venv + aiohttp, a systemd service, nginx as the reverse proxy and a Let's Encrypt
#   certificate from certbot. Running it again updates the code and restarts the service; the
#   database in /var/lib/nokturn stays.
#
#   sudo ./server/install.sh                                  # nokturn.404piwko.wtf
#   sudo ./server/install.sh --domain music.example.com --email me@example.com
#   sudo ./server/install.sh --no-tls                         # plain HTTP (testing, or TLS terminated elsewhere)
#   sudo ./server/install.sh --no-nginx --host 172.18.0.1     # another proxy in front (e.g. Caddy in Docker)
#   sudo ./server/install.sh --uninstall                      # removes the service and the nginx site, keeps the data
# --no-nginx and --host are remembered in /etc/nokturn-server.env, so later updates keep them.
set -euo pipefail

DOMAIN="nokturn.404piwko.wtf"
EMAIL=""
PORT="8710"
TLS=1
UNINSTALL=0
NGINX=""
HOST=""

APP_DIR="/opt/nokturn-server"
DATA_DIR="/var/lib/nokturn"
ENV_FILE="/etc/nokturn-server.env"
SERVICE="nokturn-server"
SERVICE_USER="nokturn"
NGINX_SITE="/etc/nginx/sites-available/nokturn"

usage() {
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --domain) DOMAIN="$2"; shift 2 ;;
        --email) EMAIL="$2"; shift 2 ;;
        --port) PORT="$2"; shift 2 ;;
        --no-tls) TLS=0; shift ;;
        --no-nginx) NGINX=0; shift ;;
        --host) HOST="$2"; shift 2 ;;
        --uninstall) UNINSTALL=1; shift ;;
        -h|--help) usage 0 ;;
        *) echo "Unknown option: $1" >&2; usage 1 ;;
    esac
done

say() { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n' "$*" >&2; }
die() { printf '\033[1;31mxx\033[0m %s\n' "$*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "Run it as root: sudo $0 $*"
[[ "$DOMAIN" =~ ^[A-Za-z0-9.-]+$ ]] || die "Invalid domain: $DOMAIN"
[[ "$PORT" =~ ^[0-9]+$ ]] || die "Invalid port: $PORT"

# An update keeps the proxy choice and the listening address of the previous install.
if [[ -f "$ENV_FILE" ]]; then
    previous_host="$(sed -n 's/^NOKTURN_HOST=//p' "$ENV_FILE" | tail -n1)"
    previous_proxy="$(sed -n 's/^NOKTURN_PROXY=//p' "$ENV_FILE" | tail -n1)"
    [[ -z "$HOST" ]] && HOST="$previous_host"
    [[ -z "$NGINX" && "$previous_proxy" == "external" ]] && NGINX=0
fi
NGINX="${NGINX:-1}"
HOST="${HOST:-127.0.0.1}"
[[ "$HOST" =~ ^[0-9A-Fa-f.:]+$ ]] || die "Invalid address: $HOST"

if [[ $UNINSTALL -eq 1 ]]; then
    say "Removing the service and the nginx site (the data in $DATA_DIR stays)"
    systemctl disable --now "$SERVICE" "$SERVICE-ytdlp.timer" 2>/dev/null || true
    rm -f "/etc/systemd/system/$SERVICE.service" "/etc/systemd/system/$SERVICE-ytdlp.service" \
          "/etc/systemd/system/$SERVICE-ytdlp.timer" /etc/nginx/sites-enabled/nokturn "$NGINX_SITE"
    systemctl daemon-reload
    if command -v nginx >/dev/null && nginx -t 2>/dev/null; then systemctl reload nginx; fi
    rm -rf "$APP_DIR"
    say "Done. Delete $DATA_DIR, $ENV_FILE and the '$SERVICE_USER' user by hand if you no longer need them."
    exit 0
fi

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[[ -f "$SRC_DIR/nokturn_server/app.py" ]] || die "Run the script from the repository (server/install.sh)"

if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    [[ "${ID:-}" == "ubuntu" || "${ID_LIKE:-}" == *debian* ]] || warn "Made for Ubuntu; this is ${PRETTY_NAME:-unknown}"
fi

say "Installing packages"
export DEBIAN_FRONTEND=noninteractive
apt-get update -q
packages=(python3 python3-venv curl sqlite3 unzip)
[[ $NGINX -eq 1 ]] && packages+=(nginx)
[[ $NGINX -eq 1 && $TLS -eq 1 ]] && packages+=(certbot python3-certbot-nginx)
apt-get install -y -q "${packages[@]}"

say "Service user and directories"
if ! id "$SERVICE_USER" >/dev/null 2>&1; then
    useradd --system --home-dir "$DATA_DIR" --shell /usr/sbin/nologin "$SERVICE_USER"
fi
install -d -o "$SERVICE_USER" -g "$SERVICE_USER" -m 0750 "$DATA_DIR"
install -d -m 0755 "$APP_DIR"

say "Copying the server to $APP_DIR"
rm -rf "$APP_DIR/nokturn_server"
cp -r "$SRC_DIR/nokturn_server" "$APP_DIR/"
cp "$SRC_DIR/requirements.txt" "$APP_DIR/"
find "$APP_DIR/nokturn_server" -name '__pycache__' -prune -exec rm -rf {} +

say "Python environment"
if [[ ! -x "$APP_DIR/venv/bin/python" ]]; then
    python3 -m venv "$APP_DIR/venv"
fi
"$APP_DIR/venv/bin/pip" install -q --upgrade pip
"$APP_DIR/venv/bin/pip" install -q -r "$APP_DIR/requirements.txt"

# The Android app plays YouTube through the server: yt-dlp resolves the tracks and Deno solves
# YouTube's JavaScript challenge for it. yt-dlp is kept up to date by a daily timer.
say "yt-dlp and Deno (audio for the Android app)"
"$APP_DIR/venv/bin/pip" install -q --upgrade "yt-dlp[default]"
install -d -m 0755 "$APP_DIR/bin"
case "$(uname -m)" in
    x86_64) DENO_ARCH="x86_64" ;;
    aarch64) DENO_ARCH="aarch64" ;;
    *) DENO_ARCH="" ;;
esac
if [[ -n "$DENO_ARCH" && ! -x "$APP_DIR/bin/deno" ]]; then
    tmp="$(mktemp -d)"
    if curl -fsSL -o "$tmp/deno.zip" "https://github.com/denoland/deno/releases/latest/download/deno-$DENO_ARCH-unknown-linux-gnu.zip"; then
        unzip -q -o "$tmp/deno.zip" -d "$APP_DIR/bin"
        chmod 0755 "$APP_DIR/bin/deno"
    else
        warn "Couldn't download Deno; some YouTube tracks may not play in the Android app."
    fi
    rm -rf "$tmp"
fi
install -d -o "$SERVICE_USER" -g "$SERVICE_USER" -m 0750 "$DATA_DIR/cache"

PUBLIC_URL="https://$DOMAIN"
[[ $TLS -eq 0 ]] && PUBLIC_URL="http://$DOMAIN"
cat > "$ENV_FILE" <<EOF
# Read by $SERVICE.service. Change and run: sudo systemctl restart $SERVICE
NOKTURN_HOST=$HOST
NOKTURN_PORT=$PORT
NOKTURN_DB=$DATA_DIR/nokturn.db
NOKTURN_PUBLIC_URL=$PUBLIC_URL
NOKTURN_PROXY=$([[ $NGINX -eq 1 ]] && echo nginx || echo external)
EOF
chmod 0644 "$ENV_FILE"

say "systemd service"
cat > "/etc/systemd/system/$SERVICE.service" <<EOF
[Unit]
Description=Nokturn online server (share links, shared playlists, listening rooms)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_USER
EnvironmentFile=$ENV_FILE
Environment=PATH=$APP_DIR/bin:$APP_DIR/venv/bin:/usr/local/bin:/usr/bin:/bin
Environment=XDG_CACHE_HOME=$DATA_DIR/cache
Environment=DENO_DIR=$DATA_DIR/cache/deno
WorkingDirectory=$APP_DIR
ExecStart=$APP_DIR/venv/bin/python -m nokturn_server
Restart=on-failure
RestartSec=3
# Hardening: the service only writes its database.
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
PrivateDevices=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX
RestrictNamespaces=yes
LockPersonality=yes
ReadWritePaths=$DATA_DIR

[Install]
WantedBy=multi-user.target
EOF
cat > "/etc/systemd/system/$SERVICE-ytdlp.service" <<EOF
[Unit]
Description=Update yt-dlp for the Nokturn server
After=network-online.target

[Service]
Type=oneshot
ExecStart=$APP_DIR/venv/bin/pip install -q --upgrade yt-dlp[default]
EOF
cat > "/etc/systemd/system/$SERVICE-ytdlp.timer" <<EOF
[Unit]
Description=Daily yt-dlp update for the Nokturn server

[Timer]
OnCalendar=daily
RandomizedDelaySec=1h
Persistent=true

[Install]
WantedBy=timers.target
EOF
systemctl daemon-reload
systemctl enable "$SERVICE" >/dev/null
systemctl enable --now "$SERVICE-ytdlp.timer" >/dev/null
systemctl restart "$SERVICE"

if [[ $NGINX -eq 1 ]]; then
# (not indented: the heredocs below end at the start of a line)
say "nginx"
# certbot adds the HTTPS server block to this file; on an update an existing certificate is kept.
if [[ $TLS -eq 1 && -f "/etc/letsencrypt/live/$DOMAIN/fullchain.pem" ]]; then
    HAVE_CERT=1
else
    HAVE_CERT=0
fi
# Some VPSes have no IPv6 — nginx refuses to start with a [::] listener there.
if [[ -f /proc/net/if_inet6 ]]; then
    LISTEN6_HTTP="listen [::]:80;"
    LISTEN6_HTTPS="listen [::]:443 ssl http2;"
else
    LISTEN6_HTTP=""
    LISTEN6_HTTPS=""
fi
proxy_block() {
    cat <<EOF
    client_max_body_size 1m;

    location /api/v1/ws {
        proxy_pass http://127.0.0.1:$PORT;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_read_timeout 1h;
        proxy_send_timeout 1h;
    }

    location /api/v1/stream/ {
        proxy_pass http://127.0.0.1:$PORT;
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_buffering off;
        proxy_read_timeout 2m;
    }

    location / {
        proxy_pass http://127.0.0.1:$PORT;
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
EOF
}
if [[ $HAVE_CERT -eq 1 ]]; then
    {
        cat <<EOF
server {
    listen 80;
    $LISTEN6_HTTP
    server_name $DOMAIN;
    return 301 https://\$host\$request_uri;
}

server {
    listen 443 ssl http2;
    $LISTEN6_HTTPS
    server_name $DOMAIN;

    ssl_certificate /etc/letsencrypt/live/$DOMAIN/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/$DOMAIN/privkey.pem;
    include /etc/letsencrypt/options-ssl-nginx.conf;
    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem;

EOF
        proxy_block
        echo "}"
    } > "$NGINX_SITE"
else
    {
        cat <<EOF
server {
    listen 80;
    $LISTEN6_HTTP
    server_name $DOMAIN;

EOF
        proxy_block
        echo "}"
    } > "$NGINX_SITE"
fi
ln -sf "$NGINX_SITE" /etc/nginx/sites-enabled/nokturn
nginx -t
systemctl enable nginx >/dev/null
if systemctl is-active --quiet nginx; then
    systemctl reload nginx
elif ! systemctl restart nginx; then
    journalctl -u nginx -n 15 --no-pager >&2 || true
    if command -v ss >/dev/null; then
        busy="$(ss -ltnpH '( sport = :80 or sport = :443 )' 2>/dev/null || true)"
        [[ -n "$busy" ]] && warn "Ports 80/443 are taken by another program (e.g. Apache, Caddy, Docker):"$'\n'"$busy"
    fi
    die "nginx doesn't start — see the log above. Free ports 80/443 (e.g. sudo systemctl disable --now apache2) and run the script again."
fi

if command -v ufw >/dev/null && ufw status 2>/dev/null | grep -q "Status: active"; then
    say "Opening ports 80 and 443 in ufw"
    ufw allow 'Nginx Full' >/dev/null
fi

if [[ $TLS -eq 1 && $HAVE_CERT -eq 0 ]]; then
    say "Let's Encrypt certificate for $DOMAIN"
    # The domain has to point at this server already (an A / AAAA record), or the challenge fails.
    resolved="$(getent ahosts "$DOMAIN" | awk '{print $1}' | sort -u | tr '\n' ' ' || true)"
    public="$(curl -fsS -4 --max-time 5 https://api.ipify.org 2>/dev/null || true)"
    if [[ -z "$resolved" ]]; then
        warn "$DOMAIN doesn't resolve yet — add the DNS record, then run the script again."
    elif [[ -n "$public" && " $resolved " != *" $public "* ]]; then
        warn "$DOMAIN points at $resolved, this server is $public — certbot will probably fail."
    fi
    certbot_args=(--nginx -d "$DOMAIN" --non-interactive --agree-tos --redirect)
    if [[ -n "$EMAIL" ]]; then
        certbot_args+=(-m "$EMAIL")
    else
        certbot_args+=(--register-unsafely-without-email)
    fi
    if ! certbot "${certbot_args[@]}"; then
        warn "No certificate yet. The server runs over HTTP; fix DNS and run the script again."
    fi
fi
else
    say "No nginx: the proxy in front of the server forwards $DOMAIN to $HOST:$PORT"
fi

EXTRA_ARGS=""
[[ "$DOMAIN" != "nokturn.404piwko.wtf" ]] && EXTRA_ARGS+=" --domain $DOMAIN"
[[ $TLS -eq 0 ]] && EXTRA_ARGS+=" --no-tls"

say "Checking the server"
check_host="$HOST"
[[ "$check_host" == "0.0.0.0" ]] && check_host="127.0.0.1"
[[ "$check_host" == *:* ]] && check_host="[$check_host]"
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if curl -fsS "http://$check_host:$PORT/api/v1/health" >/dev/null 2>&1; then
        break
    fi
    sleep 1
done
if curl -fsS "http://$check_host:$PORT/api/v1/health" >/dev/null 2>&1; then
    say "Running: $PUBLIC_URL"
else
    warn "The service doesn't answer — see: journalctl -u $SERVICE -n 50"
    exit 1
fi

cat <<EOF

  Logs:      journalctl -u $SERVICE -f
  Restart:   sudo systemctl restart $SERVICE
  Database:  $DATA_DIR/nokturn.db (back it up with: sqlite3 $DATA_DIR/nokturn.db ".backup /root/nokturn.db")
  Update:    git pull && sudo ./server/install.sh$EXTRA_ARGS
EOF
