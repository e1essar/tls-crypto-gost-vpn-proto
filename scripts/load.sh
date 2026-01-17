#!/usr/bin/env bash
set -euo pipefail

say() { printf "\n\033[1;32m[+] %s\033[0m\n" "$*"; }
warn() { printf "\n\033[1;33m[!] %s\033[0m\n" "$*"; }

ENV_FILE="${ENV_FILE:-$HOME/.gost-env.sh}"
if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
else
  warn "Env file not found: $ENV_FILE"
  warn "Run scripts/setup.sh first, or export OPENSSL_ROOT/OPENSSL_MODULES manually."
fi

say "Checking ciphersuite support TLS 1.3 GOST..."

CHECK_OUTPUT="$(openssl ciphers -tls1_3 -ciphersuites TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_S 2>&1 || true)"

if echo "$CHECK_OUTPUT" | grep -q "no cipher match"; then
  warn "GOST TLS 1.3 is not supported or didn't loaded"
  echo "$CHECK_OUTPUT" 
else
  say "OpenSSL supports GOST TLS 1.3: ciphersuite enabled"
  #echo "$CHECK_OUTPUT"
fi

