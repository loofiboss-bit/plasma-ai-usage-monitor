#!/usr/bin/env python3
"""Atomically update release surfaces from the canonical VERSION file."""

from __future__ import annotations

import json
import os
import re
import sys
import tempfile
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")


def replace_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise ValueError(f"could not find {label}")
    return updated


def render(version: str) -> dict[Path, str]:
    today = date.today().isoformat()
    outputs: dict[Path, str] = {}

    for relative in ("package/metadata.json", "package/contents/catalog/providers-v4.json",
                     "package/contents/catalog/subscriptions-v1.json"):
        path = ROOT / relative
        payload = json.loads(path.read_text(encoding="utf-8"))
        if relative.endswith("metadata.json"):
            payload["KPlugin"]["Version"] = version
            payload["X-AIUsageMonitor-RequiredPluginVersion"] = version
        else:
            payload["release"] = version
        outputs[path] = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"

    spec = (ROOT / "plasma-ai-usage-monitor.spec").read_text(encoding="utf-8")
    spec = replace_once(spec, r"^Version:\s*[^\n]+$", f"Version:        {version}", "RPM Version")
    if not re.search(rf"- {re.escape(version)}-1$", spec, re.MULTILINE):
        marker = "%changelog\n"
        entry = f"* {date.today():%a %b %d %Y} Loofi <loofi@github.com> - {version}-1\n- Prepare v{version} release\n\n"
        if marker not in spec:
            raise ValueError("could not find RPM changelog")
        spec = spec.replace(marker, marker + entry, 1)
    outputs[ROOT / "plasma-ai-usage-monitor.spec"] = spec

    metainfo_path = ROOT / "com.github.loofi.aiusagemonitor.metainfo.xml"
    metainfo = metainfo_path.read_text(encoding="utf-8")
    if not re.search(rf'<release version="{re.escape(version)}"', metainfo):
        metainfo = replace_once(
            metainfo,
            r"^(\s*)<releases>\s*$",
            rf'\1<releases>\n\1  <release version="{version}" date="{today}"/>',
            "AppStream releases",
        )
    outputs[metainfo_path] = metainfo

    readme_path = ROOT / "README.md"
    readme = readme_path.read_text(encoding="utf-8")
    readme = replace_once(
        readme,
        r"(\*\*Current (?:development )?release:\*\* `v)[0-9]+\.[0-9]+\.[0-9]+",
        rf"\g<1>{version}",
        "README current release",
    )
    outputs[readme_path] = readme

    roadmap_path = ROOT / "ROADMAP.md"
    roadmap = roadmap_path.read_text(encoding="utf-8")
    roadmap = replace_once(roadmap, r"(\*\*Current version:\*\* v)[0-9]+\.[0-9]+\.[0-9]+", rf"\g<1>{version}", "ROADMAP current version")
    outputs[roadmap_path] = roadmap
    outputs[ROOT / "VERSION"] = version + "\n"
    return outputs


def commit(outputs: dict[Path, str]) -> None:
    originals = {path: path.read_bytes() if path.exists() else None for path in outputs}
    staged: dict[Path, Path] = {}
    try:
        for path, content in outputs.items():
            fd, tmp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
            with os.fdopen(fd, "w", encoding="utf-8") as handle:
                handle.write(content)
                handle.flush()
                os.fsync(handle.fileno())
            staged[path] = Path(tmp_name)
        for path, tmp_path in staged.items():
            os.replace(tmp_path, path)
    except Exception:
        for path, content in originals.items():
            if content is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(content)
        raise
    finally:
        for tmp_path in staged.values():
            tmp_path.unlink(missing_ok=True)


def main() -> int:
    version = sys.argv[1] if len(sys.argv) == 2 else ""
    if not SEMVER.fullmatch(version):
        print("Usage: scripts/bump_version.sh MAJOR.MINOR.PATCH", file=sys.stderr)
        return 2
    outputs = render(version)
    commit(outputs)
    os.execv("/usr/bin/bash", ["bash", str(ROOT / "scripts/check_version_consistency.sh")])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
