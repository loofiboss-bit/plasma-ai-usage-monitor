#!/usr/bin/env python3
"""Mutation tests for the cross-surface version policy."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / "scripts" / "check_version_policy.py"
FILES = (
    "VERSION",
    "ROADMAP.md",
    "SECURITY.md",
    "package/metadata.json",
    "package/contents/catalog/providers-v4.json",
    "package/contents/catalog/subscriptions-v1.json",
    "plasma-ai-usage-monitor.spec",
    "com.github.loofi.aiusagemonitor.metainfo.xml",
    "docs/plans/PLASMA_AI_USAGE_MONITOR_V16_PLAN.md",
)


def run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", str(CHECKER), "--root", str(root)],
        text=True,
        capture_output=True,
        check=False,
    )


with tempfile.TemporaryDirectory(prefix="ai-monitor-version-policy-") as temp:
    fixture = Path(temp)
    for relative in FILES:
        target = fixture / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)

    baseline = run(fixture)
    if baseline.returncode != 0:
        raise SystemExit(baseline.stdout + baseline.stderr)

    mutations = {
        "roadmap": ("ROADMAP.md", "**Current release:** 15.0.0", "**Current release:** 14.0.0"),
        "security": ("SECURITY.md", "| 15.x | Supported |", "| 15.x | Unsupported |"),
        "metadata": ("package/metadata.json", '"Version": "15.0.0"', '"Version": "14.0.0"'),
        "appstream": (
            "com.github.loofi.aiusagemonitor.metainfo.xml",
            '<release version="15.0.0"',
            '<release version="14.0.0"',
        ),
        "rpm": ("plasma-ai-usage-monitor.spec", "Version:        15.0.0", "Version:        14.0.0"),
        "catalog": (
            "package/contents/catalog/providers-v4.json",
            '"release": "15.0.0"',
            '"release": "14.0.0"',
        ),
    }
    for label, (relative, before, after) in mutations.items():
        path = fixture / relative
        original = path.read_text(encoding="utf-8")
        if before not in original:
            raise SystemExit(f"Version policy test fixture missing {label} marker")
        path.write_text(original.replace(before, after, 1), encoding="utf-8")
        result = run(fixture)
        path.write_text(original, encoding="utf-8")
        if result.returncode == 0:
            raise SystemExit(f"Version policy accepted mutated {label}")

print(f"Version policy tests OK: {len(mutations)} release surfaces rejected")
