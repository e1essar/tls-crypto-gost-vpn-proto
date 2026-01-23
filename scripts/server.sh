#!/usr/bin/env bash
set -euo pipefail

SERVER_BIN="${SERVER_BIN:-/home/ubuntu/Desktop/vpn/build/server}"

CERT_PATH="${CERT_PATH:-/home/ubuntu/Desktop/vpn/certs/cert.pem}"
KEY_PATH="${KEY_PATH:-/home/ubuntu/Desktop/vpn/certs/key.pem}"

PORT="${PORT:-4433}"
CIPHER="${CIPHER:-any}"

NET_VPN="${NET_VPN:-10.8.0.0/24}"
SRV_IP="${SRV_IP:-10.8.0.1/24}"
SRV_IP_SHORT="${SRV_IP_SHORT:-10.8.0.1}"
TUN_IF="${TUN_IF:-tun0}"

OPENSSL_ROOT="${OPENSSL_ROOT:-/home/ubuntu/opt}"
OPENSSL_BIN="$OPENSSL_ROOT/bin/openssl"
OPENSSL_MODULES="${OPENSSL_MODULES:-/home/ubuntu/src/gost-setup/engine/build/bin}"
LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-$OPENSSL_ROOT/lib}"

RUNDIR="${RUNDIR:-/var/run/tlsvpn}"
PIDF="$RUNDIR/server.pid"
LOGF="${LOGF:-/var/log/tlsvpn-server.log}"

die(){ echo "❌ $*"; exit 1; }
need_root(){ if [[ $EUID -ne 0 ]]; then die "run as root"; fi; }

detect_wan(){
  WAN="${WAN:-$(ip route get 1.1.1.1 | awk '{for(i=1;i<=NF;i++) if ($i=="dev"){print $(i+1); exit}}')}"
  [[ -n "${WAN:-}" ]] || die "WAN detect failed (set WAN=...)"
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
  # usage: nft_add_once ip nat 'POSTROUTING ... rule ...'
  local family="$1" table="$2" rest="$3"
  if nft --check add rule "$family" "$table" $rest 2>/dev/null; then
    nft add rule "$family" "$table" $rest
  fi
}

openssl_sanity(){
  export LD_LIBRARY_PATH OPENSSL_MODULES
  [[ -x "$OPENSSL_BIN" ]] || die "OpenSSL not found: $OPENSSL_BIN"
  [[ -r "$OPENSSL_MODULES/gostprov.so" || -r "$OPENSSL_MODULES/gost.so" ]] \
    || die "gost provider .so not found in $OPENSSL_MODULES"

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

up() {
  need_root; detect_wan; openssl_sanity

  [[ -x "$SERVER_BIN" ]] || die "SERVER_BIN not found: $SERVER_BIN"
  [[ -e /dev/net/tun ]]  || die "/dev/net/tun missing (modprobe tun)"
  [[ -r "$CERT_PATH" ]]  || die "cert not found: $CERT_PATH"
  [[ -r "$KEY_PATH"  ]]  || die "key not found: $KEY_PATH"

  mkdir -p "$RUNDIR" "$(dirname "$LOGF")"

  #if ! strings "$SERVER_BIN" | grep -q -E -- '--tun|TUN ready'; then
  #  die "Этот server вероятно без поддержки --tun. Пересоберите (Tun.cpp/Server.cpp + заголовок)."
  #fi

  if ss -lntp 2>/dev/null | grep -q ":$PORT\\b"; then
    echo "❌ Порт $PORT уже занят:"
    ss -lntp | grep ":$PORT\\b" || true
    exit 1
  fi

  echo "🛠  Поднимаю TUN $TUN_IF…"
  ip link del "$TUN_IF" 2>/dev/null || true
  ip tuntap add dev "$TUN_IF" mode tun
  ip addr add "$SRV_IP" dev "$TUN_IF" 2>/dev/null || true
  ip link set "$TUN_IF" mtu 1400 up

  echo "🔧 Включаю форвардинг и настраиваю NAT…"
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
    || iptables -A FORWARD -i "$TUN_IF" -о "$WAN" -j ACCEPT
  iptables -C FORWARD -i "$WAN" -o "$TUN_IF" -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null \
    || iptables -A FORWARD -i "$WAN" -o "$TUN_IF" -m state --state ESTABLISHED,RELATED -j ACCEPT

  echo "▶  Стартую сервер на 0.0.0.0:$PORT…"
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
    echo "❌ Сервер не слушает порт $PORT (возможно, упал). Логи:"
    tail -n 120 "$LOGF" 2>/dev/null || true
    exit 1
  fi

  echo "✅ Сервер поднят."
  status
  echo "👉 Открой порт $PORT в firewall/cloud (если нужно)."
}

down() {
  need_root; detect_wan
  echo "⏹ Останавливаю сервер…"
  kill_if_running

  echo "🧹 Чищу TUN и правила…"
  ip link del "$TUN_IF" 2>/dev/null || true

  nft list chain ip nat POSTROUTING >/dev/null 2>&1 && \
    nft delete rule ip nat POSTROUTING oifname "$WAN" ip saddr $NET_VPN counter masquerade 2>/dev/null || true

  nft list chain ip filter FORWARD >/dev/null 2>&1 && {
    nft delete rule ip filter FORWARD iifname "$TUN_IF" oifname "$WAN" counter accept 2>/dev/null || true
    nft delete rule ip filter FORWARD iifname "$WAN" oifname "$TUN_IF" ct state established,related counter accept 2>/dev/null || true
  }

  iptables -t nat -D POSTROUTING -s "$NET_VPN" -о "$WAN" -j MASQUERADE 2>/dev/null || true
  iptables -D FORWARD -i "$TUN_IF" -o "$WAN" -j ACCEPT 2>/dev/null || true
  iptables -D FORWARD -i "$WAN" -o "$TUN_IF" -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null || true

  echo "✅ Сервер остановлен."
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
  echo "🔎 Пинг из сервера в клиента по VPN (если клиент уже подключен):"
  ping -c 3 "${SRV_IP_SHORT%.*}.2" || true
}

help() {
  cat <<EOF
usage: $0 {up|down|status|ping}
Env: SERVER_BIN CERT_PATH KEY_PATH PORT CIPHER TUN_IF NET_VPN SRV_IP SRV_IP_SHORT WAN RUNDIR LOGF OPENSSL_ROOT OPENSSL_MODULES LD_LIBRARY_PATH

Проверки:
  $OPENSSL_BIN x509 -in "\$CERT_PATH" -noout -text | grep -i 'Public Key Algorithm'   # должен быть GOST, не RSA/ECDSA
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
