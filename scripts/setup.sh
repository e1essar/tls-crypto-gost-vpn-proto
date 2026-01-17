#!/usr/bin/env bash
set -euo pipefail

WORKDIR="${WORKDIR:-$HOME/src/gost-setup}"
REPO_URL="${REPO_URL:-https://github.com/gost-engine/engine}"
REPO_DIR="$WORKDIR/engine"
LIBPROV_URL="${LIBPROV_URL:-https://github.com/provider-corner/libprov.git}"
LIBPROV_DIR="$REPO_DIR/libprov"
ENV_FILE="$HOME/.gost-env.sh"
SHELL_RC="${SHELL_RC:-$HOME/.bashrc}"

say() { printf "\n\033[1;32m[+] %s\033[0m\n" "$*"; }
warn() { printf "\n\033[1;33m[!] %s\033[0m\n" "$*"; }
die()  { printf "\n\033[1;31m[x] %s\033[0m\n" "$*"; exit 1; }

if [[ $EUID -eq 0 ]]; then
  warn "It is better to run this script as a non-root user. Continuing anyway."
fi

say "Detecting package manager and installing dependencies..."
PKG=""
if command -v apt-get >/dev/null 2>&1; then
  PKG="apt-get"
  sudo apt-get update -y
  sudo apt-get install -y \
    git build-essential cmake ninja-build automake autoconf libtool pkg-config \
    perl python3 python3-venv nasm \
    ca-certificates curl wget dos2unix unzip
elif command -v dnf >/dev/null 2>&1; then
  PKG="dnf"
  sudo dnf install -y \
    git gcc gcc-c++ make cmake ninja-build autoconf automake libtool \
    perl python3 python3-pip nasm \
    ca-certificates curl wget dos2unix unzip
elif command -v yum >/dev/null 2>&1; then
  PKG="yum"
  sudo yum install -y \
    git gcc gcc-c++ make cmake ninja-build autoconf automake libtool \
    perl python3 python3-pip nasm \
    ca-certificates curl wget dos2unix unzip
elif command -v pacman >/dev/null 2>&1; then
  PKG="pacman"
  sudo pacman -Sy --noconfirm \
    git base-devel cmake ninja autoconf automake libtool nasm \
    perl python dos2unix unzip
else
  die "No supported package manager found (apt/dnf/yum/pacman). Install dependencies manually."
fi

say "Preparing working directory: $WORKDIR"
mkdir -p "$WORKDIR"
if [[ -d "$REPO_DIR/.git" ]]; then
  say "Repository already exists. Updating..."
  git -C "$REPO_DIR" fetch --all --prune
  git -C "$REPO_DIR" reset --hard origin/HEAD
else
  say "Cloning repository: $REPO_URL"
  git clone --depth=1 "$REPO_URL" "$REPO_DIR"
fi

say "Checking/pulling libprov into $LIBPROV_DIR..."
if [[ -d "$LIBPROV_DIR/.git" ]]; then
  say "libprov already exists. Updating..."
  git -C "$LIBPROV_DIR" fetch --all --prune
  if [[ -n "${LIBPROV_BRANCH:-}" ]]; then
    git -C "$LIBPROV_DIR" checkout -q "$LIBPROV_BRANCH"
    git -C "$LIBPROV_DIR" reset --hard "origin/${LIBPROV_BRANCH}"
  else
    git -C "$LIBPROV_DIR" reset --hard origin/HEAD
  fi
else
  say "Cloning libprov into $LIBPROV_DIR..."
  if [[ -n "${LIBPROV_BRANCH:-}" ]]; then
    git clone --depth=1 --branch "$LIBPROV_BRANCH" "$LIBPROV_URL" "$LIBPROV_DIR"
  else
    git clone --depth=1 "$LIBPROV_URL" "$LIBPROV_DIR"
  fi
fi

say "Making patches/openssl-tls1.3.patch executable (if present)..."
if [[ -f "$REPO_DIR/patches/openssl-tls1.3.patch" ]]; then
  chmod +x "$REPO_DIR/patches/openssl-tls1.3.patch"
else
  warn "patches/openssl-tls1.3.patch not found — continuing."
fi

say "Normalizing line endings and making .github/before_script.sh and .github/script.sh executable..."
if [[ -f "$REPO_DIR/.github/before_script.sh" ]]; then
  dos2unix "$REPO_DIR/.github/before_script.sh" >/dev/null 2>&1 || true
  chmod +x "$REPO_DIR/.github/before_script.sh"
else
  warn ".github/before_script.sh not found."
fi
if [[ -f "$REPO_DIR/.github/script.sh" ]]; then
  dos2unix "$REPO_DIR/.github/script.sh" >/dev/null 2>&1 || true
  chmod +x "$REPO_DIR/.github/script.sh"
else
  warn ".github/script.sh not found."
fi

export PATCH_OPENSSL=1
export OPENSSL_BRANCH=openssl-3.4.2

pushd "$REPO_DIR" >/dev/null

if [[ -x ".github/before_script.sh" ]]; then
  say "Running .github/before_script.sh..."
  bash ".github/before_script.sh"
else
  warn "Skipping before_script.sh (missing or not executable)."
fi

if [[ -x ".github/script.sh" ]]; then
  say "Running .github/script.sh..."
  bash ".github/script.sh"
else
  warn "Skipping script.sh (missing or not executable)."
fi

popd >/dev/null

say "Locating where OpenSSL was installed..."
mapfile -t OPENSSL_CANDIDATES < <(
  {
    command -v openssl 2>/dev/null || true
    find "$REPO_DIR" "$WORKDIR" "$HOME" /usr/local /opt -type f -path "*/bin/openssl" 2>/dev/null || true
  } | awk 'NF' | awk '!x[$0]++'
)

OPENSSL_BIN=""
if [[ ${#OPENSSL_CANDIDATES[@]} -gt 0 ]]; then
  OPENSSL_BIN="$(for f in "${OPENSSL_CANDIDATES[@]}"; do printf "%s\t%s\n" "$(stat -c %Y "$f" 2>/dev/null || echo 0)" "$f"; done | sort -nr | awk 'NR==1{print $2}')"
fi
[[ -n "$OPENSSL_BIN" ]] || warn "Could not find a custom openssl binary, will try the system one."
if [[ -z "$OPENSSL_BIN" && -x "$(command -v openssl || true)" ]]; then
  OPENSSL_BIN="$(command -v openssl)"
fi
[[ -n "$OPENSSL_BIN" ]] || die "openssl not found. Check build logs."

OPENSSL_PREFIX="$(dirname "$(dirname "$OPENSSL_BIN")")"
say "OPENSSL_PREFIX = $OPENSSL_PREFIX"

say "Looking for the GOST provider module (*.so) in ossl-modules directories..."
MODULES_DIR_CANDIDATES=()
for p in "$OPENSSL_PREFIX/lib/ossl-modules" "$OPENSSL_PREFIX/lib64/ossl-modules" "/usr/local/lib/ossl-modules" "/usr/lib/ossl-modules"; do
  [[ -d "$p" ]] && MODULES_DIR_CANDIDATES+=("$p")
done
GOST_MODULE_PATH=""
for d in "${MODULES_DIR_CANDIDATES[@]}"; do
  f="$(find "$d" -maxdepth 1 -type f -name 'gost*.so' 2>/dev/null | head -n1 || true)"
  if [[ -n "$f" ]]; then
    GOST_MODULE_PATH="$f"
    break
  fi
done

if [[ -z "$GOST_MODULE_PATH" ]]; then
  warn "Could not find gost*.so in standard directories. It may be located next to the engine build."
  f="$(find "$REPO_DIR" -type f -name 'gost*.so' 2>/dev/null | head -n1 || true)"
  [[ -n "$f" ]] && GOST_MODULE_PATH="$f"
fi

if [[ -z "$GOST_MODULE_PATH" ]]; then
  warn "GOST provider module not found right now. If script.sh installs it later/in another location, you can ignore this warning."
else
  say "Found GOST module: $GOST_MODULE_PATH"
fi

if [[ -n "$GOST_MODULE_PATH" ]]; then
  OPENSSL_MODULES_DIR="$(dirname "$GOST_MODULE_PATH")"
else
  if [[ -d "$OPENSSL_PREFIX/lib/ossl-modules" ]]; then
    OPENSSL_MODULES_DIR="$OPENSSL_PREFIX/lib/ossl-modules"
  elif [[ -d "$OPENSSL_PREFIX/lib64/ossl-modules" ]]; then
    OPENSSL_MODULES_DIR="$OPENSSL_PREFIX/lib64/ossl-modules"
  else
    OPENSSL_MODULES_DIR="$OPENSSL_PREFIX/lib/ossl-modules"
  fi
fi
say "OPENSSL_MODULES (dir) = $OPENSSL_MODULES_DIR"

say "Writing environment variables to $ENV_FILE and sourcing them from $SHELL_RC..."

cat > "$ENV_FILE" <<EOF
export OPENSSL_ROOT="$OPENSSL_PREFIX"
export PATH="\$OPENSSL_ROOT/bin:\$PATH"
export OPENSSL_MODULES="$OPENSSL_MODULES_DIR"
if [ -d "\$OPENSSL_ROOT/lib" ]; then
  export LD_LIBRARY_PATH="\$OPENSSL_ROOT/lib:\${LD_LIBRARY_PATH:-}"
fi
if [ -d "\$OPENSSL_ROOT/lib64" ]; then
  export LD_LIBRARY_PATH="\$OPENSSL_ROOT/lib64:\${LD_LIBRARY_PATH:-}"
fi
export GOST_ENGINE_ROOT="$REPO_DIR"
EOF

grep -qF ". \"$ENV_FILE\"" "$SHELL_RC" 2>/dev/null || {
  echo "" >> "$SHELL_RC"
  echo "# Load GOST/OpenSSL env" >> "$SHELL_RC"
  echo ". \"$ENV_FILE\"" >> "$SHELL_RC"
}

if [[ -f "$ENV_FILE" ]]; then
  say "Sourcing $ENV_FILE into the current session..."
  . "$ENV_FILE"
else
  warn "$ENV_FILE not found, skipping."
fi

say "Configuring openssl.cnf to auto-load the gost provider..."

mkdir -p "$HOME/opt"
USER_OPENSSL_CNF="$HOME/opt/openssl.cnf"

CANDIDATE_CONFS=(
  "$OPENSSL_ROOT/ssl/openssl.cnf"
  "$OPENSSL_ROOT/etc/ssl/openssl.cnf"
  "/etc/ssl/openssl.cnf"
)
OPENSSL_CNF=""
for c in "${CANDIDATE_CONFS[@]}"; do
  [[ -f "$c" ]] && { OPENSSL_CNF="$c"; break; }
done
[[ -n "$OPENSSL_CNF" ]] || { echo "System openssl.cnf not found"; exit 1; }

cat > "$USER_OPENSSL_CNF" <<EOF
.include $OPENSSL_CNF

openssl_conf = openssl_init

[openssl_init]
providers = provider_sect

[provider_sect]
default = default_sect
gostprov = gost_sect

[default_sect]
activate = 1

[gost_sect]
activate = 1
EOF

echo "export OPENSSL_CONF=\"$USER_OPENSSL_CNF\"" >> "$HOME/.gost-env.sh"

say "Done. To verify in a NEW terminal session run:"
cat <<'CHECK'
  . "$HOME/.gost-env.sh"
  openssl ciphers -tls1_3 -ciphersuites TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_S -
CHECK

say "Success. If you get an error, check OPENSSL_ROOT/OPENSSL_MODULES and that gost*.so exists."
