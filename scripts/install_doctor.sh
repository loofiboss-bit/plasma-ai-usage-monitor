#!/usr/bin/env bash
set -euo pipefail

AUTO_INSTALL=false
STRICT=false

usage() {
  cat <<'USAGE'
install_doctor.sh - preflight checks for building/installing Plasma AI Usage Monitor

Usage:
  scripts/install_doctor.sh [--install-missing] [--strict]

Options:
  --install-missing  On Fedora, install missing build dependencies with dnf.
  --strict           Exit non-zero if any warning-level runtime tool is missing.
  -h, --help         Show this help.
USAGE
}

while (($# > 0)); do
  case "$1" in
    --install-missing)
      AUTO_INSTALL=true
      ;;
    --strict)
      STRICT=true
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
OS_PRETTY="Unknown"
if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  source /etc/os-release
  OS_ID="${ID:-unknown}"
  OS_PRETTY="${PRETTY_NAME:-$OS_ID}"
fi

check_cmd() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "ok"
  else
    echo "missing"
  fi
}

required_cmds=(cmake g++ kpackagetool6 protoc)
warning_cmds=(plasmashell plasmawindowed qmllint kwallet-query secret-tool aws)

missing_required=()
missing_warning=()
for cmd in "${required_cmds[@]}"; do
  if [[ "$(check_cmd "$cmd")" == "missing" ]]; then
    missing_required+=("$cmd")
  fi
done
for cmd in "${warning_cmds[@]}"; do
  if [[ "$(check_cmd "$cmd")" == "missing" ]]; then
    missing_warning+=("$cmd")
  fi
done

fedora_packages=(
  cmake
  extra-cmake-modules
  gcc-c++
  qt6-qtbase
  qt6-qtbase-devel
  qt6-qtdeclarative-devel
  libplasma-devel
  kf6-kwallet-devel
  kf6-ki18n-devel
  kf6-knotifications-devel
  kf6-kcoreaddons-devel
  openssl-devel
  protobuf-compiler
  protobuf-devel
  libsecret-devel
)

missing_fedora_packages=()
if [[ "$OS_ID" == "fedora" ]] && command -v rpm >/dev/null 2>&1; then
  for pkg in "${fedora_packages[@]}"; do
    if ! rpm -q "$pkg" >/dev/null 2>&1; then
      missing_fedora_packages+=("$pkg")
    fi
  done
fi

sqlite_driver_ok=false
if [[ -f /usr/lib64/qt6/plugins/sqldrivers/libqsqlite.so ]] || [[ -f /usr/lib/qt6/plugins/sqldrivers/libqsqlite.so ]] || [[ -f /usr/lib/x86_64-linux-gnu/qt6/plugins/sqldrivers/libqsqlite.so ]]; then
  sqlite_driver_ok=true
fi

PLASMOID_ID="com.github.loofi.aiusagemonitor"
USER_PLASMOID_DIR="${HOME}/.local/share/plasma/plasmoids/${PLASMOID_ID}"
SYSTEM_PLASMOID_DIR="/usr/share/plasma/plasmoids/${PLASMOID_ID}"
QML_RELATIVE_DIR="com/github/loofi/aiusagemonitor"

find_qml_module_dir() {
  local candidates=(
    "${HOME}/.local/lib64/qt6/qml/${QML_RELATIVE_DIR}"
    "${HOME}/.local/lib/qt6/qml/${QML_RELATIVE_DIR}"
    "${HOME}/.local/lib/qml/${QML_RELATIVE_DIR}"
    "/usr/lib64/qt6/qml/${QML_RELATIVE_DIR}"
    "/usr/lib/qt6/qml/${QML_RELATIVE_DIR}"
    "/usr/lib/x86_64-linux-gnu/qt6/qml/${QML_RELATIVE_DIR}"
    "/usr/lib/qml/${QML_RELATIVE_DIR}"
  )

  local dir
  for dir in "${candidates[@]}"; do
    if [[ -f "${dir}/qmldir" ]]; then
      echo "$dir"
      return 0
    fi
  done

  return 1
}

find_plugin_library() {
  local module_dir="$1"
  local candidates=(
    "${module_dir}/libaiusagemonitorplugin.so"
    "${module_dir}/aiusagemonitorplugin.so"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done

  local discovered
  discovered="$(find "$module_dir" -maxdepth 1 -type f -name '*aiusage*plugin*.so' 2>/dev/null | head -1 || true)"
  if [[ -n "$discovered" ]]; then
    echo "$discovered"
    return 0
  fi

  return 1
}

user_plasmoid_present=false
system_plasmoid_present=false
[[ -d "$USER_PLASMOID_DIR" ]] && user_plasmoid_present=true
[[ -d "$SYSTEM_PLASMOID_DIR" ]] && system_plasmoid_present=true

qml_module_dir="$(find_qml_module_dir || true)"
qml_import_ready=false
compiled_plugin_ok=false
compiled_plugin_path=""
plugin_ldd_ok=true
plugin_missing_libs=()
if [[ -n "$qml_module_dir" ]]; then
  qml_import_ready=true
  compiled_plugin_path="$(find_plugin_library "$qml_module_dir" || true)"
  if [[ -n "$compiled_plugin_path" ]]; then
    compiled_plugin_ok=true
    # Check for missing shared libraries
    if command -v ldd >/dev/null 2>&1; then
      if ldd "$compiled_plugin_path" | grep -q "not found"; then
        plugin_ldd_ok=false
        while read -r line; do
          plugin_missing_libs+=("$(echo "$line" | awk '{print $1}')")
        done < <(ldd "$compiled_plugin_path" | grep "not found")
      fi
    fi
  fi
fi

# Browser Profile Detection
browser_profiles_found=()
browser_paths=(
  "Firefox:~/.mozilla/firefox"
  "Firefox (Flatpak):~/.var/app/org.mozilla.firefox/.mozilla/firefox"
  "Chrome:~/.config/google-chrome"
  "Chromium:~/.config/chromium"
  "Chromium (Flatpak):~/.var/app/org.chromium.Chromium/config/chromium"
  "Brave:~/.config/BraveSoftware/Brave-Browser"
)

for pair in "${browser_paths[@]}"; do
  name="${pair%%:*}"
  path="${pair#*:}"
  eval path="$path"
  if [[ -d "$path" ]]; then
    browser_profiles_found+=("$name")
  fi
done

# AWS / Bedrock checks
aws_env_ok=false
if [[ -n "${AWS_ACCESS_KEY_ID:-}" && -n "${AWS_SECRET_ACCESS_KEY:-}" ]]; then
  aws_env_ok=true
fi
aws_config_present=false
if [[ -f "${HOME}/.aws/credentials" || -f "${HOME}/.aws/config" ]]; then
  aws_config_present=true
fi

# KWallet check
kwallet_usable=false
if command -v kwallet-query >/dev/null 2>&1; then
  if timeout 2s kwallet-query -l kdewallet >/dev/null 2>&1 || timeout 2s kwallet-query -l LocalWallet >/dev/null 2>&1; then
    kwallet_usable=true
  fi
fi

echo "=== Plasma AI Usage Monitor - Install Doctor ==="
echo "OS: $OS_PRETTY"
echo

echo "Required command checks:"
for cmd in "${required_cmds[@]}"; do
  state="$(check_cmd "$cmd")"
  printf '  - %-14s %s\n' "$cmd" "$state"
done

echo

echo "Runtime tool checks (recommended):"
for cmd in "${warning_cmds[@]}"; do
  state="$(check_cmd "$cmd")"
  printf '  - %-14s %s\n' "$cmd" "$state"
done

echo
if [[ "$OS_ID" == "fedora" ]] && command -v rpm >/dev/null 2>&1; then
  echo "Fedora package checks:"
  for pkg in "${fedora_packages[@]}"; do
    if rpm -q "$pkg" >/dev/null 2>&1; then
      printf '  - %-28s installed\n' "$pkg"
    else
      printf '  - %-28s missing\n' "$pkg"
    fi
  done
  echo
fi

echo "SQLite driver: $([[ "$sqlite_driver_ok" == true ]] && echo "ok" || echo "warning: not found")"
echo "Plasmoid package (user-local): $([[ "$user_plasmoid_present" == true ]] && echo "present" || echo "not found")"
echo "Plasmoid package (system): $([[ "$system_plasmoid_present" == true ]] && echo "present" || echo "not found")"
echo "QML import readiness: $([[ "$qml_import_ready" == true ]] && echo "ok (${qml_module_dir}/qmldir)" || echo "warning: qmldir not found")"
echo "Compiled plugin: $([[ "$compiled_plugin_ok" == true ]] && echo "ok (${compiled_plugin_path})" || echo "warning: shared library not found")"
if [[ "$compiled_plugin_ok" == true && "$plugin_ldd_ok" == false ]]; then
  echo "  ! WARNING: Plugin has missing shared library dependencies: ${plugin_missing_libs[*]}"
fi

echo
echo "Browser Sync Readiness:"
if [[ ${#browser_profiles_found[@]} -gt 0 ]]; then
  echo "  - Profiles found: ${browser_profiles_found[*]}"
else
  echo "  - warning: No browser profiles detected in standard paths."
fi
echo "  - KWallet usability: $([[ "$kwallet_usable" == true ]] && echo "ok" || echo "warning: KWallet not responsive or no wallet found")"

echo
echo "AWS Bedrock Readiness:"
echo "  - Environment vars: $([[ "$aws_env_ok" == true ]] && echo "present" || echo "missing")"
echo "  - AWS Config file:  $([[ "$aws_config_present" == true ]] && echo "found" || echo "not found")"

do_install=false
if [[ "$AUTO_INSTALL" == true ]] && [[ "$OS_ID" == "fedora" ]]; then
  if [[ ${#missing_fedora_packages[@]} -gt 0 ]]; then
    do_install=true
  fi
fi

if [[ "$do_install" == true ]]; then
  echo
  echo "Installing missing Fedora dependencies..."
  sudo dnf install -y "${missing_fedora_packages[@]}"
  echo "Dependency install finished. Re-run doctor to confirm."
fi

echo
has_blocking=false
if [[ ${#missing_required[@]} -gt 0 ]]; then
  has_blocking=true
  echo "Blocking issues:"
  for item in "${missing_required[@]}"; do
    echo "  - Missing required command: $item"
  done
fi

if [[ "$OS_ID" == "fedora" ]] && [[ ${#missing_fedora_packages[@]} -gt 0 ]] && [[ "$AUTO_INSTALL" != true ]]; then
  has_blocking=true
  echo "  - Missing Fedora build packages. Install with:"
  echo "    sudo dnf install ${missing_fedora_packages[*]}"
fi

if [[ "$sqlite_driver_ok" != true ]]; then
  echo "Warning: Qt SQLite driver not detected. History charts may not work."
  if [[ "$OS_ID" == "fedora" ]]; then
    echo "    Fix: sudo dnf install qt6-qtbase-sqlite"
  fi
fi

if [[ "$user_plasmoid_present" == true && "$system_plasmoid_present" == true ]]; then
  echo "Warning: both user-local and system plasmoid packages are installed. User-local shadows system."
  echo "    Fix: kpackagetool6 --type Plasma/Applet --remove com.github.loofi.aiusagemonitor"
fi

if [[ "$user_plasmoid_present" == true && "$compiled_plugin_ok" != true ]]; then
  echo "Warning: user-local plasmoid found, but compiled plugin NOT found in Qt import paths."
  echo "    This usually means the C++ plugin was never installed or is in a non-standard path."
fi

if [[ "$qml_import_ready" != true ]]; then
  echo "Warning: the installed QML module qmldir was not found in common Qt6 import paths."
  echo "    Check if QML2_IMPORT_PATH or QML_IMPORT_PATH needs to include your install prefix."
fi

if [[ ${#missing_warning[@]} -gt 0 ]]; then
  echo "Warnings:"
  for item in "${missing_warning[@]}"; do
    echo "  - Missing runtime command: $item"
    if [[ "$item" == "kwallet-query" && "$OS_ID" == "fedora" ]]; then
       echo "    Fix: sudo dnf install kwallet-query"
    elif [[ "$item" == "secret-tool" && "$OS_ID" == "fedora" ]]; then
       echo "    Fix: sudo dnf install libsecret"
    fi
  done
fi

if [[ "$has_blocking" == true ]]; then
  echo
  echo "Doctor result: FAIL"
  exit 1
fi

if [[ "$STRICT" == true ]] && [[ ${#missing_warning[@]} -gt 0 ]]; then
  echo
  echo "Doctor result: FAIL (strict mode)"
  exit 1
fi

echo "Doctor result: PASS"
