#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build/debug}"
OUTPUT_DIR="${2:-${ROOT_DIR}/assets/screenshots}"
SESSION_ROOT="$(mktemp -d)"
PREFIX="${SESSION_ROOT}/prefix"
CONFIG_HOME="${SESSION_ROOT}/config"
CACHE_HOME="${SESSION_ROOT}/cache"
KWIN_SCRIPT_NAME="ai-usage-monitor-v14-capture"
WINDOW_PID=""
SERVER_PID=""
KWIN_SCRIPT_LOADED=0

cleanup() {
  if [[ -n "$WINDOW_PID" ]]; then
    kill "$WINDOW_PID" 2>/dev/null || true
    wait "$WINDOW_PID" 2>/dev/null || true
  fi
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
  if [[ "$KWIN_SCRIPT_LOADED" -eq 1 ]]; then
    busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
      unloadScript s "$KWIN_SCRIPT_NAME" >/dev/null 2>&1 || true
  fi
  if [[ -d "$SESSION_ROOT" && "$SESSION_ROOT" == /tmp/* ]]; then
    find "$SESSION_ROOT" -depth -delete
  fi
}
trap cleanup EXIT

for command in cmake plasmawindowed spectacle busctl identify jq sha256sum kwriteconfig6; do
  command -v "$command" >/dev/null || {
    echo "Missing capture dependency: $command" >&2
    exit 1
  }
done

[[ -d "$BUILD_DIR" ]] || {
  echo "Build directory not found: $BUILD_DIR" >&2
  exit 1
}

mkdir -p "$OUTPUT_DIR" "$CONFIG_HOME" "$CACHE_HOME"
cmake --install "$BUILD_DIR" --prefix "$PREFIX" >/dev/null
QML_PATH="$(find "$PREFIX" -type d -path '*/qt6/qml' -print -quit)"
[[ -n "$QML_PATH" ]] || {
  echo "Installed QML module was not found under $PREFIX" >&2
  exit 1
}

python3 "$ROOT_DIR/scripts/demo/mock_ai_usage_server.py" \
  >"${SESSION_ROOT}/mock-server.log" 2>&1 &
SERVER_PID=$!
sleep 1
kill -0 "$SERVER_PID" 2>/dev/null || {
  echo "Demo server did not start" >&2
  sed -n '1,120p' "${SESSION_ROOT}/mock-server.log" >&2
  exit 1
}

busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting loadScript \
  ss "$ROOT_DIR/scripts/demo/kwin_capture_layout.js" "$KWIN_SCRIPT_NAME" >/dev/null
busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting start >/dev/null
KWIN_SCRIPT_LOADED=1

focus_capture_window() {
  local match_query="$1"
  local expected_pid="$2"
  local match_output
  local match_id
  local candidate_id
  local candidate_info

  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
    unloadScript s "$KWIN_SCRIPT_NAME" >/dev/null 2>&1 || true
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting loadScript \
    ss "$ROOT_DIR/scripts/demo/kwin_capture_layout.js" "$KWIN_SCRIPT_NAME" >/dev/null
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting start >/dev/null
  sleep 1

  match_output="$(busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
    Match s "$match_query")"
  match_id=""
  while IFS= read -r candidate_id; do
    candidate_info="$(busctl --user call org.kde.KWin /KWin org.kde.KWin \
      getWindowInfo s "${candidate_id#0_}")"
    rg -Fq "\"caption\" s \"$match_query\"" <<<"$candidate_info" || continue
    if [[ "$match_query" == "AI Usage Monitor" ]] \
        && ! rg -q "\"pid\" [a-z]+ $expected_pid([[:space:]]|$)" <<<"$candidate_info"; then
      continue
    fi
    match_id="$candidate_id"
    break
  done < <(rg -o '"0_\{[^"]+\}"' <<<"$match_output" | tr -d '"')
  [[ -n "$match_id" ]] || {
    echo "KWin could not identify the capture window: $match_query" >&2
    return 1
  }
  busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
    Run ss "$match_id" "" >/dev/null
  sleep 1
}

capture_view() {
  local view="$1"
  local filename="$2"
  local settle_seconds="$3"
  local temporary="${OUTPUT_DIR}/.${filename}.capture"
  local dimensions
  local width
  local height
  local view_config_home="${CONFIG_HOME}/${view}"
  local view_cache_home="${CACHE_HOME}/${view}"
  local match_query="AI Usage Monitor"

  mkdir -p "$view_config_home" "$view_cache_home"
  cp /usr/share/color-schemes/BreezeDark.colors "$view_config_home/kdeglobals"
  XDG_CONFIG_HOME="$view_config_home" kwriteconfig6 \
    --file kdeglobals --group General --key ColorScheme BreezeDark
  XDG_CONFIG_HOME="$view_config_home" kwriteconfig6 \
    --file plasmarc --group Theme --key name breeze-dark

  XDG_DATA_HOME="$PREFIX/share" \
  XDG_DATA_DIRS="$PREFIX/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
  XDG_CONFIG_HOME="$view_config_home" \
  XDG_CACHE_HOME="$view_cache_home" \
  QML_IMPORT_PATH="$QML_PATH" \
  QML2_IMPORT_PATH="$QML_PATH" \
  KDE_COLOR_SCHEME_PATH="/usr/share/color-schemes/BreezeDark.colors" \
  PLASMA_AI_MONITOR_DEMO=1 \
  PLASMA_AI_MONITOR_SMOKE_VIEW="$view" \
  plasmawindowed com.github.loofi.aiusagemonitor \
    >"${SESSION_ROOT}/${view}.log" 2>&1 &
  WINDOW_PID=$!

  sleep "$settle_seconds"
  kill -0 "$WINDOW_PID" 2>/dev/null || {
    echo "Plasma window exited before capture: $view" >&2
    sed -n '1,160p' "${SESSION_ROOT}/${view}.log" >&2
    exit 1
  }

  # Re-run the KWin script after the view has settled. This both normalizes the
  # geometry and re-activates the exact plasmawindowed instance immediately
  # before Spectacle asks KWin for the active window.
  if [[ "$view" == "settings" ]]; then
    match_query="AI Usage Monitor Settings"
  fi
  focus_capture_window "$match_query" "$WINDOW_PID"
  spectacle --activewindow --background --nonotify --output "$temporary"
  for _ in {1..20}; do
    [[ -s "$temporary" ]] && break
    sleep 0.25
  done
  [[ -s "$temporary" ]] || {
    echo "Spectacle did not create $filename" >&2
    exit 1
  }

  dimensions="$(identify -format '%w %h' "$temporary")"
  width="${dimensions%% *}"
  height="${dimensions##* }"
  if (( width < 1200 || width > 1800 || height < 700 || height > 980 )); then
    echo "Capture has unexpected geometry: $filename (${width}x${height})" >&2
    unlink "$temporary"
    exit 1
  fi

  mv "$temporary" "${OUTPUT_DIR}/${filename}"
  kill "$WINDOW_PID" 2>/dev/null || true
  wait "$WINDOW_PID" 2>/dev/null || true
  WINDOW_PID=""
  sleep 2
  echo "Captured $filename (${width}x${height})"
}

capture_view onboarding-source guided-first-success.png 5
capture_view onboarding-result verified-success.png 5
capture_view overview main-window.png 7
capture_view provider-detail provider-intelligence.png 5
capture_view settings settings-view.png 6
capture_view history history-view.png 5
capture_view analyst analyst-view.png 5
capture_view panel panel-view.png 5

SESSION_ID="$(cat /proc/sys/kernel/random/uuid)"
FIXTURE_SHA="$(sha256sum "$ROOT_DIR/scripts/demo/showcase_preset.json" | cut -d' ' -f1)"
ASSETS_JSON="$(
  for filename in \
    guided-first-success.png verified-success.png main-window.png \
    provider-intelligence.png settings-view.png history-view.png \
    analyst-view.png panel-view.png; do
    sha256sum "${OUTPUT_DIR}/${filename}"
  done | jq -Rn '[inputs | split("  ") | {(.[1] | split("/") | last): .[0]}] | add'
)"

jq -n \
  --arg version "$(<"$ROOT_DIR/VERSION")" \
  --arg sessionId "$SESSION_ID" \
  --arg fixtureSha256 "$FIXTURE_SHA" \
  --arg plasmaSession "Fedora KDE Plasma" \
  --arg theme "Breeze Dark" \
  --arg environment "isolated demo user" \
  --argjson assets "$ASSETS_JSON" \
  '{version: $version, sessionId: $sessionId, fixtureSha256: $fixtureSha256,
    plasmaSession: $plasmaSession, theme: $theme, environment: $environment,
    assets: $assets}' \
  >"${OUTPUT_DIR}/v14-media-manifest.json"

python3 "$ROOT_DIR/scripts/check_release_media.py"
echo "v14 media capture complete: $OUTPUT_DIR"
