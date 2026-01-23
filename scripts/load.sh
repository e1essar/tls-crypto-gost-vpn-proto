#!/usr/bin/env bash
set -euo pipefail

say() { printf "\n\033[1;32m[+] %s\033[0m\n" "$*"; }
warn() { printf "\n\033[1;33m[!] %s\033[0m\n" "$*"; }

source "$HOME/.gost-env.sh"

say "Проверяю поддержку шифросьюта TLS 1.3 GOST..."

CHECK_OUTPUT="$(openssl ciphers -tls1_3 -ciphersuites TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_S 2>&1 || true)"

if echo "$CHECK_OUTPUT" | grep -q "no cipher match"; then
  warn "❌ GOST TLS 1.3 не поддерживается или модуль не загружен."
  echo "$CHECK_OUTPUT" 
else
  say "✅ OpenSSL поддерживает GOST TLS 1.3: шифросьюит доступен."
  #echo "$CHECK_OUTPUT"
fi

