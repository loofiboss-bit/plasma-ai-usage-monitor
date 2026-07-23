#!/usr/bin/env python3
"""Measure reproducible Phase 7 plasmawindowed runtime signals."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLASMOID_ID = "com.github.loofi.aiusagemonitor"


def window_visible() -> bool:
    result = subprocess.run(
        [
            "busctl",
            "--user",
            "call",
            "org.kde.KWin",
            "/WindowsRunner",
            "org.kde.krunner1",
            "Match",
            "s",
            "AI Usage Monitor",
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0 and "AI Usage Monitor" in result.stdout


def wait_for_window(timeout: float) -> int:
    started = time.monotonic()
    deadline = started + timeout
    while time.monotonic() < deadline:
        if window_visible():
            return round((time.monotonic() - started) * 1000)
        time.sleep(0.025)
    raise RuntimeError("plasmawindowed did not expose the widget window")


def process_stats(pid: int) -> tuple[int, int]:
    status = Path(f"/proc/{pid}/status").read_text(encoding="utf-8")
    rss_kib = 0
    for line in status.splitlines():
        if line.startswith("VmRSS:"):
            rss_kib = int(line.split()[1])
            break
    stat = Path(f"/proc/{pid}/stat").read_text(encoding="utf-8").split()
    ticks = int(stat[13]) + int(stat[14])
    return rss_kib, ticks


def wait_accessible(target: str, timeout: float, env: dict[str, str]) -> int:
    result = subprocess.run(
        [
            "python3",
            str(ROOT / "scripts/demo/wait_accessible.py"),
            "--window",
            "AI Usage Monitor",
            "--target",
            target,
            "--timeout",
            str(timeout),
        ],
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return int(result.stdout.strip().split("=", 1)[1])


def terminate(process: subprocess.Popen[str]) -> None:
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline and window_visible():
        time.sleep(0.05)


def candidate_environment(
    build_dir: Path, runtime_root: Path
) -> dict[str, str]:
    prefix = runtime_root / "prefix"
    subprocess.run(
        ["cmake", "--install", str(build_dir), "--prefix", str(prefix)],
        check=True,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
    )
    qml_paths = list(prefix.glob("**/qt6/qml"))
    if not qml_paths:
        raise RuntimeError("candidate QML module was not installed")
    env = os.environ.copy()
    env.update(
        {
            "XDG_DATA_HOME": str(prefix / "share"),
            "XDG_DATA_DIRS": (
                f"{prefix / 'share'}:"
                f"{env.get('XDG_DATA_DIRS', '/usr/local/share:/usr/share')}"
            ),
            "XDG_CONFIG_HOME": str(runtime_root / "config"),
            "XDG_CACHE_HOME": str(runtime_root / "cache"),
            "QML2_IMPORT_PATH": str(qml_paths[0]),
        }
    )
    return env


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode", choices=("installed", "candidate"), required=True
    )
    parser.add_argument("--build-dir", type=Path, default=Path("build/release"))
    parser.add_argument(
        "--view", choices=("overview", "history", "analyst"), default="overview"
    )
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--idle-seconds", type=int, default=15)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.runs < 1 or args.idle_seconds < 1:
        parser.error("--runs and --idle-seconds must be positive")
    if shutil.which("plasmawindowed") is None or shutil.which("busctl") is None:
        parser.error("plasmawindowed and busctl are required")

    with tempfile.TemporaryDirectory(prefix="ai-monitor-phase7-") as temporary:
        runtime_root = Path(temporary)
        env = os.environ.copy()
        if args.mode == "candidate":
            env = candidate_environment(args.build_dir.resolve(), runtime_root)
        env.update(
            {
                "PLASMA_AI_MONITOR_DEMO": "1",
                "PLASMA_AI_MONITOR_SMOKE_VIEW": args.view,
                "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
                "QT_ACCESSIBILITY": "1",
            }
        )

        startup_ms: list[int] = []
        ready_ms: list[int] = []
        rss_samples: list[int] = []
        cpu_percent = 0.0
        for run in range(args.runs):
            if window_visible():
                raise RuntimeError(
                    "another AI Usage Monitor plasmawindowed instance is visible"
                )
            process = subprocess.Popen(
                ["plasmawindowed", PLASMOID_ID],
                env=env,
                text=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            try:
                startup_ms.append(wait_for_window(15))
                if args.mode == "candidate" and args.view in {"history", "analyst"}:
                    target = (
                        "History view ready"
                        if args.view == "history"
                        else "Analyst view ready"
                    )
                    ready_ms.append(wait_accessible(target, 15, env))
                if run == args.runs - 1:
                    rss_start, ticks_start = process_stats(process.pid)
                    rss_samples.append(rss_start)
                    for _ in range(args.idle_seconds):
                        time.sleep(1)
                        rss_samples.append(process_stats(process.pid)[0])
                    _, ticks_end = process_stats(process.pid)
                    ticks_per_second = os.sysconf("SC_CLK_TCK")
                    cpu_percent = round(
                        ((ticks_end - ticks_start) / ticks_per_second)
                        / args.idle_seconds
                        * 100,
                        3,
                    )
            finally:
                terminate(process)

        result = {
            "schema": 1,
            "mode": args.mode,
            "view": args.view,
            "runs": args.runs,
            "environment": {
                "os": platform.platform(),
                "plasma": subprocess.run(
                    ["plasmashell", "--version"],
                    text=True,
                    stdout=subprocess.PIPE,
                    check=False,
                ).stdout.strip(),
                "qt": subprocess.run(
                    ["qmake6", "--version"],
                    text=True,
                    stdout=subprocess.PIPE,
                    check=False,
                ).stdout.splitlines()[-1].strip(),
            },
            "startup_ms": startup_ms,
            "startup_median_ms": statistics.median(startup_ms),
            "accessible_ready_ms": ready_ms,
            "accessible_ready_median_ms": (
                statistics.median(ready_ms) if ready_ms else None
            ),
            "idle_seconds": args.idle_seconds,
            "idle_cpu_percent": cpu_percent,
            "rss_kib": {
                "first": rss_samples[0],
                "last": rss_samples[-1],
                "minimum": min(rss_samples),
                "maximum": max(rss_samples),
                "growth": rss_samples[-1] - rss_samples[0],
            },
        }

    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
