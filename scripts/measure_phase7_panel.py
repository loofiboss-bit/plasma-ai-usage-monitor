#!/usr/bin/env python3
"""Measure a real Plasma panel popup in an isolated nested session."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

from measure_phase7_runtime import candidate_environment

ROOT = Path(__file__).resolve().parents[1]


def available_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def run_panel_session(
    env: dict[str, str], runtime_root: Path, request_log: Path
) -> dict[str, int]:
    nested_log = runtime_root / "nested-plasma.log"
    measurement_log = runtime_root / "panel-measurement.json"
    measurement_error = runtime_root / "panel-measurement.log"
    socket_name = f"wayland-ai-monitor-phase7-{os.getpid()}-{time.time_ns()}"
    panel_script = (
        "var existing=panels();"
        "for(var i=0;i<existing.length;++i)existing[i].remove();"
        "var panel=new Panel;"
        'panel.location="bottom";'
        "panel.height=58;"
        'panel.addWidget("org.kde.plasma.kickoff");'
        'panel.addWidget("com.github.loofi.aiusagemonitor");'
        'panel.addWidget("org.kde.plasma.panelspacer");'
        'panel.addWidget("org.kde.plasma.digitalclock");'
    )
    inner = r"""
set -euo pipefail
/usr/libexec/at-spi-bus-launcher --launch-immediately \
  >>"$4" 2>&1 &
a11y_pid=$!
sleep 0.25
/usr/libexec/at-spi2-registryd --use-gnome-session \
  >>"$4" 2>&1 &
a11y_registry_pid=$!
setsid kwin_wayland --wayland-display "$OUTER_WAYLAND_DISPLAY" -s "$1" \
  --width 1600 --height 900 --scale 1 --xwayland --no-lockscreen \
  --no-global-shortcuts --exit-with-session /usr/bin/plasmashell \
  >"$4" 2>&1 &
kwin_pid=$!
cleanup() {
  kill -TERM -- "-$kwin_pid" >/dev/null 2>&1 || true
  kill -TERM "$a11y_pid" >/dev/null 2>&1 || true
  kill -TERM "$a11y_registry_pid" >/dev/null 2>&1 || true
}
trap cleanup EXIT
ready=0
for _ in $(seq 1 45); do
  if busctl --user call org.kde.plasmashell /PlasmaShell \
      org.kde.PlasmaShell evaluateScript s "$2" >/dev/null 2>&1; then
    ready=1
    break
  fi
  sleep 1
done
if [[ "$ready" -ne 1 ]]; then
  echo "Nested Plasma panel did not become ready" >&2
  exit 1
fi
busctl --user set-property org.kde.plasmashell /PlasmaShell \
  org.kde.PlasmaShell editMode b false >/dev/null 2>&1 || true
sleep 2
match_output="$(DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
  Match s "KDE Wayland Compositor" 2>/dev/null || true)"
match_id=""
while IFS= read -r candidate_id; do
  candidate_info="$(DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
    busctl --user call org.kde.KWin /KWin org.kde.KWin \
    getWindowInfo s "${candidate_id#0_}" 2>/dev/null || true)"
  if rg -q "\"pid\" [a-z]+ $kwin_pid([[:space:]]|$)" <<<"$candidate_info"; then
    match_id="$candidate_id"
    break
  fi
done < <(printf '%s\n' "$match_output" \
  | rg -o '"0_\{[^"]+\}"' | tr -d '"' || true)
if [[ -n "$match_id" ]]; then
  DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
    busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
    Run ss "$match_id" "" >/dev/null 2>&1 || true
fi
sleep 2
timeout 55s python3 "$3" --request-log "$5" >"$6" 2>"$7"
"""
    command = [
        "dbus-run-session",
        "--",
        "bash",
        "-c",
        inner,
        "phase7-panel",
        socket_name,
        panel_script,
        str(ROOT / "scripts/demo/measure_panel_accessible.py"),
        str(nested_log),
        str(request_log),
        str(measurement_log),
        str(measurement_error),
    ]
    result = subprocess.run(
        command,
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=120,
        check=False,
    )
    if result.returncode != 0:
        detail = (
            measurement_error.read_text(encoding="utf-8").strip()
            if measurement_error.exists()
            else ""
        )
        if nested_log.exists():
            detail += "\n" + nested_log.read_text(encoding="utf-8")[-4000:]
        raise RuntimeError(detail)
    if not measurement_log.exists():
        raise RuntimeError("Panel measurement returned no JSON")
    return json.loads(measurement_log.read_text(encoding="utf-8"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode", choices=("installed", "candidate"), required=True
    )
    parser.add_argument("--build-dir", type=Path, default=Path("build/release"))
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    required = (
        "bash",
        "busctl",
        "dbus-run-session",
        "kwin_wayland",
        "plasmashell",
        "rg",
        "setsid",
        "timeout",
    )
    missing = [command for command in required if shutil.which(command) is None]
    for executable in (
        Path("/usr/libexec/at-spi-bus-launcher"),
        Path("/usr/libexec/at-spi2-registryd"),
    ):
        if not executable.is_file():
            missing.append(str(executable))
    if missing:
        parser.error("Missing commands: " + ", ".join(missing))

    measurements: list[dict[str, int]] = []
    with tempfile.TemporaryDirectory(
        prefix="ai-monitor-phase7-panel-", ignore_cleanup_errors=True
    ) as temporary:
        runtime_root = Path(temporary)
        env = os.environ.copy()
        if args.mode == "candidate":
            env = candidate_environment(args.build_dir.resolve(), runtime_root)
        else:
            env.update(
                {
                    "XDG_DATA_HOME": str(runtime_root / "data"),
                    "XDG_CONFIG_HOME": str(runtime_root / "config"),
                    "XDG_CACHE_HOME": str(runtime_root / "cache"),
                }
            )
        env.update(
            {
                "OUTER_WAYLAND_DISPLAY": os.environ.get(
                    "WAYLAND_DISPLAY", "wayland-0"
                ),
                "OUTER_DBUS_SESSION_BUS_ADDRESS": os.environ.get(
                    "DBUS_SESSION_BUS_ADDRESS", ""
                ),
                "PLASMA_AI_MONITOR_DEMO": "1",
                "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
                "QT_ACCESSIBILITY": "1",
            }
        )

        for run in range(args.runs):
            last_error: RuntimeError | None = None
            for attempt in range(3):
                port = available_port()
                request_log = runtime_root / f"requests-{run}-{attempt}.jsonl"
                server = subprocess.Popen(
                    [
                        "python3",
                        str(ROOT / "scripts/demo/mock_ai_usage_server.py"),
                        "--port",
                        str(port),
                        "--request-log",
                        str(request_log),
                    ],
                    cwd=ROOT,
                    text=True,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                try:
                    env["PLASMA_AI_MONITOR_DEMO_BASE_URL"] = (
                        f"http://127.0.0.1:{port}"
                    )
                    time.sleep(0.25)
                    if server.poll() is not None:
                        raise RuntimeError("Mock server failed to start")
                    measurements.append(
                        run_panel_session(env, runtime_root, request_log)
                    )
                    last_error = None
                    break
                except RuntimeError as error:
                    last_error = error
                finally:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait(timeout=3)
            if last_error is not None:
                raise last_error

    result = {
        "schema": 1,
        "mode": args.mode,
        "runs": args.runs,
        "first_panel_popup_ms": [
            item["first_panel_popup_ms"] for item in measurements
        ],
        "first_panel_popup_median_ms": statistics.median(
            item["first_panel_popup_ms"] for item in measurements
        ),
        "warm_panel_popup_ms": [
            item["warm_panel_popup_ms"] for item in measurements
        ],
        "warm_panel_popup_median_ms": statistics.median(
            item["warm_panel_popup_ms"] for item in measurements
        ),
        "startup_network_requests": [
            item["startup_network_requests"] for item in measurements
        ],
        "fresh_popup_network_requests": [
            item["fresh_popup_network_requests"] for item in measurements
        ],
    }
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
