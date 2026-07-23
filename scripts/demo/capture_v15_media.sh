#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build/debug}"
OUTPUT_DIR="${2:-${ROOT_DIR}/assets/screenshots}"
SESSION_ROOT="$(mktemp -d)"
PREFIX="${SESSION_ROOT}/prefix"
CONFIG_HOME="${SESSION_ROOT}/config"
CACHE_HOME="${SESSION_ROOT}/cache"
KWIN_SCRIPT_NAME="ai-usage-monitor-v15-capture"
WINDOW_PID=""
SERVER_PID=""
NESTED_PID=""
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
  if [[ -n "$NESTED_PID" ]] && kill -0 "$NESTED_PID" 2>/dev/null; then
    kill -TERM -- "-$NESTED_PID" 2>/dev/null || true
    wait "$NESTED_PID" 2>/dev/null || true
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

for command in cmake plasmawindowed plasmashell kwin_wayland spectacle busctl \
  identify magick jq sha256sum kwriteconfig6 dbus-run-session setsid; do
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
  local layout_script="$3"
  local match_output
  local match_id
  local candidate_id
  local candidate_info

  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
    unloadScript s "$KWIN_SCRIPT_NAME" >/dev/null 2>&1 || true
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting loadScript \
    ss "$layout_script" "$KWIN_SCRIPT_NAME" >/dev/null
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
  local layout="${4:-wide}"
  local temporary="${OUTPUT_DIR}/.${filename}.capture"
  local dimensions
  local width
  local height
  local view_config_home="${CONFIG_HOME}/${view}"
  local view_cache_home="${CACHE_HOME}/${view}"
  local view_data_home="${SESSION_ROOT}/data/${view}"
  local layout_script="$ROOT_DIR/scripts/demo/kwin_capture_layout.js"
  local match_query="AI Usage Monitor"

  mkdir -p "$view_config_home" "$view_cache_home" "$view_data_home"
  if [[ "$layout" == "narrow" ]]; then
    layout_script="$ROOT_DIR/scripts/demo/kwin_capture_narrow.js"
  fi
  case "$view" in
    media-history-retained)
      python3 "$ROOT_DIR/scripts/demo/generate_v15_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario retained
      ;;
    media-history-gap)
      python3 "$ROOT_DIR/scripts/demo/generate_v15_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario gap
      ;;
    media-analyst-sufficient)
      python3 "$ROOT_DIR/scripts/demo/generate_v15_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario analyst-sufficient
      ;;
    media-analyst-insufficient)
      python3 "$ROOT_DIR/scripts/demo/generate_v15_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario analyst-insufficient
      ;;
  esac
  cp /usr/share/color-schemes/BreezeDark.colors "$view_config_home/kdeglobals"
  XDG_CONFIG_HOME="$view_config_home" kwriteconfig6 \
    --file kdeglobals --group General --key ColorScheme BreezeDark
  XDG_CONFIG_HOME="$view_config_home" kwriteconfig6 \
    --file plasmarc --group Theme --key name breeze-dark

  XDG_DATA_HOME="$view_data_home" \
  XDG_DATA_DIRS="$PREFIX/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
  XDG_CONFIG_HOME="$view_config_home" \
  XDG_CACHE_HOME="$view_cache_home" \
  QML_IMPORT_PATH="$QML_PATH" \
  QML2_IMPORT_PATH="$QML_PATH" \
  KDE_COLOR_SCHEME_PATH="/usr/share/color-schemes/BreezeDark.colors" \
  PLASMA_AI_MONITOR_DEMO=1 \
  PLASMA_AI_MONITOR_SMOKE_VIEW="$view" \
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 \
  QT_ACCESSIBILITY=1 \
  plasmawindowed com.github.loofi.aiusagemonitor \
    >"${SESSION_ROOT}/${view}.log" 2>&1 &
  WINDOW_PID=$!

  sleep "$settle_seconds"
  kill -0 "$WINDOW_PID" 2>/dev/null || {
    echo "Plasma window exited before capture: $view" >&2
    sed -n '1,160p' "${SESSION_ROOT}/${view}.log" >&2
    exit 1
  }

  if [[ "$view" == "settings" ]]; then
    python3 "$ROOT_DIR/scripts/demo/activate_accessible.py" \
      --window "AI Usage Monitor Settings" \
      --target "Providers"
    sleep 2
  fi

  # Re-run the KWin script after the view has settled. This both normalizes the
  # geometry and re-activates the exact plasmawindowed instance immediately
  # before Spectacle asks KWin for the active window.
  if [[ "$view" == "settings" ]]; then
    match_query="AI Usage Monitor Settings"
  fi
  focus_capture_window "$match_query" "$WINDOW_PID" "$layout_script"
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
  if [[ "$layout" == "narrow" ]]; then
    if (( width < 820 || width > 1000 || height < 1050 || height > 1300 )); then
      echo "Narrow capture has unexpected geometry: $filename (${width}x${height})" >&2
      unlink "$temporary"
      exit 1
    fi
  elif (( width < 1200 || width > 2100 || height < 700 || height > 1100 )); then
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

focus_nested_panel() {
  local match_output
  local match_id
  local candidate_id
  local candidate_info

  match_output="$(busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
    Match s "KDE Wayland Compositor")"
  match_id=""
  while IFS= read -r candidate_id; do
    candidate_info="$(busctl --user call org.kde.KWin /KWin org.kde.KWin \
      getWindowInfo s "${candidate_id#0_}")"
    rg -Fq "KDE Wayland Compositor" <<<"$candidate_info" || continue
    match_id="$candidate_id"
    break
  done < <(rg -o '"0_\{[^"]+\}"' <<<"$match_output" | tr -d '"')
  [[ -n "$match_id" ]] || {
    echo "KWin could not identify the nested Plasma panel window" >&2
    return 1
  }
  busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
    Run ss "$match_id" "" >/dev/null
  sleep 2
}

capture_panel() {
  local filename="panel-lowest-quota.png"
  local temporary="${OUTPUT_DIR}/.${filename}.capture"
  local raw="${temporary}.raw.png"
  local panel_config_home="${CONFIG_HOME}/panel"
  local panel_cache_home="${CACHE_HOME}/panel"
  local panel_data_home="${SESSION_ROOT}/panel-data"
  local nested_socket="wayland-aiusage-${RANDOM}-${RANDOM}"
  local panel_script
  local dimensions
  local width
  local height
  local cropped_height

  mkdir -p "$panel_config_home" "$panel_cache_home" "$panel_data_home"
  cp /usr/share/color-schemes/BreezeDark.colors "$panel_config_home/kdeglobals"
  XDG_CONFIG_HOME="$panel_config_home" kwriteconfig6 \
    --file kdeglobals --group General --key ColorScheme BreezeDark
  XDG_CONFIG_HOME="$panel_config_home" kwriteconfig6 \
    --file plasmarc --group Theme --key name breeze-dark

  panel_script='var existing = panels(); for (var i = 0; i < existing.length; ++i) existing[i].remove(); var panel = new Panel; panel.location = "bottom"; panel.height = 58; panel.addWidget("org.kde.plasma.kickoff"); var monitor = panel.addWidget("com.github.loofi.aiusagemonitor"); monitor.currentConfigGroup = ["General"]; monitor.writeConfig("compactDisplayMode", "lowest-quota"); panel.addWidget("org.kde.plasma.panelspacer"); panel.addWidget("org.kde.plasma.digitalclock");'

  # The quoted script expands its variables inside the isolated D-Bus session.
  # shellcheck disable=SC2016
  setsid dbus-run-session -- env \
    XDG_DATA_HOME="$panel_data_home" \
    XDG_DATA_DIRS="$PREFIX/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
    XDG_CONFIG_HOME="$panel_config_home" \
    XDG_CACHE_HOME="$panel_cache_home" \
    QML_IMPORT_PATH="$QML_PATH" \
    QML2_IMPORT_PATH="$QML_PATH" \
    KDE_COLOR_SCHEME_PATH="/usr/share/color-schemes/BreezeDark.colors" \
    PLASMA_AI_MONITOR_DEMO=1 \
    PLASMA_AI_MONITOR_SMOKE_VIEW=media-panel \
    bash -c '
      kwin_wayland --wayland-display "$WAYLAND_DISPLAY" -s "$1" \
        --width 1600 --height 900 --scale 1 --xwayland --no-lockscreen \
        --no-global-shortcuts --exit-with-session /usr/bin/plasmashell &
      kwin_pid=$!
      for _ in $(seq 1 45); do
        if busctl --user call org.kde.plasmashell /PlasmaShell \
            org.kde.PlasmaShell evaluateScript s "$2" >/dev/null 2>&1; then
          echo PANEL_READY
          break
        fi
        sleep 1
      done
      wait "$kwin_pid"
    ' nested "$nested_socket" "$panel_script" \
    >"${SESSION_ROOT}/panel.log" 2>&1 &
  NESTED_PID=$!

  for _ in {1..55}; do
    rg -q '^PANEL_READY$' "${SESSION_ROOT}/panel.log" && break
    kill -0 "$NESTED_PID" 2>/dev/null || {
      echo "Nested Plasma session exited before panel capture" >&2
      sed -n '1,180p' "${SESSION_ROOT}/panel.log" >&2
      exit 1
    }
    sleep 1
  done
  rg -q '^PANEL_READY$' "${SESSION_ROOT}/panel.log" || {
    echo "Nested Plasma panel did not become ready" >&2
    sed -n '1,180p' "${SESSION_ROOT}/panel.log" >&2
    exit 1
  }
  sleep 8

  focus_nested_panel
  spectacle --activewindow --background --nonotify --output "$raw"
  for _ in {1..20}; do
    [[ -s "$raw" ]] && break
    sleep 0.25
  done
  [[ -s "$raw" ]] || {
    echo "Spectacle did not create $filename" >&2
    exit 1
  }

  dimensions="$(identify -format '%w %h' "$raw")"
  width="${dimensions%% *}"
  height="${dimensions##* }"
  cropped_height=$((height - 50))
  (( cropped_height > 0 )) || {
    echo "Nested panel capture is too short: ${width}x${height}" >&2
    exit 1
  }
  magick "$raw" -gravity South -crop "${width}x${cropped_height}+0+0" \
    +repage -resize '1600x900>' "$temporary"
  unlink "$raw"

  dimensions="$(identify -format '%w %h' "$temporary")"
  width="${dimensions%% *}"
  height="${dimensions##* }"
  if (( width < 1200 || width > 2100 || height < 700 || height > 1100 )); then
    echo "Panel capture has unexpected geometry: ${width}x${height}" >&2
    unlink "$temporary"
    exit 1
  fi
  mv "$temporary" "${OUTPUT_DIR}/${filename}"

  kill -TERM -- "-$NESTED_PID" 2>/dev/null || true
  wait "$NESTED_PID" 2>/dev/null || true
  NESTED_PID=""
  sleep 2
  echo "Captured $filename (${width}x${height}) from an isolated Plasma panel"
}

capture_view media-overview overview-popup.png 6 narrow
capture_view media-attention attention-state.png 5
capture_view media-quota quota-reset-state.png 5
capture_view media-tool-only tool-only-overview.png 5
capture_view media-history-retained retained-history.png 10
capture_view media-history-gap history-gap.png 10
capture_view media-analyst-sufficient analyst-sufficient.png 6
capture_view media-analyst-insufficient analyst-insufficient.png 6
capture_panel

SESSION_ID="$(cat /proc/sys/kernel/random/uuid)"
FIXTURE_SHA="$(
  cd "$ROOT_DIR"
  sha256sum \
    scripts/demo/showcase_preset.json \
    scripts/demo/generate_v15_media_history.py \
    package/contents/ui/components/MediaDailyState.qml \
    | sha256sum | cut -d' ' -f1
)"
SOURCE_TREE_SHA="$(
  cd "$ROOT_DIR"
  git ls-files --cached --others --exclude-standard -- VERSION package scripts/demo \
    | sort \
    | while IFS= read -r path; do
        [[ -f "$path" ]] && sha256sum "$path"
      done \
    | sha256sum | cut -d' ' -f1
)"
CAPTURE_COMMIT="${CAPTURE_COMMIT:-$(git -C "$ROOT_DIR" rev-parse HEAD)}"
CAPTURED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
PLASMA_VERSION="$(plasmashell --version | awk '{print $2}')"
ASSETS_JSON="$(
  for filename in \
    overview-popup.png attention-state.png quota-reset-state.png \
    tool-only-overview.png retained-history.png history-gap.png \
    analyst-sufficient.png analyst-insufficient.png panel-lowest-quota.png; do
    sha256sum "${OUTPUT_DIR}/${filename}"
  done | jq -Rn '[inputs | split("  ") | {(.[1] | split("/") | last): .[0]}] | add'
)"

jq -n \
  --arg version "$(<"$ROOT_DIR/VERSION")" \
  --arg sessionId "$SESSION_ID" \
  --arg fixtureSha256 "$FIXTURE_SHA" \
  --arg sourceTreeSha256 "$SOURCE_TREE_SHA" \
  --arg plasmaSession "Fedora KDE Plasma" \
  --arg theme "Breeze Dark" \
  --arg environment "isolated demo user" \
  --arg captureCommit "$CAPTURE_COMMIT" \
  --arg capturedAt "$CAPTURED_AT" \
  --arg plasmaVersion "$PLASMA_VERSION" \
  --arg scale "100%" \
  --argjson assets "$ASSETS_JSON" \
  '{version: $version, sessionId: $sessionId, fixtureSha256: $fixtureSha256,
    sourceTreeSha256: $sourceTreeSha256,
    plasmaSession: $plasmaSession, theme: $theme, environment: $environment,
    captureCommit: $captureCommit, capturedAt: $capturedAt,
    plasmaVersion: $plasmaVersion, scale: $scale,
    scenarios: {
      "overview-popup.png": "media-overview",
      "attention-state.png": "media-attention",
      "quota-reset-state.png": "media-quota",
      "tool-only-overview.png": "media-tool-only",
      "retained-history.png": "media-history-retained",
      "history-gap.png": "media-history-gap",
      "analyst-sufficient.png": "media-analyst-sufficient",
      "analyst-insufficient.png": "media-analyst-insufficient",
      "panel-lowest-quota.png": "media-panel"
    },
    assets: $assets}' \
  >"${OUTPUT_DIR}/v15-media-manifest.json"

python3 "$ROOT_DIR/scripts/check_release_media.py"
echo "v15 media capture complete: $OUTPUT_DIR"
