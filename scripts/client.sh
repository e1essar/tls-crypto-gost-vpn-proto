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

CLIENT_BIN="${CLIENT_BIN:-$BUILD_DIR/client}"

# TLS server IP/port (regular network between VMs, NOT the VPN subnet)
SERVER_ADDR="${SERVER_ADDR:-10.0.0.1}"   # <-- set your server IP here
PORT="${PORT:-4433}"
CIPHER="${CIPHER:-any}"

# VPN subnet settings (must match the server side)
NET_VPN="${NET_VPN:-10.8.0.0/24}"
CLT_IP="${CLT_IP:-10.8.0.2/24}"
CLT_IP_SHORT="${CLT_IP_SHORT:-10.8.0.2}"
SRV_IP_SHORT="${SRV_IP_SHORT:-10.8.0.1}"   # server IP inside the VPN subnet
TUN_IF="${TUN_IF:-tun0}"

# Common OpenSSL prefix (same idea as on the server)
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
PIDF="$RUNDIR/client.pid"
LOGF="${LOGF:-/var/log/tlsvpn-client.log}"

### ======== HELPERS ========

die(){ echo "$*"; exit 1; }
need_root(){ if [[ $EUID -ne 0 ]]; then die "run as root"; fi; }

sysctl_set(){ sysctl -w "$1"="$2" >/dev/null; }

kill_if_running(){
  [[ -f "$PIDF" ]] || return 0
  local pid; pid=$(cat "$PIDF" || true)
  if [[ -n "${pid:-}" ]] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" || true; sleep 0.2; kill -9 "$pid" 2>/dev/null || true
  fi
  rm -f "$PIDF"
}

### ======== OPENSSL / PROVIDER SANITY CHECKS ========

openssl_sanity(){
  export LD_LIBRARY_PATH OPENSSL_MODULES
  [[ -x "$OPENSSL_BIN" ]] || die "OpenSSL not found: $OPENSSL_BIN"

  # Important: the GOST provider module must be present in OPENSSL_MODULES
  [[ -r "$OPENSSL_MODULES/gostprov.so" || -r "$OPENSSL_MODULES/gost.so" ]] \
    || die "GOST provider .so not found in $OPENSSL_MODULES"

  echo "ℹ  OpenSSL: $("$OPENSSL_BIN" version -a | head -n 1)"

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
  openssl_sanity

  [[ -x "$CLIENT_BIN" ]] || die "CLIENT_BIN not found: $CLIENT_BIN"
  [[ -e /dev/net/tun ]]  || die "/dev/net/tun missing (try: modprobe tun)"

  mkdir -p "$RUNDIR" "$(dirname "$LOGF")"

  echo "Bringing up TUN $TUN_IF on the client..."
  ip link del "$TUN_IF" 2>/dev/null || true
  ip tuntap add dev "$TUN_IF" mode tun
  ip addr add "$CLT_IP" dev "$TUN_IF" 2>/dev/null || true
  ip link set "$TUN_IF" mtu 1400 up

  echo "🔧 Applying sysctl tweaks..."
  sysctl_set net.ipv4.conf.all.rp_filter 0
  sysctl_set net.ipv4.conf.default.rp_filter 0
  sysctl_set net.ipv4.conf."$TUN_IF".rp_filter 0

  # Important: decide whether you want only NET_VPN routed via the tunnel,
  # or all traffic (default route) via the tunnel.
  echo "➡  Adding VPN route via $SRV_IP_SHORT..."
  ip route replace "$NET_VPN" via "$SRV_IP_SHORT" dev "$TUN_IF" || true
  ip route add default dev "$TUN_IF" || true

  echo "Starting client: $SERVER_ADDR:$PORT..."
  export LD_LIBRARY_PATH OPENSSL_MODULES
  : > "$LOGF"
  kill_if_running

  # Important: adjust client flags to match YOUR binary.
  nohup "$CLIENT_BIN" \
    --host "$SERVER_ADDR" --port "$PORT" --cipher "$CIPHER" \
    --tun "$TUN_IF" >>"$LOGF" 2>&1 &

  echo $! > "$PIDF"

  sleep 1
  if ! ps -p "$(cat "$PIDF")" >/dev/null 2>&1; then
    echo "Client process crashed. Logs:"
    tail -n 120 "$LOGF" 2>/dev/null || true
    exit 1
  fi

  echo "✅ Client is up."
  status
}

down() {
  need_root
  echo "Stopping client..."
  kill_if_running

  echo "Cleaning up TUN and routes..."
  ip route del "$NET_VPN" via "$SRV_IP_SHORT" dev "$TUN_IF" 2>/dev/null || true
  ip link del "$TUN_IF" 2>/dev/null || true

  echo "Client stopped."
}

status() {
  echo "==== Client status ===="
  ip -br addr show "$TUN_IF" 2>/dev/null || true
  echo "Routes:"; ip route | sed 's/^/  /'
  echo "Log tail:"; tail -n 60 "$LOGF" 2>/dev/null || true
  if [[ -f "$PIDF" ]]; then echo "PID: $(cat "$PIDF")"; fi
}

ping_test() {
  echo "Ping from client to server over VPN (if connected):"
  ping -c 3 "$SRV_IP_SHORT" || true

  echo "Ping a public IP (example):"
  ping -c 3 5.255.255.242 || true
}

help() {
  cat <<EOF
usage: $0 {up|down|status|ping}
Env: CLIENT_BIN SERVER_ADDR PORT CIPHER TUN_IF NET_VPN CLT_IP CLT_IP_SHORT SRV_IP_SHORT RUNDIR LOGF OPENSSL_ROOT OPENSSL_MODULES LD_LIBRARY_PATH

Checks:
  $OPENSSL_BIN x509 -in "\$CERT_PATH" -noout -text | grep -i 'Public Key Algorithm'
  $OPENSSL_BIN s_client -connect \$SERVER_ADDR:\$PORT -tls1_3 \\
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
