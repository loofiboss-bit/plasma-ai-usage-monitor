#!/usr/bin/env python3
"""Mutation tests for the cross-surface version policy."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

from update_release_version import render


ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / "scripts" / "check_version_policy.py"
BASE_FILES = (
    "VERSION",
    "README.md",
    "ROADMAP.md",
    "SECURITY.md",
    "package/metadata.json",
    "package/contents/catalog/providers-v4.json",
    "package/contents/catalog/subscriptions-v1.json",
    "plasma-ai-usage-monitor.spec",
    "com.github.loofi.aiusagemonitor.metainfo.xml",
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
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    major = int(version.split(".", 1)[0])
    plan_candidates = (
        f"docs/plans/PLASMA_AI_USAGE_MONITOR_V{major}_PLAN.md",
        f"docs/plans/PLASMA_AI_USAGE_MONITOR_V{major}_CODEX_PLAN.md",
    )
    plan = next(
        (relative for relative in plan_candidates if (ROOT / relative).is_file()),
        None,
    )
    if plan is None:
        raise SystemExit(f"Version policy test fixture missing v{major} plan")

    for relative in (*BASE_FILES, plan):
        target = fixture / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)

    baseline = run(fixture)
    if baseline.returncode != 0:
        raise SystemExit(baseline.stdout + baseline.stderr)

    major_part, minor_part, patch_part = map(int, version.split("."))
    next_version = f"{major_part}.{minor_part}.{patch_part + 1}"
    next_readme = render(next_version)[ROOT / "README.md"]
    if (
        f"The **{next_version} release (" not in next_readme
        or f"/releases/tag/v{next_version})" not in next_readme
    ):
        raise SystemExit("Version bump left the README heading or release link stale")

    previous = f"{major - 1}.0.0"
    mutations = {
        "readme heading": ("README.md", f"The **{version} release (", f"The **{previous} release ("),
        "readme link": (
            "README.md",
            f"/releases/tag/v{version})",
            f"/releases/tag/v{previous})",
        ),
        "roadmap": ("ROADMAP.md", f"**Current release:** {version}", f"**Current release:** {previous}"),
        "security": ("SECURITY.md", f"| {major}.x | Supported |", f"| {major}.x | Unsupported |"),
        "metadata": ("package/metadata.json", f'"Version": "{version}"', f'"Version": "{previous}"'),
        "appstream": (
            "com.github.loofi.aiusagemonitor.metainfo.xml",
            f'<release version="{version}"',
            f'<release version="{previous}"',
        ),
        "rpm": ("plasma-ai-usage-monitor.spec", f"Version:        {version}", f"Version:        {previous}"),
        "catalog": (
            "package/contents/catalog/providers-v4.json",
            f'"release": "{version}"',
            f'"release": "{previous}"',
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

    appstream_path = fixture / "com.github.loofi.aiusagemonitor.metainfo.xml"
    original_appstream = appstream_path.read_text(encoding="utf-8")

    commented = original_appstream.replace(
        f'<release version="{version}"',
        f'<!-- Release notes review -->\n    <release version="{version}"',
        1,
    )
    appstream_path.write_text(commented, encoding="utf-8")
    result = run(fixture)
    appstream_path.write_text(original_appstream, encoding="utf-8")
    if result.returncode != 0:
        raise SystemExit(f"Version policy rejected AppStream release preceded by comments: {result.stderr}")

    appstream_path.write_text("<component><releases>", encoding="utf-8")
    result = run(fixture)
    appstream_path.write_text(original_appstream, encoding="utf-8")
    if result.returncode == 0:
        raise SystemExit("Version policy accepted malformed AppStream XML")

print(f"Version policy tests OK: {len(mutations)} release surfaces rejected")
