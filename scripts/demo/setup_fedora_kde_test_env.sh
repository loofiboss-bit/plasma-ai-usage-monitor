#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
FEDORA_VERSION="44"
INSTALL_MISSING=false
PREPARE_WIDGET=false

usage() {
  cat <<'USAGE'
setup_fedora_kde_test_env.sh - prepare a Fedora KDE guest for live widget validation

Usage:
  scripts/demo/setup_fedora_kde_test_env.sh [--fedora 44] [--install-missing] [--prepare-widget]

Options:
  --fedora <version>  Expected Fedora version. Defaults to 44.
  --install-missing   Install missing Fedora packages with dnf.
  --prepare-widget    Build debug, install the C++ plugin, install the user plasmoid, and reload Plasma.
  -h, --help          Show this help.
USAGE
}

while (($# > 0)); do
  case "$1" in
    --fedora)
      shift
      FEDORA_VERSION="${1:-}"
      ;;
    --install-missing)
      INSTALL_MISSING=true
      ;;
    --prepare-widget)
      PREPARE_WIDGET=true
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
  shift
done

OS_ID="unknown"
OS_VERSION="unknown"
if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  source /etc/os-release
  OS_ID="${ID:-unknown}"
  OS_VERSION="${VERSION_ID:-unknown}"
fi

if [[ "$OS_ID" != "fedora" || "$OS_VERSION" != "$FEDORA_VERSION" ]]; then
  echo "Expected Fedora ${FEDORA_VERSION}; detected ${OS_ID} ${OS_VERSION}."
  echo "This helper is intended for a real Fedora KDE ${FEDORA_VERSION} session."
  exit 1
fi

missing=()
for pkg in \
  cmake extra-cmake-modules gcc-c++ just python3 python3-pip python3-venv firefox \
  qt6-qtbase qt6-qtbase-devel qt6-qtdeclarative-devel libplasma-devel \
  kf6-kwallet-devel kf6-ki18n-devel kf6-knotifications-devel kf6-kcoreaddons-devel \
  openssl-devel protobuf-compiler protobuf-devel libsecret-devel appstream rpmlint plasma-workspace; do
  if ! rpm -q "$pkg" >/dev/null 2>&1; then
    missing+=("$pkg")
  fi
done

if [[ "${#missing[@]}" -gt 0 ]]; then
  echo "Missing Fedora packages: ${missing[*]}"
  if [[ "$INSTALL_MISSING" == true ]]; then
    sudo dnf install -y "${missing[@]}"
  else
    echo "Next: rerun with --install-missing or run:"
    echo "  sudo dnf install ${missing[*]}"
    exit 1
  fi
else
  echo "Fedora KDE package prerequisites are installed."
fi

bash "$ROOT_DIR/scripts/demo/bootstrap_demo_env.sh"

if [[ "$PREPARE_WIDGET" == true ]]; then
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
  cmake --build "$ROOT_DIR/build" --parallel
  sudo cmake --install "$ROOT_DIR/build"
  bash "$ROOT_DIR/scripts/install_local_plasmoid.sh"
  bash "$ROOT_DIR/scripts/reload_plasma.sh"
fi

cat <<EOF
Fedora KDE ${FEDORA_VERSION} demo environment ready.

Next steps:
  source .venv/bin/activate
  python scripts/demo/mock_ai_usage_server.py
  PLASMA_AI_MONITOR_DEMO=1 plasmawindowed com.github.loofi.aiusagemonitor

If port 8080 is occupied:
  python scripts/demo/mock_ai_usage_server.py --port 18080
  PLASMA_AI_MONITOR_DEMO=1 PLASMA_AI_MONITOR_DEMO_BASE_URL=http://127.0.0.1:18080 plasmawindowed com.github.loofi.aiusagemonitor
EOF
