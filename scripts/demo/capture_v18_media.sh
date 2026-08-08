#!/usr/bin/env bash
set -euo pipefail

if [[ "${AI_USAGE_V18_CAPTURE_INNER:-0}" != "1" ]]; then
  CAPTURE_SCRIPT="$(realpath "$0")"
  CAPTURE_ROOT="$(cd "$(dirname "$CAPTURE_SCRIPT")/../.." && pwd)"
  CAPTURE_BUILD_DIR="${1:-}"
  CAPTURE_OUTPUT_DIR="${2:-}"
  VIRTUAL_ROOT="$(mktemp -d)"
  VIRTUAL_RUNTIME="${VIRTUAL_ROOT}/runtime"
  mkdir -p "$VIRTUAL_RUNTIME"
  chmod 700 "$VIRTUAL_RUNTIME"
  CAPTURE_SCRIPT_LINK="${VIRTUAL_ROOT}/capture-v18"
  ln -s "$CAPTURE_SCRIPT" "$CAPTURE_SCRIPT_LINK"
  cleanup_virtual() {
    if [[ -d "$VIRTUAL_ROOT" && "$VIRTUAL_ROOT" == /tmp/* ]]; then
      find "$VIRTUAL_ROOT" -depth -delete
    fi
  }
  trap cleanup_virtual EXIT
  dbus-run-session -- env \
    AI_USAGE_V18_CAPTURE_INNER=1 \
    AI_USAGE_V18_BUILD_DIR="$CAPTURE_BUILD_DIR" \
    AI_USAGE_V18_OUTPUT_DIR="$CAPTURE_OUTPUT_DIR" \
    AI_USAGE_V18_ROOT_DIR="$CAPTURE_ROOT" \
    XDG_RUNTIME_DIR="$VIRTUAL_RUNTIME" \
    QT_IM_MODULE= \
    GTK_IM_MODULE= \
    kwin_wayland --virtual --socket wayland-v18-capture \
      --width 1920 --height 1200 --scale 1 --no-lockscreen \
      --no-global-shortcuts --exit-with-session "$CAPTURE_SCRIPT_LINK"
  exit $?
fi

/usr/libexec/at-spi-bus-launcher --launch-immediately --a11y=1 >/dev/null 2>&1 &
sleep 1
/usr/libexec/at-spi2-registryd --dbus-name org.a11y.atspi.Registry >/dev/null 2>&1 &

ROOT_DIR="${AI_USAGE_V18_ROOT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}"
BUILD_DIR="${1:-${AI_USAGE_V18_BUILD_DIR:-${ROOT_DIR}/build/debug}}"
OUTPUT_DIR="${2:-${AI_USAGE_V18_OUTPUT_DIR:-${ROOT_DIR}/assets/screenshots}}"
SESSION_ROOT="$(mktemp -d)"
PREFIX="${SESSION_ROOT}/prefix"
CONFIG_HOME="${SESSION_ROOT}/config"
CACHE_HOME="${SESSION_ROOT}/cache"
EVIDENCE_JSONL="${SESSION_ROOT}/capture-evidence.jsonl"
KWIN_SCRIPT_NAME="ai-usage-monitor-v18-capture"
WINDOW_PID=""
SERVER_PID=""
NESTED_PID=""
KWIN_SCRIPT_LOADED=0
FOCUSED_WINDOW_ID=""
FOCUSED_WINDOW_INFO=""

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
  identify magick jq sha256sum kwriteconfig6 dbus-run-session setsid pgrep; do
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
    if ! rg -q "\"pid\" [a-z]+ $expected_pid([[:space:]]|$)" <<<"$candidate_info"; then
      continue
    fi
    match_id="$candidate_id"
    break
  done < <(rg -o '"0_\{[^"]+\}"' <<<"$match_output" | tr -d '"')
  [[ -n "$match_id" ]] || {
    echo "KWin could not identify the capture window: $match_query" >&2
    return 1
  }
  FOCUSED_WINDOW_ID="${match_id#0_}"
  FOCUSED_WINDOW_INFO="$candidate_info"
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
  local view_home="${SESSION_ROOT}/home/${view}"
  local view_qml_path="$QML_PATH"
  local layout_script="$ROOT_DIR/scripts/demo/kwin_capture_v18_layout.js"
  local match_query="AI Usage Monitor"
  local accessible_marker="Overview view ready"
  local accessible_before
  local accessible_after

  mkdir -p "$view_config_home" "$view_cache_home" "$view_data_home" "$view_home"
  if [[ "$layout" == "narrow" ]]; then
    layout_script="$ROOT_DIR/scripts/demo/kwin_capture_v18_narrow.js"
  fi
  case "$view" in
    media-history-retained)
      python3 "$ROOT_DIR/scripts/demo/generate_v17_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario retained
      ;;
    media-history-gap)
      python3 "$ROOT_DIR/scripts/demo/generate_v17_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario gap
      ;;
    media-analyst-sufficient)
      python3 "$ROOT_DIR/scripts/demo/generate_v17_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario analyst-sufficient
      ;;
    media-analyst-insufficient)
      python3 "$ROOT_DIR/scripts/demo/generate_v17_media_history.py" \
        --output "$view_data_home/plasma-ai-usage-monitor/usage_history.db" \
        --scenario analyst-insufficient
      ;;
  esac
  case "$view" in
    media-history-*) accessible_marker="History view ready" ;;
    media-analyst-*) accessible_marker="Insights view ready" ;;
    media-source-detail) accessible_marker="Back to source list" ;;
    onboarding-*) accessible_marker="Guided first success" ;;
    settings) accessible_marker="Providers" ;;
    budget-settings) accessible_marker="Budget Control" ;;
    plugin-recovery)
      accessible_marker="The native plugin is older than the widget"
      view_qml_path="$ROOT_DIR/scripts/fixtures/bootstrap/plugin-older:$QML_PATH"
      ;;
  esac
  cp /usr/share/color-schemes/BreezeDark.colors "$view_config_home/kdeglobals"
  XDG_CONFIG_HOME="$view_config_home" kwriteconfig6 \
    --file kdeglobals --group General --key ColorScheme BreezeDark
  XDG_CONFIG_HOME="$view_config_home" kwriteconfig6 \
    --file plasmarc --group Theme --key name breeze-dark

  HOME="$view_home" \
  XDG_DATA_HOME="$view_data_home" \
  XDG_DATA_DIRS="$PREFIX/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
  XDG_CONFIG_HOME="$view_config_home" \
  XDG_CACHE_HOME="$view_cache_home" \
  QML_IMPORT_PATH="$view_qml_path" \
  QML2_IMPORT_PATH="$view_qml_path" \
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

  if [[ "$view" == "settings" || "$view" == "budget-settings" ]]; then
    match_query="AI Usage Monitor Settings"
    local settings_target="Providers"
    if [[ "$view" == "budget-settings" ]]; then
      settings_target="Budget Control"
    fi
    python3 "$ROOT_DIR/scripts/demo/activate_accessible.py" \
      --window "$match_query" \
      --target "$settings_target"
    sleep 2
  fi
  accessible_before="$(
    python3 "$ROOT_DIR/scripts/demo/wait_accessible.py" \
      --pid "$WINDOW_PID" --window "$match_query" \
      --target "$accessible_marker"
  )"

  # Re-run the KWin script after the view has settled. This both normalizes the
  # geometry and re-activates the exact plasmawindowed instance immediately
  # before Spectacle asks KWin for the active window.
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
  accessible_after="$(
    python3 "$ROOT_DIR/scripts/demo/wait_accessible.py" \
      --pid "$WINDOW_PID" --window "$match_query" \
      --target "$accessible_marker"
  )"
  jq -cn \
    --arg asset "$filename" \
    --argjson pid "$WINDOW_PID" \
    --arg windowId "$FOCUSED_WINDOW_ID" \
    --arg windowInfo "$FOCUSED_WINDOW_INFO" \
    --arg marker "$accessible_marker" \
    --arg before "$accessible_before" \
    --arg after "$accessible_after" \
    '{asset: $asset, pid: $pid, windowId: $windowId,
      windowIdentity: $windowInfo, marker: $marker,
      markerBefore: $before, markerAfter: $after}' >>"$EVIDENCE_JSONL"

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
  local expected_pid="$1"
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
    rg -q "\"pid\" [a-z]+ $expected_pid([[:space:]]|$)" \
      <<<"$candidate_info" || continue
    match_id="$candidate_id"
    break
  done < <(rg -o '"0_\{[^"]+\}"' <<<"$match_output" | tr -d '"')
  [[ -n "$match_id" ]] || {
    echo "KWin could not identify the nested Plasma panel window" >&2
    return 1
  }
  FOCUSED_WINDOW_ID="${match_id#0_}"
  FOCUSED_WINDOW_INFO="$candidate_info"
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
  local panel_home="${SESSION_ROOT}/home/panel"
  local panel_runtime_dir="${SESSION_ROOT}/panel-runtime"
  local nested_pid_file="${SESSION_ROOT}/nested-kwin.pid"
  local panel_pid_file="${SESSION_ROOT}/nested-plasmashell.pid"
  local nested_bus_file="${SESSION_ROOT}/nested-dbus-address"
  local nested_socket="wayland-aiusage-${RANDOM}-${RANDOM}"
  local panel_script
  local dimensions
  local width
  local height
  local cropped_height

  mkdir -p "$panel_config_home" "$panel_cache_home" "$panel_data_home" \
    "$panel_home" "$panel_runtime_dir"
  chmod 700 "$panel_runtime_dir"
  cp /usr/share/color-schemes/BreezeDark.colors "$panel_config_home/kdeglobals"
  XDG_CONFIG_HOME="$panel_config_home" kwriteconfig6 \
    --file kdeglobals --group General --key ColorScheme BreezeDark
  XDG_CONFIG_HOME="$panel_config_home" kwriteconfig6 \
    --file plasmarc --group Theme --key name breeze-dark

  magick -size 1600x900 'xc:#202326' "$panel_home/wallpaper.png"
  panel_script="var existing = panels(); for (var i = 0; i < existing.length; ++i) existing[i].remove(); var desktopsList = desktops(); for (var d = 0; d < desktopsList.length; ++d) { var widgets = desktopsList[d].widgets(); for (var w = 0; w < widgets.length; ++w) widgets[w].remove(); desktopsList[d].wallpaperPlugin = \"org.kde.image\"; desktopsList[d].currentConfigGroup = [\"Wallpaper\", \"org.kde.image\", \"General\"]; desktopsList[d].writeConfig(\"Image\", \"file://${panel_home}/wallpaper.png\"); } var panel = new Panel; panel.location = \"bottom\"; panel.height = 58; panel.addWidget(\"org.kde.plasma.kickoff\"); var monitor = panel.addWidget(\"com.github.loofi.aiusagemonitor\"); monitor.currentConfigGroup = [\"General\"]; monitor.writeConfig(\"compactDisplayMode\", \"lowest-quota\"); panel.addWidget(\"org.kde.plasma.panelspacer\"); panel.addWidget(\"org.kde.plasma.digitalclock\");"

  # The quoted script expands its variables inside the isolated D-Bus session.
  # shellcheck disable=SC2016
  setsid dbus-run-session -- env \
    HOME="$panel_home" \
    XDG_DATA_HOME="$panel_data_home" \
    XDG_DATA_DIRS="$PREFIX/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
    XDG_CONFIG_HOME="$panel_config_home" \
    XDG_CACHE_HOME="$panel_cache_home" \
    WAYLAND_DISPLAY="${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}" \
    XDG_RUNTIME_DIR="$panel_runtime_dir" \
    QML_IMPORT_PATH="$QML_PATH" \
    QML2_IMPORT_PATH="$QML_PATH" \
    KDE_COLOR_SCHEME_PATH="/usr/share/color-schemes/BreezeDark.colors" \
    PLASMA_AI_MONITOR_DEMO=1 \
    PLASMA_AI_MONITOR_SMOKE_VIEW=media-panel \
    QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 \
    QT_ACCESSIBILITY=1 \
    bash -c '
      /usr/libexec/at-spi-bus-launcher --launch-immediately --a11y=1 >/dev/null 2>&1 &
      sleep 1
      /usr/libexec/at-spi2-registryd --dbus-name org.a11y.atspi.Registry >/dev/null 2>&1 &
      kwin_wayland --wayland-display "$WAYLAND_DISPLAY" -s "$1" \
        --width 1600 --height 900 --scale 1 --xwayland --no-lockscreen \
        --no-global-shortcuts --exit-with-session /usr/bin/plasmashell &
      kwin_pid=$!
      echo "$kwin_pid" >"$3"
      for _ in $(seq 1 45); do
        if busctl --user call org.kde.plasmashell /PlasmaShell \
            org.kde.PlasmaShell evaluateScript s "$2" >/dev/null 2>&1; then
          echo PANEL_READY
          break
        fi
        sleep 1
      done
      panel_pid="$(pgrep -P "$kwin_pid" -x plasmashell | head -1)"
      test -n "$panel_pid"
      echo "$panel_pid" >"$4"
      echo "$DBUS_SESSION_BUS_ADDRESS" >"$5"
      python3 "$6" --pid "$panel_pid" \
        --target "AI Usage Monitor:" &&
        echo PANEL_ACCESSIBLE_READY
      wait "$kwin_pid"
    ' nested "$nested_socket" "$panel_script" "$nested_pid_file" \
      "$panel_pid_file" "$nested_bus_file" \
      "$ROOT_DIR/scripts/demo/wait_accessible.py" \
    >"${SESSION_ROOT}/panel.log" 2>&1 &
  NESTED_PID=$!

  for _ in {1..55}; do
    if rg -q '^PANEL_READY$' "${SESSION_ROOT}/panel.log" \
        && rg -q '^PANEL_ACCESSIBLE_READY$' "${SESSION_ROOT}/panel.log"; then
      break
    fi
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
  rg -q '^PANEL_ACCESSIBLE_READY$' "${SESSION_ROOT}/panel.log" || {
    echo "Nested panel accessibility marker did not become ready" >&2
    sed -n '1,180p' "${SESSION_ROOT}/panel.log" >&2
    exit 1
  }
  sleep 8

  local nested_window_pid
  local panel_accessible_pid
  local panel_marker_before
  local panel_marker_after
  nested_window_pid="$(<"$nested_pid_file")"
  panel_accessible_pid="$(<"$panel_pid_file")"
  panel_marker_before="$(
    rg 'accessible_ready_ms=.*AI Usage Monitor:' "${SESSION_ROOT}/panel.log" \
      | tail -1
  )"
  focus_nested_panel "$nested_window_pid"
  spectacle --activewindow --background --nonotify --output "$raw"
  for _ in {1..20}; do
    [[ -s "$raw" ]] && break
    sleep 0.25
  done
  [[ -s "$raw" ]] || {
    echo "Spectacle did not create $filename" >&2
    exit 1
  }
  panel_marker_after="$(
    DBUS_SESSION_BUS_ADDRESS="$(<"$nested_bus_file")" \
      python3 "$ROOT_DIR/scripts/demo/wait_accessible.py" \
        --pid "$panel_accessible_pid" --target "AI Usage Monitor:"
  )"
  jq -cn \
    --arg asset "$filename" \
    --argjson pid "$nested_window_pid" \
    --argjson accessiblePid "$panel_accessible_pid" \
    --arg windowId "$FOCUSED_WINDOW_ID" \
    --arg windowInfo "$FOCUSED_WINDOW_INFO" \
    --arg marker "AI Usage Monitor:" \
    --arg markerBefore "$panel_marker_before" \
    --arg markerAfter "$panel_marker_after" \
    '{asset: $asset, pid: $pid, accessiblePid: $accessiblePid,
      windowId: $windowId,
      windowIdentity: $windowInfo, marker: $marker,
      markerBefore: $markerBefore,
      markerAfter: $markerAfter}' >>"$EVIDENCE_JSONL"

  dimensions="$(identify -format '%w %h' "$raw")"
  width="${dimensions%% *}"
  height="${dimensions##* }"
  cropped_height=$((height - 50))
  (( cropped_height > 0 )) || {
    echo "Nested panel capture is too short: ${width}x${height}" >&2
    exit 1
  }
  magick "$raw" -fill '#202326' \
    -draw "rectangle 0,0 ${width},$((height - 70))" \
    -gravity South -crop "${width}x${cropped_height}+0+0" \
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
capture_view media-source-detail source-detail.png 6
capture_view media-history-gap history-gap.png 10
capture_view media-analyst-sufficient analyst-sufficient.png 6
capture_view media-analyst-insufficient analyst-insufficient.png 6
capture_view onboarding-source guided-first-success.png 6
capture_view budget-settings budget-control.png 8
capture_view plugin-recovery plugin-recovery.png 6
capture_panel

SESSION_ID="$(cat /proc/sys/kernel/random/uuid)"
FIXTURE_SHA="$(
  cd "$ROOT_DIR"
  sha256sum \
    scripts/demo/showcase_preset.json \
    scripts/demo/generate_v17_media_history.py \
    package/contents/ui/components/MediaDailyState.qml \
    | sha256sum | cut -d' ' -f1
)"
SOURCE_TREE_SHA="$(
  cd "$ROOT_DIR"
  git ls-files --cached --others --exclude-standard -- \
    CMakeLists.txt package plugin scripts/demo \
    | sort \
    | while IFS= read -r path; do
        [[ -f "$path" ]] && sha256sum "$path"
      done \
    | sha256sum | cut -d' ' -f1
)"
CAPTURE_COMMIT="${CAPTURE_COMMIT:-$(git -C "$ROOT_DIR" rev-parse HEAD)}"
SOURCE_TREE_COMMIT="${SOURCE_TREE_COMMIT:-$CAPTURE_COMMIT}"
SOURCE_TREE_MODE="git-commit"
if [[ -n "$(git -C "$ROOT_DIR" status --short -- package plugin scripts/demo)" ]]; then
  SOURCE_TREE_MODE="filesystem-release-candidate"
fi
CAPTURED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
PLASMA_VERSION="$(plasmashell --version | awk '{print $2}')"
ASSETS_JSON="$(
  for filename in \
    overview-popup.png attention-state.png source-detail.png history-gap.png \
    analyst-sufficient.png analyst-insufficient.png guided-first-success.png \
    budget-control.png plugin-recovery.png panel-lowest-quota.png; do
    sha256sum "${OUTPUT_DIR}/${filename}"
  done | jq -Rn '[inputs | split("  ") | {(.[1] | split("/") | last): .[0]}] | add'
)"
CAPTURE_EVIDENCE="$(
  jq -s 'map({key: .asset, value: del(.asset)}) | from_entries' \
    "$EVIDENCE_JSONL"
)"

jq -n \
  --arg version "$(<"$ROOT_DIR/RELEASE_TARGET")" \
  --arg sessionId "$SESSION_ID" \
  --arg fixtureSha256 "$FIXTURE_SHA" \
  --arg sourceTreeSha256 "$SOURCE_TREE_SHA" \
  --arg archiveSourceTreeSha256 "$SOURCE_TREE_SHA" \
  --arg sourceTreeCommit "$SOURCE_TREE_COMMIT" \
  --arg sourceTreeMode "$SOURCE_TREE_MODE" \
  --arg plasmaSession "Fedora KDE Plasma" \
  --arg theme "Breeze Dark" \
  --arg environment "isolated demo user" \
  --arg captureCommit "$CAPTURE_COMMIT" \
  --arg capturedAt "$CAPTURED_AT" \
  --arg plasmaVersion "$PLASMA_VERSION" \
  --arg scale "100%" \
  --argjson captureEvidence "$CAPTURE_EVIDENCE" \
  --argjson assets "$ASSETS_JSON" \
  '{version: $version, sessionId: $sessionId, fixtureSha256: $fixtureSha256,
    sourceTreeSha256: $sourceTreeSha256,
    archiveSourceTreeSha256: $archiveSourceTreeSha256,
    sourceTreeCommit: $sourceTreeCommit,
    sourceTreeMode: $sourceTreeMode,
    plasmaSession: $plasmaSession, theme: $theme, environment: $environment,
    captureCommit: $captureCommit, capturedAt: $capturedAt,
    plasmaVersion: $plasmaVersion, scale: $scale,
    captureEvidence: $captureEvidence,
    scenarios: {
      "overview-popup.png": "media-overview",
      "attention-state.png": "media-attention",
      "source-detail.png": "media-source-detail",
      "history-gap.png": "media-history-gap",
      "analyst-sufficient.png": "media-analyst-sufficient",
      "analyst-insufficient.png": "media-analyst-insufficient",
      "guided-first-success.png": "onboarding-source",
      "budget-control.png": "budget-settings",
      "plugin-recovery.png": "plugin-recovery",
      "panel-lowest-quota.png": "media-panel"
    },
    assets: $assets}' \
  >"${OUTPUT_DIR}/v18-media-manifest.json"

python3 "$ROOT_DIR/scripts/check_release_media.py"
echo "v18 media capture complete: $OUTPUT_DIR"
