#!/usr/bin/env python3
"""Generate a deterministic SPDX source manifest for an exact Git tag."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def git(*args: str, binary: bool = False):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=not binary)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    subprocess.run([str(ROOT / "scripts/verify_exact_tag.sh"), args.tag], cwd=ROOT, check=True)
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    commit = git("rev-parse", f"{args.tag}^{{commit}}").strip()
    paths = [line for line in git("ls-tree", "-r", "--name-only", args.tag).splitlines() if line]
    files = []
    for index, path in enumerate(paths, 1):
        content = git("show", f"{args.tag}:{path}", binary=True)
        files.append({
            "SPDXID": f"SPDXRef-File-{index}",
            "fileName": path,
            "checksums": [{"algorithm": "SHA256", "checksumValue": hashlib.sha256(content).hexdigest()}],
        })
    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"plasma-ai-usage-monitor-{version}-source",
        "documentNamespace": f"https://github.com/loofiboss-bit/plasma-ai-usage-monitor/releases/tag/{args.tag}#spdx-{commit}",
        "creationInfo": {"created": "1970-01-01T00:00:00Z", "creators": ["Tool: generate_source_sbom.py"]},
        "packages": [{
            "name": "plasma-ai-usage-monitor", "SPDXID": "SPDXRef-Package",
            "versionInfo": version, "downloadLocation": f"https://github.com/loofiboss-bit/plasma-ai-usage-monitor/tree/{args.tag}",
            "filesAnalyzed": True,
        }],
        "files": files,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
