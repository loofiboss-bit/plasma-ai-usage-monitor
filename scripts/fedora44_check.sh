#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

failures=0
warnings=0

ok() {
  printf 'OK   %s\n' "$1"
}

warn() {
  warnings=$((warnings + 1))
  printf 'WARN %s\n' "$1"
  [[ $# -gt 1 ]] && printf '     Next: %s\n' "$2"
}

fail() {
  failures=$((failures + 1))
  printf 'FAIL %s\n' "$1"
  [[ $# -gt 1 ]] && printf '     Next: %s\n' "$2"
}

check_cmd() {
  local cmd="$1"
  local package_hint="$2"
  if command -v "$cmd" >/dev/null 2>&1; then
    ok "$cmd found: $(command -v "$cmd")"
  else
    fail "$cmd is missing" "sudo dnf install ${package_hint}"
  fi
}

check_rpm() {
  local pkg="$1"
  if command -v rpm >/dev/null 2>&1 && rpm -q "$pkg" >/dev/null 2>&1; then
    ok "RPM package installed: $pkg"
  else
    fail "RPM package missing: $pkg" "sudo dnf install $pkg"
  fi
}

echo "=== Plasma AI Usage Monitor Fedora 44 Release Check ==="
echo

OS_ID="unknown"
OS_VERSION="unknown"
OS_PRETTY="unknown"
if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  source /etc/os-release
  OS_ID="${ID:-unknown}"
  OS_VERSION="${VERSION_ID:-unknown}"
  OS_PRETTY="${PRETTY_NAME:-unknown}"
fi

if [[ "$OS_ID" == "fedora" && "$OS_VERSION" == "44" ]]; then
  ok "Fedora version is 44 (${OS_PRETTY})"
elif [[ "$OS_ID" == "fedora" ]]; then
  fail "Fedora version is ${OS_VERSION}, expected 44" "Run this gate inside Fedora KDE 44 or a fedora:44 validation container."
else
  fail "OS is ${OS_PRETTY}, expected Fedora 44" "Run this gate inside Fedora KDE 44 or a fedora:44 validation container."
fi

if command -v plasmashell >/dev/null 2>&1; then
  plasma_version="$(plasmashell --version 2>/dev/null || true)"
  if [[ -n "$plasma_version" ]]; then
    ok "Plasma available: ${plasma_version}"
  else
    warn "plasmashell exists but version could not be read" "Run from a real Plasma session for live UI validation."
  fi
else
  warn "plasmashell is not installed or not on PATH" "sudo dnf install plasma-workspace; real UI validation still requires a Plasma session."
fi

if command -v qmake6 >/dev/null 2>&1; then
  ok "Qt6 available: $(qmake6 -query QT_VERSION 2>/dev/null || echo unknown)"
else
  check_rpm qt6-qtbase-devel
fi

for pkg in \
  cmake extra-cmake-modules gcc-c++ qt6-qtbase qt6-qtbase-devel \
  qt6-qtdeclarative-devel libplasma-devel kf6-kwallet-devel \
  kf6-ki18n-devel kf6-knotifications-devel kf6-kcoreaddons-devel \
  openssl-devel appstream rpmlint; do
  check_rpm "$pkg"
done

if command -v rpm >/dev/null 2>&1 && rpm -q libsecret-devel >/dev/null 2>&1; then
  ok "RPM package installed: libsecret-devel"
else
  warn "Optional Browser Sync development package missing: libsecret-devel" "sudo dnf install libsecret-devel if you are changing libsecret integration code."
fi

echo
echo "Runtime tools:"
check_cmd kpackagetool6 plasma-workspace
check_cmd plasmawindowed plasma-workspace
check_cmd kwallet-query kwallet-query
check_cmd secret-tool libsecret

echo
echo "QML/plugin paths:"
module_rel="com/github/loofi/aiusagemonitor"
module_candidates=(
  "${HOME}/.local/lib64/qt6/qml/${module_rel}"
  "${HOME}/.local/lib/qt6/qml/${module_rel}"
  "${HOME}/.local/lib/qml/${module_rel}"
  "/usr/lib64/qt6/qml/${module_rel}"
  "/usr/lib/qt6/qml/${module_rel}"
)
module_found=false
for dir in "${module_candidates[@]}"; do
  if [[ -f "${dir}/qmldir" ]]; then
    ok "QML module found: ${dir}"
    module_found=true
  fi
done
if [[ "$module_found" != true ]]; then
  warn "Installed QML module not found in common paths" "Build/install with: just build-debug && sudo cmake --install build"
fi

user_plasmoid="${HOME}/.local/share/plasma/plasmoids/com.github.loofi.aiusagemonitor"
system_plasmoid="/usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor"
if [[ -d "$user_plasmoid" && -d "$system_plasmoid" ]]; then
  warn "User-local plasmoid shadows the system install" "Run: just clean-local or kpackagetool6 --type Plasma/Applet --remove com.github.loofi.aiusagemonitor before live release smoke."
elif [[ -d "$user_plasmoid" ]]; then
  warn "User-local plasmoid is installed" "For release validation, prefer a clean system install or verify with just versions."
elif [[ -d "$system_plasmoid" ]]; then
  ok "System plasmoid install found"
else
  warn "No installed plasmoid found" "Install for live validation with: sudo cmake --install build"
fi

if [[ -n "${QML2_IMPORT_PATH:-}${QML_IMPORT_PATH:-}" ]]; then
  warn "Custom QML import path is set" "Unset QML2_IMPORT_PATH/QML_IMPORT_PATH for release validation unless intentionally isolating a repo build."
else
  ok "No custom QML import path detected"
fi

echo
echo "Release checks:"
if bash scripts/check_version_consistency.sh; then ok "Version consistency"; else fail "Version consistency check failed" "Run: bash scripts/check_version_consistency.sh"; fi
if bash scripts/check_no_hardcoded_versions.sh; then ok "No stale QML versions"; else fail "Hardcoded stale QML versions found" "Remove hardcoded semantic versions from QML."; fi
if python3 scripts/check_qml_registered_types.py; then ok "QML registered types"; else fail "QML registered type check failed" "Align plugin/CMakeLists.txt with qmlRegisterType calls."; fi
if python3 scripts/check_provider_catalog.py; then ok "Provider Catalog v5"; else fail "Provider Catalog v5 invalid" "Update package/contents/catalog/providers-v4.json."; fi
if python3 scripts/check_subscription_catalog.py; then ok "Subscription Catalog v1"; else fail "Subscription Catalog v1 invalid" "Update package/contents/catalog/subscriptions-v1.json."; fi
if python3 scripts/check_no_hardcoded_pricing.py; then ok "No hardcoded subscription pricing"; else fail "Hardcoded subscription pricing found" "Move subscription prices and quotas into subscriptions-v1.json."; fi
if python3 scripts/check_config_portability.py; then ok "Config portability schema"; else fail "Config portability schema invalid" "Keep configDiagnostics.qml portableConfigKeys aligned with package/contents/config/main.xml."; fi
if command -v appstreamcli >/dev/null 2>&1 && appstreamcli validate com.github.loofi.aiusagemonitor.metainfo.xml; then ok "AppStream metadata"; else fail "AppStream validation failed" "Install appstream and fix com.github.loofi.aiusagemonitor.metainfo.xml."; fi
if command -v rpmlint >/dev/null 2>&1 && rpmlint plasma-ai-usage-monitor.spec; then ok "rpmlint spec validation"; else fail "rpmlint failed" "Install rpmlint and fix plasma-ai-usage-monitor.spec findings."; fi
if bash scripts/package_source_tarball.sh --check; then ok "Source tarball check"; else fail "Source tarball check failed" "Fix scripts/package_source_tarball.sh --check findings."; fi
if bash scripts/package_plasmoid.sh --check; then ok "Plasmoid package check"; else fail "Plasmoid package check failed" "Fix scripts/package_plasmoid.sh --check findings."; fi
if python3 scripts/check_package_payload.py; then ok "Package payload"; else fail "Package payload failed" "Run package scripts, then re-run python3 scripts/check_package_payload.py."; fi

echo
if [[ "$failures" -gt 0 ]]; then
  echo "Fedora 44 check: FAIL (${failures} failures, ${warnings} warnings)"
  exit 1
fi

echo "Fedora 44 check: PASS (${warnings} warnings)"
