#!/usr/bin/env python3
"""Extend the existing exact-PID popup probe with bounded process sampling."""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "demo"))
import measure_panel_accessible as probe
from measure_phase7_runtime import process_stats


def sample_process(pid: int, seconds: int) -> dict:
    started = time.monotonic()
    rss, ticks = process_stats(pid)
    samples = [{"elapsedSeconds": 0.0, "rssKiB": rss, "cpuTicks": ticks}]
    for index in range(1, seconds + 1):
        time.sleep(max(0.0, started + index - time.monotonic()))
        rss, ticks = process_stats(pid)
        samples.append(
            {
                "elapsedSeconds": round(time.monotonic() - started, 4),
                "rssKiB": rss,
                "cpuTicks": ticks,
            }
        )
    elapsed = time.monotonic() - started
    cpu = (samples[-1]["cpuTicks"] - samples[0]["cpuTicks"]) / os.sysconf("SC_CLK_TCK")
    return {
        "durationSeconds": elapsed,
        "cpuPercentOneCore": cpu / elapsed * 100,
        "rssGrowthKiB": samples[-1]["rssKiB"] - samples[0]["rssKiB"],
        "rssMinKiB": min(row["rssKiB"] for row in samples),
        "rssMaxKiB": max(row["rssKiB"] for row in samples),
        "samples": samples,
    }


def wait_for_boot_requests(path: Path, timeout: float = 35.0) -> dict:
    """Wait for this fixed demo's four immediate replies, then one quiet second."""
    started = time.monotonic()
    changed = started
    previous = probe.request_count(path)
    while time.monotonic() - started < timeout:
        current = probe.request_count(path)
        if current != previous:
            previous = current
            changed = time.monotonic()
        quiet = time.monotonic() - changed
        if current >= 4 and quiet >= 1.0:
            return {
                "requestCount": current,
                "quietSeconds": quiet,
                "waitSeconds": time.monotonic() - started,
            }
        time.sleep(0.025)
    raise RuntimeError(
        f"Demo boot requests did not settle: count={previous}, expected at least four"
    )


def main() -> None:
    seconds = int(os.environ.get("AI_USAGE_IDLE_SECONDS", "0"))
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-log", type=Path, required=True)
    parser.add_argument("--session-log", type=Path, required=True)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--timeout", type=float, default=35.0)
    args = parser.parse_args()
    prefix = "AI Usage Monitor:"
    probe.wait_for_node(prefix, args.pid, args.timeout)
    probe.wait_for_log_text(
        args.session_log, "AI_USAGE_RUNTIME_STATE ready", args.timeout
    )
    boot_settled = wait_for_boot_requests(args.request_log, args.timeout)
    startup_requests = probe.request_count(args.request_log)
    closed = sample_process(args.pid, seconds) if seconds else None
    before = probe.request_count(args.request_log)
    first = probe.open_popup(prefix, args.pid, args.session_log, args.timeout)
    time.sleep(1)
    after = probe.request_count(args.request_log)
    opened = sample_process(args.pid, seconds) if seconds else None
    probe.close_popup(prefix, args.pid, args.timeout)
    compact = probe.wait_for_node(prefix, args.pid, args.timeout)
    probe.focus(compact)
    offset = args.session_log.stat().st_size
    probe.press(compact)
    warm = probe.wait_for_log_duration(
        args.session_log, "AI_USAGE_POPUP_WARM_FRAME", offset, args.timeout
    )
    probe.wait_for_popup(args.pid, args.timeout)
    result = {
        "first_panel_popup_ms": first,
        "warm_panel_popup_ms": warm,
        "startup_network_requests": startup_requests,
        "boot_requests_settled": boot_settled,
        "popup_request_count_before": before,
        "popup_request_count_after": after,
        "fresh_popup_network_requests": after - before if not seconds else None,
        "network_measurement_kind": "fresh_after_boot"
        if not seconds
        else "after_closed_idle",
    }
    if seconds:
        result.update(
            post_idle_popup_network_requests=after - before,
            idleClosed=closed,
            idleOpen=opened,
        )
    print(json.dumps(result), flush=True)
    os._exit(0)


if __name__ == "__main__":
    main()
