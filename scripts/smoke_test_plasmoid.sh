#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-build/ci}"
PREFIX="$(mktemp -d)"
LOG_DIR="${ROOT_DIR}/.demo-output/plasmoid-smoke"
mkdir -p "$LOG_DIR"
trap 'rm -rf "$PREFIX"' EXIT

cmake --install "$BUILD_DIR" --prefix "$PREFIX"
qml_path="$(find "$PREFIX" -type d -path '*/qt6/qml' -print -quit)"
if [[ -z "$qml_path" ]]; then
  echo "Compiled QML plugin path not found under $PREFIX" >&2
  exit 1
fi

for view in overview history analyst; do
  log_file="${LOG_DIR}/${view}.log"
  set +e
  XDG_DATA_DIRS="${PREFIX}/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
  QML2_IMPORT_PATH="$qml_path" \
  PLASMA_AI_MONITOR_DEMO=1 \
  PLASMA_AI_MONITOR_SMOKE_VIEW="$view" \
  timeout 6s plasmawindowed com.github.loofi.aiusagemonitor >"$log_file" 2>&1
  status=$?
  set -e
  if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    echo "Plasmoid smoke failed for $view (exit $status)" >&2
    cat "$log_file" >&2
    exit 1
  fi
  if grep -Eiq 'QQmlApplicationEngine failed|Error loading QML|Type .* unavailable|is not a type|ReferenceError|Binding loop|Cannot assign|Required property .* was not initialized' "$log_file"; then
    echo "Critical QML diagnostic in $view" >&2
    cat "$log_file" >&2
    exit 1
  fi
done

echo "Full plasmoid smoke passed: Overview, History, Analyst"
