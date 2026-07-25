#!/usr/bin/env python3
"""Run the zero-warning, machine-readable shipped-QML release gate."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--report")
    args = parser.parse_args()
    qmllint = shutil.which("qmllint") or shutil.which("qmllint-qt6")
    if not qmllint:
        print("qmllint is required", file=sys.stderr)
        return 2
    files = sorted((ROOT / "package/contents/ui").rglob("*.qml"))
    module_root = ROOT / args.build_dir / "plugin"
    module_qmldir = (
        module_root
        / "com"
        / "github"
        / "loofi"
        / "aiusagemonitor"
        / "qmldir"
    )
    if not module_qmldir.is_file():
        print(
            f"QML module type information is missing: {module_qmldir}",
            file=sys.stderr,
        )
        return 1
    report = (
        Path(args.report)
        if args.report
        else ROOT / args.build_dir / "qml-lint-report.json"
    )
    report.parent.mkdir(parents=True, exist_ok=True)
    command = [
        qmllint,
        "-I",
        str(module_root),
        "-i",
        str(module_qmldir),
        "--max-warnings",
        "0",
        "--json",
        str(report),
        *map(str, files),
    ]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    output = result.stdout + result.stderr
    if output:
        print(output, end="")
    try:
        payload = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"Cannot read qmllint report: {error}", file=sys.stderr)
        return 1
    warnings = [
        warning
        for file_result in payload.get("files", [])
        for warning in file_result.get("warnings", [])
        if warning.get("type") != "info"
    ]
    if result.returncode != 0 or warnings:
        print(
            f"qmllint failed: {len(warnings)} warnings; report={report}",
            file=sys.stderr,
        )
        return result.returncode or 1
    print(f"qmllint OK: {len(files)} files, 0 warnings; report={report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
