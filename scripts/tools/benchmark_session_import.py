#!/usr/bin/env python3
"""Link the synthetic 100k-event public import probe against existing Release objects."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import statistics
import subprocess
import tempfile
from pathlib import Path

from benchmark_environment import isolated_environment

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    cache = (build / "CMakeCache.txt").read_text()
    source = Path(
        next(
            line.split("=", 1)[1]
            for line in cache.splitlines()
            if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL=")
        )
    )
    objects = sorted(
        (build / "plugin/tests/CMakeFiles/test_localsessionmodel.dir").rglob("*.o")
    )
    objects = [path for path in objects if path.name != "test_localsessionmodel.cpp.o"]
    if len(objects) < 4:
        raise RuntimeError("Build the Release test_localsessionmodel target first")
    flags = shlex.split(
        subprocess.check_output(
            [
                "pkg-config",
                "--cflags",
                "--libs",
                "Qt6Core",
                "Qt6Sql",
                "Qt6Concurrent",
                "Qt6Test",
            ],
            text=True,
        )
    )
    with tempfile.TemporaryDirectory(
        prefix="ai-monitor-import-benchmark-"
    ) as temporary:
        directory = Path(temporary)
        command = [
            "c++",
            "-O3",
            "-DNDEBUG",
            "-std=c++20",
            f"-I{source / 'plugin'}",
            str(ROOT / "scripts/tools/session_import_benchmark.cpp"),
            *[str(path) for path in objects],
            "-o",
            str(directory / "bench"),
            *flags,
        ]
        subprocess.run(command, check=True)
        samples = []
        for run in range(args.runs):
            env = isolated_environment(directory / f"run-{run}")
            result = subprocess.run(
                ["dbus-run-session", "--", str(directory / "bench")],
                text=True,
                capture_output=True,
                env=env,
                timeout=200,
                check=True,
            )
            sample = json.loads(result.stdout.strip().splitlines()[-1])
            sample["loadAverage"] = os.getloadavg()
            sample["stderr"] = result.stderr
            samples.append(sample)
    report = {
        "buildDirectory": str(build),
        "sourceDirectory": str(source),
        "runs": args.runs,
        "samples": samples,
        "medianImportMs": statistics.median(row["importMs"] for row in samples),
        "verifiedRowsPerRun": 100000,
        "conditions": "Timing starts at setToolEnabled, excludes file generation; includes all bounded import cycles and event-count verification",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
