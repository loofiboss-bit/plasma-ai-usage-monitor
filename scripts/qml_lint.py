#!/usr/bin/env python3
"""Run repository-owned QML lint and reject parser/type errors."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    qmllint = shutil.which("qmllint") or shutil.which("qmllint-qt6")
    if not qmllint:
        print("qmllint is required", file=sys.stderr)
        return 2
    files = sorted((ROOT / "package/contents/ui").rglob("*.qml"))
    command = [qmllint, "-I", str(ROOT / args.build_dir / "plugin"), *map(str, files)]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    output = result.stdout + result.stderr
    if output:
        print(output, end="")
    # Existing warning categories remain visible while v12 modularization burns
    # them down; syntax, import, and type errors are release-blocking now.
    has_error = any(line.startswith("Error:") for line in output.splitlines())
    return 1 if has_error else 0


if __name__ == "__main__":
    raise SystemExit(main())
