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

    Connections {
        target: Plasmoid

        function recordFirstFrame() {
            Qt.callLater(function() {
                console.warn("AI_USAGE_POPUP_FIRST_FRAME");
            });
        }

        function onExpandedChanged() {
            if (Plasmoid.expanded) {
                recordFirstFrame();
            } else {
                console.warn("AI_USAGE_POPUP_CLOSED");
            }
        }

        Component.onCompleted: {
            if (Plasmoid.expanded) recordFirstFrame();
        }
    }
'''


def run_in_virtual_outer(arguments: list[str]) -> int:
    """Run the whole ABBA sequence on a headless virtual KWin output."""
    with tempfile.TemporaryDirectory(
        prefix="ai-monitor-v18-phase0-outer-", ignore_cleanup_errors=True
    ) as temporary:
        virtual_root = Path(temporary)
        runtime_dir = virtual_root / "runtime"
        runtime_dir.mkdir(mode=0o700)
        wrapper = virtual_root / "phase0-run"
        command = [sys.executable, str(Path(__file__).resolve()), *arguments]
        wrapper.write_text(
            "#!/usr/bin/env bash\n"
            "set -euo pipefail\n"
            "exec env AI_USAGE_PHASE0_VIRTUAL=1 QT_IM_MODULE= GTK_IM_MODULE= "
            "QT_VIRTUALKEYBOARD_DISABLE=1 " + shlex.join(command) + "\n",
            encoding="utf-8",
        )
        wrapper.chmod(0o700)
        environment = os.environ.copy()
        environment.update(
            {
                "XDG_RUNTIME_DIR": str(runtime_dir),
                "QT_IM_MODULE": "",
                "GTK_IM_MODULE": "",
                "QT_VIRTUALKEYBOARD_DISABLE": "1",
                "QT_NO_XDG_DESKTOP_PORTAL": "1",
                "GTK_USE_PORTAL": "0",
            }
        )
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
            check=False,
        )
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
    measurement_log = runtime_root / "panel-measurement.json"
    measurement_error = runtime_root / "panel-measurement.log"
    socket_name = f"wayland-ai-monitor-v18-{os.getpid()}-{time.time_ns()}"
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
  --no-global-shortcuts --exit-with-session /usr/bin/plasmashell >"$4" 2>&1 &
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
[[ "$ready" -eq 1 ]] || { echo "Nested Plasma panel did not become ready" >&2; exit 1; }
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
done < <(printf '%s\n' "$match_output" | rg -o '"0_\{[^\"]+\}"' | tr -d '"' || true)
[[ -n "$match_id" ]] || {
  echo "Outer KWin could not bind the exact nested compositor PID" >&2
  exit 1
}
DBUS_SESSION_BUS_ADDRESS="$OUTER_DBUS_SESSION_BUS_ADDRESS" \
  busctl --user call org.kde.KWin /WindowsRunner org.kde.krunner1 \
  Run ss "$match_id" "" >/dev/null
sleep 1
plasmashell_pid="$(pgrep -P "$kwin_pid" -x plasmashell | head -n 1 || true)"
if [[ -z "$plasmashell_pid" ]]; then
  plasmashell_pid="$(pgrep -n -x plasmashell)"
fi
[[ -n "$plasmashell_pid" ]] || { echo "Nested plasmashell PID not found" >&2; exit 1; }
timeout 55s python3 "$3" --request-log "$5" --kwin-log "$4" \
  --pid "$plasmashell_pid" >"$6" 2>"$7"
"""
    env = dict(env)
    env["PLASMA_AI_MONITOR_PERF_TRACE"] = str(trace_path)
    result = subprocess.run(
        [
            "dbus-run-session", "--", "bash", "-c", inner, "v18-panel",
            socket_name, panel_script,
            str(ROOT / "scripts/demo/measure_panel_accessible.py"),
            str(nested_log), str(request_log), str(measurement_log),
            str(measurement_error),
        ],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=120,
        check=False,
    )
    if result.returncode != 0 or not measurement_log.exists():
        detail = measurement_error.read_text(encoding="utf-8").strip() if measurement_error.exists() else ""
        if nested_log.exists():
            detail += "\n" + nested_log.read_text(encoding="utf-8")[-4000:]
        raise RuntimeError(detail.strip() or "Panel measurement returned no JSON")
    measurement = json.loads(measurement_log.read_text(encoding="utf-8"))
    measurement["trace"] = trace_summary(trace_path)
    return measurement


def installed_environment(build_dir: Path, runtime_root: Path) -> dict[str, str]:
    env = candidate_environment(build_dir.resolve(), runtime_root)
    installed = list(
        (runtime_root / "prefix").glob(
            "share/plasma/plasmoids/com.github.loofi.aiusagemonitor/"
            "contents/ui/FullRepresentation.qml"
        )
    )
    if len(installed) != 1:
        raise RuntimeError("installed FullRepresentation.qml was not found")
    full_representation = installed[0]
    source = full_representation.read_text(encoding="utf-8")
    closing = source.rfind("}")
    if closing < 0 or "AI_USAGE_POPUP_FIRST_FRAME" in source:
        raise RuntimeError("popup instrumentation target is invalid")
    full_representation.write_text(
        source[:closing] + POPUP_INSTRUMENTATION + source[closing:],
        encoding="utf-8",
    )
    config_home = Path(env["XDG_CONFIG_HOME"])
    config_home.mkdir(parents=True, exist_ok=True)
    (config_home / "kdeglobals").write_text(
        "[KDE]\nAnimationDurationFactor=0\n", encoding="utf-8"
    )
    env.update(
        {
            "OUTER_WAYLAND_DISPLAY": os.environ.get("WAYLAND_DISPLAY", "wayland-0"),
            "OUTER_DBUS_SESSION_BUS_ADDRESS": os.environ.get("DBUS_SESSION_BUS_ADDRESS", ""),
            "PLASMA_AI_MONITOR_DEMO": "1",
            "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
            "QT_ACCESSIBILITY": "1",
            "KDE_KWIN_ANIMATIONS_ENABLED": "0",
        }
    )
    return env


def run_sample(variant: str, build_dir: Path, sample: int, warmup: bool) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(
        prefix=f"ai-monitor-v18-{variant}-", ignore_cleanup_errors=True
    ) as temporary:
        runtime_root = Path(temporary)
        env = installed_environment(build_dir, runtime_root)
        port = available_port()
        request_log = runtime_root / "requests.jsonl"
        trace_path = runtime_root / "performance.jsonl"
        server = subprocess.Popen(
            ["python3", str(ROOT / "scripts/demo/mock_ai_usage_server.py"), "--port", str(port), "--request-log", str(request_log)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            env["PLASMA_AI_MONITOR_DEMO_BASE_URL"] = f"http://127.0.0.1:{port}"
            time.sleep(0.25)
            if server.poll() is not None:
                raise RuntimeError("Mock server failed to start")
            result = run_panel_session(env, runtime_root, request_log, trace_path)
            result.update({"variant": variant, "sample": sample, "warmup": warmup, "valid": True})
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
    missing = [name for name in ("bash", "busctl", "dbus-run-session", "kwin_wayland", "pgrep", "plasmashell", "rg", "setsid", "timeout") if shutil.which(name) is None]
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
    total = args.warmups + args.runs
    for index in range(total):
        order = ("baseline", "candidate") if index % 2 == 0 else ("candidate", "baseline")
        for variant in order:
            sequence.append(variant)
            build = args.baseline_build_dir if variant == "baseline" else args.candidate_build_dir
            try:
                item = run_sample(variant, build, index, index < args.warmups)
                if index < args.warmups:
                    warmup_samples.append(item)
                else:
                    measured[variant].append(item)
            except Exception as error:  # invalid samples are evidence, never discarded
                errors.append({"variant": variant, "sample": index, "error": str(error)})

    valid = not errors and all(len(rows) == args.runs for rows in measured.values())
    result: dict[str, Any] = {
        "schema": 2,
        "valid": valid,
        "bootId": boot_id,
        "outerSession": os.environ.get("XDG_SESSION_ID", ""),
        "environment": {"platform": platform.platform(), "plasma": subprocess.run(["plasmashell", "--version"], text=True, stdout=subprocess.PIPE, check=False).stdout.strip()},
        "commits": {"baseline": baseline_commit, "candidate": candidate_commit},
        "instrumentationSha256": hashlib.sha256(
            POPUP_INSTRUMENTATION.encode()
        ).hexdigest(),
        "buildDirectories": {
            "baseline": str(args.baseline_build_dir.resolve()),
            "candidate": str(args.candidate_build_dir.resolve()),
        },
        "runs": args.runs,
        "warmups": args.warmups,
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
