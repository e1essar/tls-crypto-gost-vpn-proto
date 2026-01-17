#!/usr/bin/env bash
set -euo pipefail

### ======== BASE SETTINGS (overridable via ENV) ========

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"

# If you built a custom OpenSSL+GOST provider using scripts/setup.sh, it typically
# creates this file with OPENSSL_ROOT/OPENSSL_MODULES and LD_LIBRARY_PATH.
ENV_FILE="${ENV_FILE:-$HOME/.gost-env.sh}"
if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi

SERVER_BIN="${SERVER_BIN:-$BUILD_DIR/server}"

CERT_PATH="${CERT_PATH:-$REPO_ROOT/certs/cert.pem}"
KEY_PATH="${KEY_PATH:-$REPO_ROOT/certs/key.pem}"

PORT="${PORT:-4433}"
CIPHER="${CIPHER:-any}"

NET_VPN="${NET_VPN:-10.8.0.0/24}"
SRV_IP="${SRV_IP:-10.8.0.1/24}"
SRV_IP_SHORT="${SRV_IP_SHORT:-10.8.0.1}"
TUN_IF="${TUN_IF:-tun0}"

# OpenSSL settings.
OPENSSL_ROOT="${OPENSSL_ROOT:-}"
OPENSSL_MODULES="${OPENSSL_MODULES:-}"

OPENSSL_BIN="${OPENSSL_BIN:-}"
if [[ -z "$OPENSSL_BIN" && -n "$OPENSSL_ROOT" && -x "$OPENSSL_ROOT/bin/openssl" ]]; then
  OPENSSL_BIN="$OPENSSL_ROOT/bin/openssl"
fi
if [[ -z "$OPENSSL_BIN" ]]; then
  OPENSSL_BIN="$(command -v openssl || true)"
fi

LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"

RUNDIR="${RUNDIR:-/var/run/tlsvpn}"
PIDF="$RUNDIR/server.pid"
LOGF="${LOGF:-/var/log/tlsvpn-server.log}"

### ======== HELPERS ========

die(){ echo "$*"; exit 1; }
need_root(){ if [[ $EUID -ne 0 ]]; then die "run as root"; fi; }

detect_wan(){
  WAN="${WAN:-$(ip route get 1.1.1.1 | awk '{for(i=1;i<=NF;i++) if ($i=="dev"){print $(i+1); exit}}')}"
  [[ -n "${WAN:-}" ]] || die "WAN auto-detect failed (set WAN=...)"
  echo "ℹ  WAN=$WAN"
}

sysctl_set(){ sysctl -w "$1"="$2" >/dev/null; }

kill_if_running(){
  [[ -f "$PIDF" ]] || return 0
  local pid; pid=$(cat "$PIDF" || true)
  if [[ -n "${pid:-}" ]] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" || true; sleep 0.2; kill -9 "$pid" 2>/dev/null || true
  fi
  rm -f "$PIDF"
}

nft_add_once(){
  # usage: nft_add_once ip nat 'POSTROUTING ...'
  local family="$1" table="$2" rest="$3"
  if nft --check add rule "$family" "$table" $rest 2>/dev/null; then
    nft add rule "$family" "$table" $rest
  fi
}

### ======== OPENSSL / PROVIDER SANITY CHECKS ========

openssl_sanity(){
  export LD_LIBRARY_PATH OPENSSL_MODULES
  [[ -x "$OPENSSL_BIN" ]] || die "OpenSSL not found: $OPENSSL_BIN"

  # Important: the GOST provider module must exist under OPENSSL_MODULES
  [[ -r "$OPENSSL_MODULES/gostprov.so" || -r "$OPENSSL_MODULES/gost.so" ]] \
    || die "GOST provider .so not found in $OPENSSL_MODULES"

  echo "ℹ  OpenSSL: $("$OPENSSL_BIN" version -a | head -n 1)"

  # Try loading gostprov first; if that fails, try gost; otherwise fail with diagnostics.
  if "$OPENSSL_BIN" list -providers -provider-path "$OPENSSL_MODULES" -provider gostprov -verbose >/dev/null 2>&1; then
    echo "ℹ  Provider 'gostprov' is loadable."
  elif "$OPENSSL_BIN" list -providers -provider-path "$OPENSSL_MODULES" -provider gost -verbose >/dev/null 2>&1; then
    echo "ℹ  Provider 'gost' is loadable (fallback)."
  else
    echo "==== providers debug ===="
    "$OPENSSL_BIN" list -providers -provider-path "$OPENSSL_MODULES" -verbose || true
    echo "========================="
    die "OpenSSL cannot load provider 'gostprov' (nor 'gost') with provider-path=$OPENSSL_MODULES"
  fi
}

### ======== ACTIONS ========

up() {
  need_root
  detect_wan
  openssl_sanity

  [[ -x "$SERVER_BIN" ]] || die "SERVER_BIN not found: $SERVER_BIN"
  [[ -e /dev/net/tun ]]  || die "/dev/net/tun missing (try: modprobe tun)"
  [[ -r "$CERT_PATH" ]]  || die "Certificate not found: $CERT_PATH"
  [[ -r "$KEY_PATH"  ]] || die "Key not found: $KEY_PATH"

  mkdir -p "$RUNDIR" "$(dirname "$LOGF")"

  if ss -lntp 2>/dev/null | grep -q ":$PORT\\b"; then
    echo " Port $PORT is already in use:"
    ss -lntp | grep ":$PORT\\b" || true
    exit 1
  fi

  echo "Bringing up TUN $TUN_IF..."
  ip link del "$TUN_IF" 2>/dev/null || true
  ip tuntap add dev "$TUN_IF" mode tun
  ip addr add "$SRV_IP" dev "$TUN_IF" 2>/dev/null || true
  ip link set "$TUN_IF" mtu 1400 up

  echo "Enabling forwarding and configuring NAT..."
  sysctl_set net.ipv4.ip_forward 1
  sysctl_set net.ipv4.conf.all.rp_filter 0
  sysctl_set net.ipv4.conf.default.rp_filter 0
  sysctl_set net.ipv4.conf."$TUN_IF".rp_filter 0
  sysctl_set net.ipv4.conf."$WAN".rp_filter 0

  nft add table ip nat 2>/dev/null || true
  nft add chain ip nat POSTROUTING '{ type nat hook postrouting priority srcnat; }' 2>/dev/null || true
  nft_add_once ip nat "POSTROUTING oifname \"$WAN\" ip saddr $NET_VPN counter masquerade"

  nft add table ip filter 2>/dev/null || true
  nft add chain ip filter FORWARD '{ type filter hook forward priority filter; policy drop; }' 2>/dev/null || true
  nft_add_once ip filter "FORWARD iifname \"$TUN_IF\" oifname \"$WAN\" counter accept"
  nft_add_once ip filter "FORWARD iifname \"$WAN\" oifname \"$TUN_IF\" ct state established,related counter accept"

  iptables -t nat -C POSTROUTING -s "$NET_VPN" -o "$WAN" -j MASQUERADE 2>/dev/null \
    || iptables -t nat -A POSTROUTING -s "$NET_VPN" -o "$WAN" -j MASQUERADE
  iptables -C FORWARD -i "$TUN_IF" -o "$WAN" -j ACCEPT 2>/dev/null \
    || iptables -A FORWARD -i "$TUN_IF" -o "$WAN" -j ACCEPT
  iptables -C FORWARD -i "$WAN" -o "$TUN_IF" -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null \
    || iptables -A FORWARD -i "$WAN" -o "$TUN_IF" -m state --state ESTABLISHED,RELATED -j ACCEPT

  echo "Starting server on 0.0.0.0:$PORT..."
  export LD_LIBRARY_PATH OPENSSL_MODULES
  echo "ENV: OPENSSL_MODULES=$OPENSSL_MODULES"
  : > "$LOGF"
  kill_if_running

  nohup "$SERVER_BIN" --port "$PORT" --cipher "$CIPHER" \
    --cert "$CERT_PATH" --key "$KEY_PATH" --tun "$TUN_IF" >>"$LOGF" 2>&1 &

  echo $! > "$PIDF"

  ok=0
  for i in {1..50}; do
    if ss -lntp 2>/dev/null | grep -q ":$PORT\\b"; then ok=1; break; fi
    sleep 0.1
  done
  if [[ $ok -ne 1 ]]; then
    echo "Server is not listening on port $PORT (it may have crashed). Logs:"
    tail -n 120 "$LOGF" 2>/dev/null || true
    exit 1
  fi

  echo "Server is up."
  status
  echo "Open port $PORT in your firewall/cloud rules if needed."
}

down() {
  need_root
  detect_wan
  echo "Stopping server..."
  kill_if_running

  echo "Cleaning up TUN and rules..."
  ip link del "$TUN_IF" 2>/dev/null || true

  nft list chain ip nat POSTROUTING >/dev/null 2>&1 && \
    nft delete rule ip nat POSTROUTING oifname "$WAN" ip saddr $NET_VPN counter masquerade 2>/dev/null || true

  nft list chain ip filter FORWARD >/dev/null 2>&1 && {
    nft delete rule ip filter FORWARD iifname "$TUN_IF" oifname "$WAN" counter accept 2>/dev/null || true
    nft delete rule ip filter FORWARD iifname "$WAN" oifname "$TUN_IF" ct state established,related counter accept 2>/dev/null || true
  }

  iptables -t nat -D POSTROUTING -s "$NET_VPN" -o "$WAN" -j MASQUERADE 2>/dev/null || true
  iptables -D FORWARD -i "$TUN_IF" -o "$WAN" -j ACCEPT 2>/dev/null || true
  iptables -D FORWARD -i "$WAN" -o "$TUN_IF" -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null || true

  echo "Server stopped."
}

status() {
  echo "==== Server status ===="
  ip -br addr show "$TUN_IF" 2>/dev/null || true
  echo "Routes:"; ip route | sed 's/^/  /'
  echo "nftables nat:"; nft list chain ip nat POSTROUTING 2>/dev/null || true
  echo "nftables FORWARD:"; nft list chain ip filter FORWARD 2>/dev/null || true
  echo "iptables nat POSTROUTING:"; iptables -t nat -vnL POSTROUTING 2>/dev/null | sed 's/^/  /' || true
  echo "iptables FORWARD:"; iptables -vnL FORWARD 2>/dev/null | sed 's/^/  /' || true
  if [[ -f "$PIDF" ]]; then echo "PID: $(cat "$PIDF")"; fi
  echo "Log tail:"; tail -n 60 "$LOGF" 2>/dev/null || true
}

ping_test() {
  echo "Ping from server to client over VPN (if the client is already connected):"
  ping -c 3 "${SRV_IP_SHORT%.*}.2" || true
}

help() {
  cat <<EOF
usage: $0 {up|down|status|ping}
Env: SERVER_BIN CERT_PATH KEY_PATH PORT CIPHER TUN_IF NET_VPN SRV_IP SRV_IP_SHORT WAN RUNDIR LOGF OPENSSL_ROOT OPENSSL_MODULES LD_LIBRARY_PATH

Checks:
  $OPENSSL_BIN x509 -in "\$CERT_PATH" -noout -text | grep -i 'Public Key Algorithm'   # should be GOST, not RSA/ECDSA
  $OPENSSL_BIN list -providers -provider-path "$OPENSSL_MODULES" -provider gostprov -verbose
  $OPENSSL_BIN s_client -connect 127.0.0.1:\$PORT -tls1_3 \\
      -provider-path "$OPENSSL_MODULES" -provider gostprov -provider default \\
      -ciphersuites "TLS_GOSTR341112_256_WITH_MAGMA_MGM_L"
EOF
}

case "${1:-}" in
  up) up ;;
  down) down ;;
  status) status ;;
  ping) ping_test ;;
  *) help ;;
esac
