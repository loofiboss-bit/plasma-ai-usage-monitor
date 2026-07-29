#!/usr/bin/env python3
"""Validate stable and planned versions across every release surface."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"Version policy FAIL: {message}")


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read {path.name}: {error}")


def require_match(text: str, pattern: str, label: str) -> re.Match[str]:
    match = re.search(pattern, text, re.MULTILINE)
    if match is None:
        fail(f"{label} is missing")
    return match


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    args = parser.parse_args()
    root = args.root.resolve()

    version = (root / "VERSION").read_text(encoding="utf-8").strip()
    stable = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
    if stable is None:
        fail(f"VERSION is not stable semantic versioning: {version!r}")
    stable_major = int(stable.group(1))
    roadmap = (root / "ROADMAP.md").read_text(encoding="utf-8")
    roadmap_current = require_match(
        roadmap,
        r"^\*\*Current release:\*\*\s+(\d+\.\d+\.\d+)\b",
        "ROADMAP current release",
    ).group(1)
    if roadmap_current != version:
        fail(f"ROADMAP current release is {roadmap_current}, expected {version}")
    if re.search(rf"^###\s+{re.escape(version)}\b", roadmap, re.MULTILINE) is None:
        fail(f"ROADMAP release section {version} is missing")

    security = (root / "SECURITY.md").read_text(encoding="utf-8")
    if (
        re.search(
            rf"^\|\s*{stable_major}\.x\s*\|\s*Supported\s*\|",
            security,
            re.MULTILINE,
        )
        is None
    ):
        fail(f"SECURITY does not support the current {stable_major}.x major")
    if re.search(rf"^\|\s*{stable_major - 1}\.x and older\s*\|", security, re.MULTILINE) is None:
        fail("SECURITY older-release row does not follow the current major")

    metadata = read_json(root / "package" / "metadata.json")
    direct_versions = {
        "package metadata": metadata.get("KPlugin", {}).get("Version"),
        "required plugin": metadata.get("X-AIUsageMonitor-RequiredPluginVersion"),
        "provider catalog": read_json(
            root / "package" / "contents" / "catalog" / "providers-v4.json"
        ).get("release"),
        "subscription catalog": read_json(
            root / "package" / "contents" / "catalog" / "subscriptions-v1.json"
        ).get("release"),
    }
    for label, value in direct_versions.items():
        if value != version:
            fail(f"{label} is {value!r}, expected {version}")

    spec = (root / "plasma-ai-usage-monitor.spec").read_text(encoding="utf-8")
    spec_version = require_match(
        spec, r"^Version:\s*(\d+\.\d+\.\d+)\s*$", "RPM Version"
    ).group(1)
    if spec_version != version:
        fail(f"RPM Version is {spec_version}, expected {version}")

    metainfo = (
        root / "com.github.loofi.aiusagemonitor.metainfo.xml"
    ).read_text(encoding="utf-8")
    appstream_version = require_match(
        metainfo,
        r"<releases>\s*(?:<!--.*?-->\s*)*<release version=\"(\d+\.\d+\.\d+)\"",
        "latest AppStream release",
    ).group(1)
    if appstream_version != version:
        fail(f"latest AppStream release is {appstream_version}, expected {version}")

    plan_candidates = (
        root / "docs" / "plans" / f"PLASMA_AI_USAGE_MONITOR_V{stable_major}_PLAN.md",
        root / "docs" / "plans" / f"PLASMA_AI_USAGE_MONITOR_V{stable_major}_CODEX_PLAN.md",
    )
    plan_path = next((path for path in plan_candidates if path.is_file()), None)
    if plan_path is None:
        fail(f"canonical v{stable_major} plan is missing")
    plan = plan_path.read_text(encoding="utf-8")
    target_match = require_match(
        plan,
        r"^(?:- \*\*Target release:\*\*\s+`([^`]+)`|\|\s*Target\s*\|\s*`?([^`|]+?)`?\s*\|)",
        f"v{stable_major} plan target",
    )
    target = next(group.strip() for group in target_match.groups() if group).removeprefix("v")
    if target != version:
        fail(f"v{stable_major} plan target is {target}, expected {version}")

    print(
        "Version policy OK: "
        f"release candidate {version}, all release surfaces aligned"
    )


if __name__ == "__main__":
    main()
