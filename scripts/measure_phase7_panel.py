#!/usr/bin/env python3
"""Compare exact v17 and candidate panel popup performance in isolated sessions."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shlex
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

from measure_phase7_runtime import candidate_environment

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RUNS = 20
DEFAULT_WARMUPS = 3
POPUP_INSTRUMENTATION = r'''
    Component.onCompleted: {
        console.warn("AI_USAGE_PERF_INSTRUMENTATION_READY");
    }
    property string perfFrameMarker: ""
    property int perfOpenCount: 0

    onExpandedChanged: {
        if (!root.expanded)
            return;
        root.perfOpenCount += 1;
        root.perfFrameMarker = root.perfOpenCount === 1
            ? "AI_USAGE_POPUP_FIRST_FRAME"
            : "AI_USAGE_POPUP_WARM_FRAME";
        perfFrameClock.restart();
    }

    FrameAnimation {
        id: perfFrameClock
        running: false
        onTriggered: {
            const marker = root.perfFrameMarker;
            const elapsed = Math.max(0, Math.round(elapsedTime * 1000));
            stop();
            console.warn(marker + " " + elapsed + " ms");
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: false
        onTriggered: console.warn(
            "AI_USAGE_RUNTIME_STATE " + dependencyController.stateName
            + " " + dependencyController.errorDetail
        )
    }

'''

ACCESSIBLE_ACTIVATION_INSTRUMENTATION = {
    "contents/ui/main.qml": (
        '            onClicked: root.plasmoid["activated"]()\n',
        '            onClicked: root.plasmoid["activated"]()\n'
        '            Accessible.onPressAction: root.plasmoid["activated"]()\n',
    ),
    "contents/ui/CompactRepresentation.qml": (
        "    onClicked: Plasmoid.activated()\n",
        "    onClicked: Plasmoid.activated()\n"
        "    Accessible.onPressAction: Plasmoid.activated()\n",
    ),
}


def write_isolated_session_config(config_root: Path) -> None:
    """Disable unrelated first-run and update work in a temporary Plasma profile."""
    autostart_dir = config_root / "autostart"
    autostart_dir.mkdir(parents=True, exist_ok=True)
    (autostart_dir / "org.kde.discover.notifier.desktop").write_text(
        "[Desktop Entry]\nHidden=true\n",
        encoding="utf-8",
    )
    feedback_dir = config_root / "KDE"
    feedback_dir.mkdir(parents=True, exist_ok=True)
    (feedback_dir / "UserFeedback.conf").write_text(
        "[UserFeedback]\nLastEncouragement=2100-01-01T00:00:00Z\n",
        encoding="utf-8",
    )
    (config_root / "plasma-welcomerc").write_text(
        "[General]\nLastSeenVersion=999.0.0\n",
        encoding="utf-8",
    )


def run_in_virtual_outer(arguments: list[str]) -> int:
    """Run the whole ABBA sequence on a headless virtual KWin output."""
    with tempfile.TemporaryDirectory(
        prefix="ai-monitor-v18-phase0-outer-", ignore_cleanup_errors=True
    ) as temporary:
        virtual_root = Path(temporary)
        runtime_dir = virtual_root / "runtime"
        runtime_dir.mkdir(mode=0o700)
        isolated_directories = {
            "HOME": virtual_root / "home",
            "XDG_DATA_HOME": virtual_root / "data",
            "XDG_CONFIG_HOME": virtual_root / "config",
            "XDG_CACHE_HOME": virtual_root / "cache",
            "XDG_STATE_HOME": virtual_root / "state",
        }
        for directory in isolated_directories.values():
            directory.mkdir(mode=0o700)
        write_isolated_session_config(isolated_directories["XDG_CONFIG_HOME"])
        wrapper = virtual_root / "phase0-run"
        command = [sys.executable, str(Path(__file__).resolve()), *arguments]
        wrapper.write_text(
            "#!/usr/bin/env bash\n"
            "set -euo pipefail\n"
            "set +e\n"
            "env AI_USAGE_PHASE0_VIRTUAL=1 QT_IM_MODULE= GTK_IM_MODULE= "
            "QT_VIRTUALKEYBOARD_DISABLE=1 " + shlex.join(command) + "\n"
            "status=$?\n"
            "for service in org.kde.kded6 org.kde.ActivityManager; do\n"
            "  pid=$(busctl --user status \"$service\" 2>/dev/null "
            "| sed -n 's/^PID=//p' | head -n 1)\n"
            "  [[ -n \"$pid\" ]] && kill -TERM \"$pid\" 2>/dev/null || true\n"
            "done\n"
            "exit \"$status\"\n",
            encoding="utf-8",
        )
        wrapper.chmod(0o700)
        environment = os.environ.copy()
        environment.update(
            {
                **{key: str(value) for key, value in isolated_directories.items()},
                "XDG_RUNTIME_DIR": str(runtime_dir),
                "XDG_DATA_DIRS": "/usr/local/share:/usr/share",
                "XDG_CONFIG_DIRS": "/etc/xdg",
                "PAM_KWALLET5_LOGIN": "",
                "SSH_AUTH_SOCK": "",
                "SESSION_MANAGER": "",
                "XAUTHORITY": "",
                "QT_IM_MODULE": "",
                "GTK_IM_MODULE": "",
                "QT_VIRTUALKEYBOARD_DISABLE": "1",
                "QT_NO_XDG_DESKTOP_PORTAL": "1",
                "GTK_USE_PORTAL": "0",
                "AI_USAGE_PHASE0_OUTER_LOG": str(
                    virtual_root / "outer-kwin.log"
                ),
            }
        )
        outer_log = Path(environment["AI_USAGE_PHASE0_OUTER_LOG"])
        with outer_log.open("w", encoding="utf-8") as outer_stream:
            completed = subprocess.run(
                [
                    "dbus-run-session",
                    "--",
                    "kwin_wayland",
                    "--virtual",
                    "--socket",
                    "wayland-v18-phase0",
                    "--width",
                    "1920",
                    "--height",
                    "1200",
                    "--scale",
                    "1",
                    "--no-lockscreen",
                    "--no-global-shortcuts",
                    "--exit-with-session",
                    str(wrapper),
                ],
                cwd=ROOT,
                env=environment,
                stdout=outer_stream,
                stderr=subprocess.STDOUT,
                check=False,
            )
        if completed.returncode != 0:
            print(outer_log.read_text(encoding="utf-8")[-20000:], file=sys.stderr)
        return completed.returncode


def available_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def percentile95(values: list[int]) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot calculate p95 without samples")
    position = 0.95 * (len(ordered) - 1)
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return round(ordered[lower] + (ordered[upper] - ordered[lower]) * fraction, 2)


def trace_summary(path: Path) -> dict[str, Any]:
    events: dict[str, float] = {}
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            event = json.loads(line)
            events[str(event["name"])] = float(event["elapsedMs"])

    def duration(start: str, end: str) -> float | None:
        if start not in events or end not in events:
            return None
        return max(0.0, round(events[end] - events[start], 2))

    return {
        "events": events,
        "qml_component_creation_ms": duration(
            "full_representation_created", "destination_component_loaded"
        ),
        "database_queries_ms": duration("database_init_start", "database_init_end"),
        "guardrail_queries_ms": duration("guardrail_query_start", "guardrail_query_end"),
        "chart_preparation_ms": duration(
            "destination_component_loaded", "first_rendered_frame"
        ),
        "first_rendered_frame_ms": events.get("first_rendered_frame"),
    }


def run_panel_session(
    env: dict[str, str], runtime_root: Path, request_log: Path, trace_path: Path
) -> dict[str, Any]:
    nested_log = runtime_root / "nested-plasma.log"
    session_shell_log = runtime_root / "nested-session-shell.log"
    binding_template = runtime_root / "bind-outer-window.template.js"
    binding_script = runtime_root / "bind-outer-window.js"
    binding_template.write_text(
        "const targetPid = __TARGET_PID__;\n"
        "function bindWindow(window) {\n"
        "    if (Number(window.pid) !== targetPid) return;\n"
        "    workspace.activeWindow = window;\n"
        "    callDBus(\n"
        "        'com.github.loofi.aiusagemonitor.Phase0Binding',\n"
        "        '/Phase0Binding',\n"
        "        'com.github.loofi.aiusagemonitor.Phase0Binding',\n"
        "        'WindowBound',\n"
        "        String(window.internalId), String(window.pid)\n"
        "    );\n"
        "}\n"
        "workspace.windowList().forEach(bindWindow);\n"
        "workspace.windowAdded.connect(bindWindow);\n",
        encoding="utf-8",
    )
    measurement_log = runtime_root / "panel-measurement.json"
    measurement_error = runtime_root / "panel-measurement.log"
    socket_name = f"wayland-ai-monitor-v18-{os.getpid()}-{time.time_ns()}"
    panel_script = (
        "var existing=panels();"
        "for(var i=0;i<existing.length;++i)existing[i].remove();"
        "var panel=new Panel;"
        'panel.location="bottom";'
        "panel.height=58;"
        'panel.addWidget("com.github.loofi.aiusagemonitor");'
        'panel.addWidget("org.kde.plasma.panelspacer");'
        'panel.addWidget("org.kde.plasma.digitalclock");'
    )
    inner = r"""
set -euo pipefail
export QT_IM_MODULE=
export GTK_IM_MODULE=
export QT_VIRTUALKEYBOARD_DISABLE=1
/usr/libexec/at-spi-bus-launcher --launch-immediately >>"$4" 2>&1 &
a11y_pid=$!
sleep 0.25
/usr/libexec/at-spi2-registryd --use-gnome-session >>"$4" 2>&1 &
a11y_registry_pid=$!
setsid kwin_wayland --wayland-display "$OUTER_WAYLAND_DISPLAY" -s "$1" \
  --width 1600 --height 900 --scale 1 --xwayland --no-lockscreen \
  --no-global-shortcuts --exit-with-session /usr/bin/plasmashell >>"$4" 2>&1 &
kwin_pid=$!
binding_pid=""
plasmashell_pid=""
cleanup() {
  for service in org.kde.kded6 org.kde.ActivityManager; do
    service_pid="$(busctl --user status "$service" 2>/dev/null \
      | sed -n 's/^PID=//p' | head -n 1)"
    [[ -n "$service_pid" ]] && kill -TERM "$service_pid" >/dev/null 2>&1 || true
  done
  [[ -n "$binding_pid" ]] && kill -TERM "$binding_pid" >/dev/null 2>&1 || true
  [[ -n "$plasmashell_pid" ]] \
    && kill -TERM "$plasmashell_pid" >/dev/null 2>&1 || true
  for _ in $(seq 1 40); do
    kill -0 "$kwin_pid" >/dev/null 2>&1 || break
    sleep 0.05
  done
  if kill -0 "$kwin_pid" >/dev/null 2>&1; then
    kill -TERM -- "-$kwin_pid" >/dev/null 2>&1 || true
    for _ in $(seq 1 20); do
      kill -0 "$kwin_pid" >/dev/null 2>&1 || break
      sleep 0.05
    done
  fi
  kill -0 "$kwin_pid" >/dev/null 2>&1 \
    && kill -KILL -- "-$kwin_pid" >/dev/null 2>&1 || true
  wait "$kwin_pid" >/dev/null 2>&1 || true
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
[[ "$ready" -eq 1 ]] || { echo "Nested Plasma panel did not become ready" >&2; exit 1; }
sleep 2
sed "s/__TARGET_PID__/$kwin_pid/" "$8" >"$9"
binding_output="${9}.bound"
binding_ready="${9}.ready"
DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  python3 "${10}" --pid "$kwin_pid" --output "$binding_output" \
  --ready "$binding_ready" >>"$4" 2>&1 &
binding_pid=$!
for _ in $(seq 1 40); do
  [[ -s "$binding_ready" ]] && break
  kill -0 "$binding_pid" >/dev/null 2>&1 || {
    echo "Outer window binding service exited before readiness" >&2
    exit 1
  }
  sleep 0.05
done
[[ -s "$binding_ready" ]] || {
  echo "Outer window binding service did not become ready" >&2
  exit 1
}
DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
  unloadScript s ai-monitor-phase0-window-binding >/dev/null 2>&1 || true
load_output="$(DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
  loadScript ss "$9" ai-monitor-phase0-window-binding 2>/dev/null || true)"
rg -q '^i [0-9]+$' <<<"$load_output" || {
  echo "Outer KWin could not load the PID-bound window script: $load_output" >&2
  exit 1
}
DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
  start >/dev/null
window_id=""
window_info=""
for _ in $(seq 1 80); do
  if [[ -s "$binding_output" ]]; then
    read -r window_id bound_pid <"$binding_output"
    [[ "$bound_pid" == "$kwin_pid" ]] || window_id=""
  fi
  if [[ -n "$window_id" ]]; then
    window_info="$(DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
      busctl --user call org.kde.KWin /KWin org.kde.KWin \
      getWindowInfo s "$window_id" 2>/dev/null || true)"
    if rg -q "\"pid\" [a-z]+ $kwin_pid([[:space:]]|$)" <<<"$window_info"; then
      break
    fi
    window_id=""
  fi
  sleep 0.25
done
DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
  unloadScript s ai-monitor-phase0-window-binding >/dev/null 2>&1 || true
[[ -n "$window_id" ]] || {
  echo "Outer KWin could not bind nested compositor PID $kwin_pid" >&2
  exit 1
}
printf 'AI_USAGE_OUTER_WINDOW_INFO %s\n' "$window_info" >>"$4"
plasmashell_pid="$(pgrep -P "$kwin_pid" -x plasmashell | head -n 1 || true)"
if [[ -z "$plasmashell_pid" ]]; then
  plasmashell_pid="$(pgrep -n -x plasmashell)"
fi
[[ -n "$plasmashell_pid" ]] || { echo "Nested plasmashell PID not found" >&2; exit 1; }
timeout 55s python3 "$3" --request-log "$5" --session-log "$4" \
  --pid "$plasmashell_pid" >"$6" 2>"$7"
"""
    env = dict(env)
    env["PLASMA_AI_MONITOR_PERF_TRACE"] = str(trace_path)
    with session_shell_log.open("w", encoding="utf-8") as session_stream:
        result = subprocess.run(
            [
                "dbus-run-session", "--", "bash", "-c", inner, "v18-panel",
                socket_name, panel_script,
                str(ROOT / "scripts/demo/measure_panel_accessible.py"),
                str(nested_log), str(request_log), str(measurement_log),
                str(measurement_error), str(binding_template),
                str(binding_script),
                str(ROOT / "scripts/demo/kwin_window_binding_service.py"),
            ],
            cwd=ROOT,
            env=env,
            text=True,
            stdout=session_stream,
            stderr=subprocess.STDOUT,
            timeout=120,
            check=False,
        )
    if result.returncode != 0 or not measurement_log.exists():
        detail = f"Panel measurement failed with exit status {result.returncode}"
        session_output = session_shell_log.read_text(encoding="utf-8").strip()
        if session_output:
            detail += "\nSession output:\n" + session_output
        if measurement_error.exists():
            measurement_error_text = measurement_error.read_text(
                encoding="utf-8"
            ).strip()
            if measurement_error_text:
                detail += "\n" + measurement_error_text
        if measurement_log.exists():
            measurement_output = measurement_log.read_text(
                encoding="utf-8"
            ).strip()
            if measurement_output:
                detail += "\nProbe stdout:\n" + measurement_output
        if nested_log.exists():
            detail += "\n" + nested_log.read_text(encoding="utf-8")[-20000:]
        raise RuntimeError(detail.strip())
    measurement = json.loads(measurement_log.read_text(encoding="utf-8"))
    measurement["trace"] = trace_summary(trace_path)
    return measurement


def installed_environment(build_dir: Path, runtime_root: Path) -> dict[str, str]:
    env = candidate_environment(build_dir.resolve(), runtime_root)
    write_isolated_session_config(runtime_root / "config")
    installed = list(
        (runtime_root / "prefix").glob(
            "share/plasma/plasmoids/com.github.loofi.aiusagemonitor/"
            "contents/ui/main.qml"
        )
    )
    if len(installed) != 1:
        raise RuntimeError("installed main.qml was not found")
    main_qml = installed[0]
    source = main_qml.read_text(encoding="utf-8")
    closing = source.rfind("}")
    if closing < 0 or "AI_USAGE_POPUP_FIRST_FRAME" in source:
        raise RuntimeError("popup instrumentation target is invalid")
    main_qml.write_text(
        source[:closing] + POPUP_INSTRUMENTATION + source[closing:],
        encoding="utf-8",
    )
    plasmoid_root = main_qml.parents[2]
    for relative_path, (target, replacement) in (
        ACCESSIBLE_ACTIVATION_INSTRUMENTATION.items()
    ):
        qml_path = plasmoid_root / relative_path
        qml_source = qml_path.read_text(encoding="utf-8")
        if qml_source.count(target) != 1 or "Accessible.onPressAction" in qml_source:
            raise RuntimeError(
                f"accessible activation instrumentation target is invalid: {qml_path}"
            )
        qml_path.write_text(
            qml_source.replace(target, replacement, 1), encoding="utf-8"
        )
    outer_display = os.environ.get("WAYLAND_DISPLAY", "wayland-0")
    if not outer_display.startswith("/"):
        outer_display = str(
            Path(os.environ["XDG_RUNTIME_DIR"]) / outer_display
        )
    env.update(
        {
            "OUTER_WAYLAND_DISPLAY": outer_display,
            "OUTER_DBUS_SESSION_BUS_ADDRESS": os.environ.get("DBUS_SESSION_BUS_ADDRESS", ""),
            "PLASMA_AI_MONITOR_DEMO": "1",
            "AIUSAGE_MONITOR_CATALOG_DIR": str(
                runtime_root
                / "prefix/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/contents/catalog"
            ),
            "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
            "QT_ACCESSIBILITY": "1",
            "QT_ASSUME_STDERR_HAS_CONSOLE": "1",
            "QT_FORCE_STDERR_LOGGING": "1",
        }
    )
    return env


def run_sample(
    variant: str,
    env: dict[str, str],
    sample_root: Path,
    sample: int,
    warmup: bool,
) -> dict[str, Any]:
    sample_root.mkdir(mode=0o700, parents=True)
    runtime_dir = sample_root / "runtime"
    runtime_dir.mkdir(mode=0o700)
    port = available_port()
    request_log = sample_root / "requests.jsonl"
    trace_path = sample_root / "performance.jsonl"
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
        sample_env = dict(env)
        sample_env["XDG_RUNTIME_DIR"] = str(runtime_dir)
        sample_env["PLASMA_AI_MONITOR_DEMO_BASE_URL"] = (
            f"http://127.0.0.1:{port}"
        )
        time.sleep(0.25)
        if server.poll() is not None:
            raise RuntimeError("Mock server failed to start")
        result = run_panel_session(
            sample_env, sample_root, request_log, trace_path
        )
        result.update(
            {
                "variant": variant,
                "sample": sample,
                "warmup": warmup,
                "valid": True,
            }
        )
        return result
    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=3)


def summarize(name: str, samples: list[dict[str, Any]]) -> dict[str, Any]:
    first = [int(item["first_panel_popup_ms"]) for item in samples]
    warm = [int(item["warm_panel_popup_ms"]) for item in samples]
    return {
        "variant": name,
        "samples": samples,
        "first_ms": first,
        "first_median_ms": statistics.median(first),
        "first_p95_ms": percentile95(first),
        "warm_ms": warm,
        "warm_median_ms": statistics.median(warm),
        "warm_p95_ms": percentile95(warm),
        "startup_network_requests": [item["startup_network_requests"] for item in samples],
        "fresh_popup_network_requests": [item["fresh_popup_network_requests"] for item in samples],
    }


def main() -> None:
    if os.environ.get("AI_USAGE_PHASE0_VIRTUAL") != "1":
        raise SystemExit(run_in_virtual_outer(sys.argv[1:]))

    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-build-dir", type=Path, required=True)
    parser.add_argument("--candidate-build-dir", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=DEFAULT_RUNS)
    parser.add_argument("--warmups", type=int, default=DEFAULT_WARMUPS)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--baseline-commit", required=True)
    parser.add_argument("--candidate-commit", required=True)
    args = parser.parse_args()
    if args.runs < 1 or args.warmups < 0:
        parser.error("--runs must be positive and --warmups non-negative")
    missing = [name for name in ("bash", "busctl", "dbus-run-session", "kwin_wayland", "pgrep", "plasmashell", "rg", "rpm", "sed", "setsid", "timeout") if shutil.which(name) is None]
    missing += [str(path) for path in (Path("/usr/libexec/at-spi-bus-launcher"), Path("/usr/libexec/at-spi2-registryd")) if not path.is_file()]
    if missing:
        parser.error("Missing commands: " + ", ".join(missing))

    baseline_commit = subprocess.run(
        ["git", "rev-parse", f"{args.baseline_commit}^{{commit}}"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        check=True,
    ).stdout.strip()
    candidate_commit = subprocess.run(
        ["git", "rev-parse", f"{args.candidate_commit}^{{commit}}"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        check=True,
    ).stdout.strip()
    tagged_v17_commit = subprocess.run(
        ["git", "rev-parse", "v17.0.0^{commit}"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        check=True,
    ).stdout.strip()
    current_commit = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        check=True,
    ).stdout.strip()
    if baseline_commit != tagged_v17_commit:
        parser.error("baseline commit must resolve to the exact annotated v17.0.0 tag")
    if candidate_commit != current_commit:
        parser.error("candidate commit must resolve to the current clean checkout")
    if subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT,
        text=True, stdout=subprocess.PIPE, check=True,
    ).stdout:
        parser.error("candidate checkout must be clean before final ABBA measurement")

    boot_id = Path("/proc/sys/kernel/random/boot_id").read_text(encoding="utf-8").strip()
    measured: dict[str, list[dict[str, Any]]] = {"baseline": [], "candidate": []}
    warmup_samples: list[dict[str, Any]] = []
    errors: list[dict[str, Any]] = []
    sequence: list[str] = []
    with tempfile.TemporaryDirectory(
        prefix="ai-monitor-v18-phase0-samples-", ignore_cleanup_errors=True
    ) as temporary:
        comparison_root = Path(temporary)
        variant_roots = {
            "baseline": comparison_root / "baseline",
            "candidate": comparison_root / "candidate",
        }
        environments = {
            "baseline": installed_environment(
                args.baseline_build_dir, variant_roots["baseline"]
            ),
            "candidate": installed_environment(
                args.candidate_build_dir, variant_roots["candidate"]
            ),
        }
        total = args.warmups + args.runs
        for index in range(total):
            order = (
                ("baseline", "candidate")
                if index % 2 == 0
                else ("candidate", "baseline")
            )
            for variant in order:
                sequence.append(variant)
                sample_root = (
                    variant_roots[variant]
                    / "samples"
                    / f"sample-{index:02d}"
                )
                try:
                    item = run_sample(
                        variant,
                        environments[variant],
                        sample_root,
                        index,
                        index < args.warmups,
                    )
                    if index < args.warmups:
                        warmup_samples.append(item)
                    else:
                        measured[variant].append(item)
                except Exception as error:  # invalid samples remain evidence
                    errors.append(
                        {
                            "variant": variant,
                            "sample": index,
                            "error": str(error),
                        }
                    )

    valid = not errors and all(len(rows) == args.runs for rows in measured.values())
    result: dict[str, Any] = {
        "schema": 2,
        "valid": valid,
        "bootId": boot_id,
        "outerSession": os.environ.get("XDG_SESSION_ID", ""),
        "environment": {
            "platform": platform.platform(),
            "plasma": "plasma-workspace " + subprocess.run(
                ["rpm", "-q", "--qf", "%{VERSION}-%{RELEASE}", "plasma-workspace"],
                text=True,
                stdout=subprocess.PIPE,
                check=True,
            ).stdout.strip(),
        },
        "commits": {"baseline": baseline_commit, "candidate": candidate_commit},
        "instrumentationSha256": hashlib.sha256(
            (
                POPUP_INSTRUMENTATION
                + json.dumps(
                    ACCESSIBLE_ACTIVATION_INSTRUMENTATION, sort_keys=True
                )
            ).encode()
        ).hexdigest(),
        "buildDirectories": {
            "baseline": str(args.baseline_build_dir.resolve()),
            "candidate": str(args.candidate_build_dir.resolve()),
        },
        "runs": args.runs,
        "warmups": args.warmups,
        "runtimeReuse": "isolated-prefix-config-cache-per-variant",
        "sequence": sequence,
        "warmupSamples": warmup_samples,
        "errors": errors,
    }
    if valid:
        baseline = summarize("baseline", measured["baseline"])
        candidate = summarize("candidate", measured["candidate"])
        gates = {
            "first_median_max_125": candidate["first_median_ms"] <= 125,
            "warm_median_max_125": candidate["warm_median_ms"] <= 125,
            "first_p95_max_180": candidate["first_p95_ms"] <= 180,
            "warm_p95_max_180": candidate["warm_p95_ms"] <= 180,
            "first_no_regression": candidate["first_median_ms"] <= baseline["first_median_ms"],
            "warm_no_regression": candidate["warm_median_ms"] <= baseline["warm_median_ms"],
            "fresh_popup_zero_network": all(
                value == 0
                for value in candidate["fresh_popup_network_requests"]
                + baseline["fresh_popup_network_requests"]
            ),
            "startup_network_matches_baseline": (
                candidate["startup_network_requests"]
                == baseline["startup_network_requests"]
            ),
            "candidate_trace_complete": all(
                {
                    "full_representation_created",
                    "destination_component_loaded",
                    "database_init_start",
                    "database_init_end",
                    "guardrail_query_start",
                    "guardrail_query_end",
                    "first_rendered_frame",
                }.issubset(item["trace"]["events"])
                for item in candidate["samples"]
            ),
        }
        result.update({"baseline": baseline, "candidate": candidate, "gates": gates, "passed": all(gates.values())})
    else:
        result["passed"] = False
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
