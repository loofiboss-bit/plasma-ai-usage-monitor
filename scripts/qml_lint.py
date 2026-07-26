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
PLASMA_I18N_FUNCTIONS = frozenset(("i18n", "i18nc", "i18np", "i18ncp"))


def is_plasma_i18n_warning(file_result: dict, warning: dict) -> bool:
    """Recognize qmllint's false positive for Plasma-injected i18n functions."""
    if warning.get("id") != "unqualified":
        return False

    filename = file_result.get("filename")
    offset = warning.get("charOffset")
    length = warning.get("length")
    if not filename or not isinstance(offset, int) or not isinstance(length, int):
        return False

    try:
        source = Path(filename).read_text(encoding="utf-8")
    except OSError:
        return False
    return source[offset : offset + length] in PLASMA_I18N_FUNCTIONS


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
        "1000000",
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
    warnings = []
    ignored = 0
    for file_result in payload.get("files", []):
        kept = []
        for warning in file_result.get("warnings", []):
            if (
                warning.get("type") != "info"
                and is_plasma_i18n_warning(file_result, warning)
            ):
                ignored += 1
                continue
            kept.append(warning)
            if warning.get("type") != "info":
                warnings.append(warning)
        file_result["warnings"] = kept
    report.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    if result.returncode != 0 or warnings:
        print(
            f"qmllint failed: {len(warnings)} warnings; report={report}",
            file=sys.stderr,
        )
        return result.returncode or 1
    print(
        f"qmllint OK: {len(files)} files, 0 warnings "
        f"({ignored} Plasma i18n context false positives ignored); "
        f"report={report}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
