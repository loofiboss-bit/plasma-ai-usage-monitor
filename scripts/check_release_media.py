#!/usr/bin/env python3
"""Validate the canonical v18 screenshot set and its capture manifest."""

from __future__ import annotations

import hashlib
import json
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
    "source-detail.png",
    "history-gap.png",
    "analyst-sufficient.png",
    "analyst-insufficient.png",
    "guided-first-success.png",
    "budget-control.png",
    "plugin-recovery.png",
    "panel-lowest-quota.png",
)
SCENARIOS = {
    "overview-popup.png": "media-overview",
    "attention-state.png": "media-attention",
    "source-detail.png": "media-source-detail",
    "history-gap.png": "media-history-gap",
    "analyst-sufficient.png": "media-analyst-sufficient",
    "analyst-insufficient.png": "media-analyst-insufficient",
    "guided-first-success.png": "onboarding-source",
    "budget-control.png": "budget-settings",
    "plugin-recovery.png": "plugin-recovery",
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
            "CMakeLists.txt",
            "package",
            "plugin",
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

def filesystem_source_tree_sha256() -> str:
    git_paths = subprocess.run(
        [
            "git",
            "ls-files",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "CMakeLists.txt",
            "package",
            "plugin",
            "scripts/demo",
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if git_paths.returncode == 0:
        paths = git_paths.stdout.splitlines()
    else:
        paths = [
            str(path.relative_to(ROOT))
            for root in (
                ROOT / "package",
                ROOT / "plugin",
                ROOT / "scripts" / "demo",
            )
            for path in root.rglob("*")
            if path.is_file()
            and "__pycache__" not in path.parts
            and path.suffix not in {".pyc", ".pyo"}
        ]
        paths.append("CMakeLists.txt")

    inventory = []
    for relative in sorted(paths):
        path = ROOT / relative
        if path.is_file():
            inventory.append(
                hashlib.sha256(path.read_bytes()).hexdigest()
                + "  " + relative + "\n"
            )
    return hashlib.sha256("".join(inventory).encode()).hexdigest()


version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
release_target_path = ROOT / "RELEASE_TARGET"
expected_version = (
    release_target_path.read_text(encoding="utf-8").strip()
    if release_target_path.is_file()
    else version
)
if release_target_path.is_file():
    current_major, current_minor, current_patch = map(int, version.split("."))
    target_major, target_minor, target_patch = map(int, expected_version.split("."))
    if (target_major, target_minor, target_patch) != (current_major + 1, 0, 0):
        fail("RELEASE_TARGET must be the next major .0.0 while VERSION remains unbumped")
manifest_path = SCREENSHOTS / "v18-media-manifest.json"
if not manifest_path.is_file():
    fail("assets/screenshots/v18-media-manifest.json is missing")
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
if manifest.get("version") != expected_version:
    fail(
        f"manifest version {manifest.get('version')!r} does not match "
        f"release target {expected_version}"
    )
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
            ROOT / "scripts" / "demo" / "generate_v17_media_history.py",
            ROOT / "package" / "contents" / "ui" / "components"
            / "MediaDailyState.qml",
        )
    ).encode()
).hexdigest()
if manifest["fixtureSha256"] != expected_fixture:
    fail("manifest fixture hash does not match the v18 media fixtures")

source_tree_commit = manifest["sourceTreeCommit"]
if not re.fullmatch(r"[0-9a-f]{40}", source_tree_commit):
    fail("manifest sourceTreeCommit must be a full Git object ID")
source_tree_mode = manifest.get("sourceTreeMode", "git-commit")
if source_tree_mode == "git-commit":
    expected_source_tree = committed_source_tree_sha256(source_tree_commit)
elif source_tree_mode == "filesystem-release-candidate":
    expected_source_tree = filesystem_source_tree_sha256()
else:
    fail(f"unsupported sourceTreeMode {source_tree_mode!r}")
if manifest["sourceTreeSha256"] != expected_source_tree:
    fail("manifest source tree hash does not match its recorded source")
if manifest.get("scenarios") != SCENARIOS:
    fail("manifest scenarios do not match the v18 capture contract")
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

source_identity = (
    source_tree_commit[:12]
    if source_tree_mode == "git-commit"
    else f"{manifest['sourceTreeSha256'][:12]} (filesystem release candidate)"
)
print(
    f"Release media OK: {len(REQUIRED)} screenshots from session "
    f"{manifest['sessionId']} at source tree {source_identity}"
)
