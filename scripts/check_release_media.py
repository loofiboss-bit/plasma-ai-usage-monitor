#!/usr/bin/env python3
"""Validate the canonical v14 screenshot set and its capture manifest."""

from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCREENSHOTS = ROOT / "assets" / "screenshots"
REQUIRED = (
    "guided-first-success.png",
    "verified-success.png",
    "main-window.png",
    "provider-intelligence.png",
    "settings-view.png",
    "history-view.png",
    "analyst-view.png",
    "panel-view.png",
)


def fail(message: str) -> None:
    raise SystemExit(f"Release media FAIL: {message}")


def png_dimensions(path: Path) -> tuple[int, int]:
    payload = path.read_bytes()[:24]
    if len(payload) != 24 or payload[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"{path.relative_to(ROOT)} is not a valid PNG")
    return struct.unpack(">II", payload[16:24])


version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
manifest_path = SCREENSHOTS / "v14-media-manifest.json"
if not manifest_path.is_file():
    fail("assets/screenshots/v14-media-manifest.json is missing")
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
if manifest.get("version") != version:
    fail(f"manifest version {manifest.get('version')!r} does not match VERSION {version}")
if not manifest.get("sessionId") or not manifest.get("fixtureSha256"):
    fail("manifest must identify one capture session and demo fixture")
if manifest.get("theme") != "Breeze Dark":
    fail("manifest must record the Breeze Dark capture theme")
if manifest.get("environment") != "isolated demo user":
    fail("manifest must record the isolated demo-user environment")

expected_fixture = hashlib.sha256(
    (ROOT / "scripts" / "demo" / "showcase_preset.json").read_bytes()
).hexdigest()
if manifest["fixtureSha256"] != expected_fixture:
    fail("manifest fixture hash does not match showcase_preset.json")

asset_hashes: dict[str, str] = {}
for filename in REQUIRED:
    path = SCREENSHOTS / filename
    if not path.is_file():
        fail(f"missing {path.relative_to(ROOT)}")
    width, height = png_dimensions(path)
    minimum_height = 150 if filename == "panel-view.png" else 700
    if width < 1200 or width > 1800 or height < minimum_height or height > 980:
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
for filename in ("guided-first-success.png", "main-window.png", "settings-view.png",
                 "history-view.png", "analyst-view.png", "panel-view.png"):
    if filename not in metainfo:
        fail(f"AppStream metadata does not reference {filename}")

print(f"Release media OK: {len(REQUIRED)} screenshots from session {manifest['sessionId']}")
