#!/usr/bin/env python3
"""Repeat existing deterministic 100k-event fixtures without host user data."""

from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

from benchmark_environment import isolated_environment


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    fixtures = {
        "historyAnalyst100k": (
            "test_phase7_performance",
            "databaseQueriesMeetReleaseBudgets",
        ),
        "sessionQueries100k": (
            "test_localsessionmodel",
            "queriesHandleOneHundredThousandUsageEvents",
        ),
        "sessionExport100k": (
            "test_localsessionmodel",
            "streamsOneHundredThousandExportEventsAtomically",
        ),
    }
    result = {
        "schema": 1,
        "buildDirectory": str(args.build_dir.resolve()),
        "runs": args.runs,
        "conditions": "Release fixture test wall time includes fixture seeding/assertions; phase7 metrics time queries only",
        "fixtures": {},
    }
    for name, (binary, function) in fixtures.items():
        samples = []
        for run in range(args.runs):
            with tempfile.TemporaryDirectory(prefix="ai-monitor-scale-") as temporary:
                env = isolated_environment(Path(temporary))
                started = time.monotonic()
                completed = subprocess.run(
                    [
                        "dbus-run-session",
                        "--",
                        str(args.build_dir.resolve() / "plugin/tests" / binary),
                        function,
                    ],
                    env=env,
                    check=False,
                    text=True,
                    capture_output=True,
                    timeout=180,
                )
                sample = {
                    "wallMs": round((time.monotonic() - started) * 1000, 3),
                    "returnCode": completed.returncode,
                    "stdout": completed.stdout,
                    "stderr": completed.stderr,
                    "loadAverage": os.getloadavg(),
                }
                sample["queryMetrics"] = [
                    dict(re.findall(r"(\w+)=(\d+)", line))
                    for line in completed.stdout.splitlines()
                    if "PHASE7_METRIC" in line
                ]
                samples.append(sample)
        result["fixtures"][name] = {
            "samples": samples,
            "medianWallMs": statistics.median(row["wallMs"] for row in samples),
            "passed": all(row["returnCode"] == 0 for row in samples),
        }
    result["passed"] = all(row["passed"] for row in result["fixtures"].values())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "passed": result["passed"],
                "medianWallMs": {
                    name: row["medianWallMs"]
                    for name, row in result["fixtures"].items()
                },
            },
            indent=2,
        )
    )
    raise SystemExit(0 if result["passed"] else 1)


if __name__ == "__main__":
    main()
