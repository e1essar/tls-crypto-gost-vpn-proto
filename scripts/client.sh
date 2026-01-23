#!/usr/bin/env bash
set -euo pipefail

CLIENT_BIN="${CLIENT_BIN:-/home/ubuntu/Desktop/vpn/build/client}"

SERVER_ADDR="${SERVER_ADDR:-10.0.0.1}"
PORT="${PORT:-4433}"
CIPHER="${CIPHER:-any}"

NET_VPN="${NET_VPN:-10.8.0.0/24}"
CLT_IP="${CLT_IP:-10.8.0.2/24}"
CLT_IP_SHORT="${CLT_IP_SHORT:-10.8.0.2}"
SRV_IP_SHORT="${SRV_IP_SHORT:-10.8.0.1}"
TUN_IF="${TUN_IF:-tun0}"

OPENSSL_ROOT="${OPENSSL_ROOT:-/home/ubuntu/opt}"
OPENSSL_BIN="$OPENSSL_ROOT/bin/openssl"
OPENSSL_MODULES="${OPENSSL_MODULES:-/home/ubuntu/src/gost-setup/engine/build/bin}"
LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-$OPENSSL_ROOT/lib}"

RUNDIR="${RUNDIR:-/var/run/tlsvpn}"
PIDF="$RUNDIR/client.pid"
LOGF="${LOGF:-/var/log/tlsvpn-client.log}"

die(){ echo "❌ $*"; exit 1; }
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
  need_root; openssl_sanity

  [[ -x "$CLIENT_BIN" ]] || die "CLIENT_BIN not found: $CLIENT_BIN"
  [[ -e /dev/net/tun ]]  || die "/dev/net/tun missing (modprobe tun)"

  mkdir -p "$RUNDIR" "$(dirname "$LOGF")"

  echo "🛠  Поднимаю TUN $TUN_IF на клиенте…"
  ip link del "$TUN_IF" 2>/dev/null || true
  ip tuntap add dev "$TUN_IF" mode tun
  ip addr add "$CLT_IP" dev "$TUN_IF" 2>/dev/null || true
  ip link set "$TUN_IF" mtu 1400 up

  echo "🔧 Настраиваю sysctl…"
  sysctl_set net.ipv4.conf.all.rp_filter 0
  sysctl_set net.ipv4.conf.default.rp_filter 0
  sysctl_set net.ipv4.conf."$TUN_IF".rp_filter 0

  echo "➡  Добавляю маршрут в VPN-сеть через сервер $SRV_IP_SHORT…"
  ip route replace "$NET_VPN" via "$SRV_IP_SHORT" dev "$TUN_IF" || true
  ip route add default dev "$TUN_IF" || true
  
  #echo "➡  Настраиваю маршруты для VPN…"

	#ORIG_GW=$(ip route show default | awk '{print $3}' | head -n1)
	#ORIG_DEV=$(ip route show default | awk '{print $5}' | head -n1)

	#mkdir -p "$RUNDIR"
	#echo "$ORIG_GW $ORIG_DEV" > "$RUNDIR/orig_default.route"

	#echo "   Оригинальный default: via $ORIG_GW dev $ORIG_DEV"
	#ip route replace "$SERVER_ADDR" via "$ORIG_GW" dev "$ORIG_DEV" || true

	#ip route replace "$NET_VPN" dev "$TUN_IF" || true

	#ip route replace default dev "$TUN_IF" || true
	#echo "   Новый default: dev $TUN_IF"


  echo "▶  Стартую клиент: $SERVER_ADDR:$PORT…"
  export LD_LIBRARY_PATH OPENSSL_MODULES
  : > "$LOGF"
  kill_if_running

  nohup "$CLIENT_BIN" \
    --host "$SERVER_ADDR" --port "$PORT" --cipher "$CIPHER" \
    --tun "$TUN_IF" >>"$LOGF" 2>&1 &

  echo $! > "$PIDF"

  sleep 1
  if ! ps -p "$(cat "$PIDF")" >/dev/null 2>&1; then
    echo "❌ Клиентский процесс упал. Логи:"
    tail -n 120 "$LOGF" 2>/dev/null || true
    exit 1
  fi

  echo "✅ Клиент поднят."
  status
}

down() {
  need_root
  echo "⏹ Останавливаю клиент…"
  kill_if_running

  echo "🧹 Чищу TUN и маршрут…"
  ip route del "$NET_VPN" via "$SRV_IP_SHORT" dev "$TUN_IF" 2>/dev/null || true
  ip link del "$TUN_IF" 2>/dev/null || true
  
	#ip route del "$NET_VPN" dev "$TUN_IF" 2>/dev/null || true

	#ip route del "$SERVER_ADDR" 2>/dev/null || true

	#if [[ -f "$RUNDIR/orig_default.route" ]]; then
	  #read -r ORIG_GW ORIG_DEV < "$RUNDIR/orig_default.route"
	  #ip route replace default via "$ORIG_GW" dev "$ORIG_DEV" 2>/dev/null || true
	  #rm -f "$RUNDIR/orig_default.route"
	  #echo "   Восстановлен default: via $ORIG_GW dev $ORIG_DEV"
	#fi

	#ip link del "$TUN_IF" 2>/dev/null || true

  echo "✅ Клиент остановлен."
}

status() {
  echo "==== Client status ===="
  ip -br addr show "$TUN_IF" 2>/dev/null || true
  echo "Routes:"; ip route | sed 's/^/  /'
  echo "Log tail:"; tail -n 60 "$LOGF" 2>/dev/null || true
  if [[ -f "$PIDF" ]]; then echo "PID: $(cat "$PIDF")"; fi
}

ping_test() {
  echo "🔎 Пинг из клиента в сервер по VPN (если подключено):"
  ping -c 3 "$SRV_IP_SHORT" || true
  
  echo "🔎 Пинг из клиента в сервер по VPN (ya.ru):"
  ping -c 3 5.255.255.242 || true
}

help() {
  cat <<EOF
usage: $0 {up|down|status|ping}
Env: CLIENT_BIN CERT_PATH KEY_PATH SERVER_ADDR PORT CIPHER TUN_IF NET_VPN CLT_IP CLT_IP_SHORT SRV_IP_SHORT RUNDIR LOGF OPENSSL_ROOT OPENSSL_MODULES LD_LIBRARY_PATH

Проверки:
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
