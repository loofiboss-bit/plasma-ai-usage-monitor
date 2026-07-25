#!/usr/bin/env python3
"""Validate the canonical v15 screenshot set and its capture manifest."""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import struct
from pathlib import Path

from release_media_evidence import EvidenceError, validate_capture_evidence

ROOT = Path(__file__).resolve().parents[1]
SCREENSHOTS = ROOT / "assets" / "screenshots"
REQUIRED = (
    "overview-popup.png",
    "attention-state.png",
    "quota-reset-state.png",
    "tool-only-overview.png",
    "retained-history.png",
    "history-gap.png",
    "analyst-sufficient.png",
    "analyst-insufficient.png",
    "panel-lowest-quota.png",
)
SCENARIOS = {
    "overview-popup.png": "media-overview",
    "attention-state.png": "media-attention",
    "quota-reset-state.png": "media-quota",
    "tool-only-overview.png": "media-tool-only",
    "retained-history.png": "media-history-retained",
    "history-gap.png": "media-history-gap",
    "analyst-sufficient.png": "media-analyst-sufficient",
    "analyst-insufficient.png": "media-analyst-insufficient",
    "panel-lowest-quota.png": "media-panel",
}


def fail(message: str) -> None:
    raise SystemExit(f"Release media FAIL: {message}")


def png_dimensions(path: Path) -> tuple[int, int]:
    payload = path.read_bytes()[:24]
    if len(payload) != 24 or payload[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"{path.relative_to(ROOT)} is not a valid PNG")
    return struct.unpack(">II", payload[16:24])


def committed_source_tree_sha256(commit: str) -> str:
    paths_result = subprocess.run(
        [
            "git",
            "ls-tree",
            "-r",
            "--name-only",
            commit,
            "--",
            "VERSION",
            "package",
            "scripts/demo",
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if paths_result.returncode != 0:
        fail(f"cannot inspect recorded source-tree commit {commit}")

    inventory = []
    for relative in sorted(paths_result.stdout.splitlines()):
        blob = subprocess.run(
            ["git", "show", f"{commit}:{relative}"],
            cwd=ROOT,
            check=False,
            capture_output=True,
        )
        if blob.returncode != 0:
            fail(f"cannot read {relative} from source-tree commit {commit}")
        inventory.append(
            hashlib.sha256(blob.stdout).hexdigest() + "  " + relative + "\n"
        )
    return hashlib.sha256("".join(inventory).encode()).hexdigest()


version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
manifest_path = SCREENSHOTS / "v15-media-manifest.json"
if not manifest_path.is_file():
    fail("assets/screenshots/v15-media-manifest.json is missing")
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
if manifest.get("version") != version:
    fail(f"manifest version {manifest.get('version')!r} does not match VERSION {version}")
if (
    not manifest.get("sessionId")
    or not manifest.get("fixtureSha256")
    or not manifest.get("sourceTreeSha256")
    or not manifest.get("sourceTreeCommit")
):
    fail(
        "manifest must identify one capture session, fixture, source tree, "
        "and source-tree commit"
    )
if manifest.get("theme") != "Breeze Dark":
    fail("manifest must record the Breeze Dark capture theme")
if manifest.get("environment") != "isolated demo user":
    fail("manifest must record the isolated demo-user environment")
for key in ("captureCommit", "capturedAt", "plasmaVersion", "scale"):
    if not manifest.get(key):
        fail(f"manifest must record {key}")

expected_fixture = hashlib.sha256(
    "".join(
        hashlib.sha256(path.read_bytes()).hexdigest()
        + "  "
        + str(path.relative_to(ROOT))
        + "\n"
        for path in (
            ROOT / "scripts" / "demo" / "showcase_preset.json",
            ROOT / "scripts" / "demo" / "generate_v15_media_history.py",
            ROOT / "package" / "contents" / "ui" / "components"
            / "MediaDailyState.qml",
        )
    ).encode()
).hexdigest()
if manifest["fixtureSha256"] != expected_fixture:
    fail("manifest fixture hash does not match the v15 media fixtures")

source_tree_commit = manifest["sourceTreeCommit"]
if not re.fullmatch(r"[0-9a-f]{40}", source_tree_commit):
    fail("manifest sourceTreeCommit must be a full Git object ID")
if not os.environ.get("AI_USAGE_MONITOR_MEDIA_FORCE_FILESYSTEM"):
    expected_source_tree = committed_source_tree_sha256(source_tree_commit)
    if manifest["sourceTreeSha256"] != expected_source_tree:
        fail(
            "manifest source tree hash does not match its recorded "
            "source-tree commit"
        )
if manifest.get("scenarios") != SCENARIOS:
    fail("manifest scenarios do not match the v15 capture contract")
if int(version.split(".", 1)[0]) >= 16:
    try:
        validate_capture_evidence(manifest.get("captureEvidence"), REQUIRED)
    except EvidenceError as error:
        fail(str(error))

asset_hashes: dict[str, str] = {}
for filename in REQUIRED:
    path = SCREENSHOTS / filename
    if not path.is_file():
        fail(f"missing {path.relative_to(ROOT)}")
    width, height = png_dimensions(path)
    if filename == "overview-popup.png":
        valid_geometry = 820 <= width <= 1000 and 1050 <= height <= 1300
    else:
        valid_geometry = 1200 <= width <= 2100 and 700 <= height <= 1100
    if not valid_geometry:
        fail(f"{filename} has unexpected geometry ({width}x{height})")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    asset_hashes[filename] = digest
    if manifest.get("assets", {}).get(filename) != digest:
        fail(f"manifest hash does not match {filename}")

if len(set(asset_hashes.values())) != len(REQUIRED):
    fail("canonical screenshots must be distinct images")

readme = (ROOT / "README.md").read_text(encoding="utf-8")
metainfo = (ROOT / "com.github.loofi.aiusagemonitor.metainfo.xml").read_text(encoding="utf-8")
for filename in REQUIRED:
    if filename not in readme:
        fail(f"README does not reference {filename}")
for filename in REQUIRED:
    if filename not in metainfo:
        fail(f"AppStream metadata does not reference {filename}")

print(
    f"Release media OK: {len(REQUIRED)} screenshots from session "
    f"{manifest['sessionId']} at source tree {source_tree_commit[:12]}"
)
