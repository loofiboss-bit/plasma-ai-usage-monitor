#!/usr/bin/env python3
"""Keep release validation credential-free and free of timed promotion waits."""

from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"Release policy FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


version = (ROOT / "VERSION").read_text().strip()
if version != "14.0.0":
    fail(f"phase 7 requires VERSION 14.0.0, got {version}")

checklist = (ROOT / "docs/release/v14.0.0-checklist.md").read_text()
required_policy = (
    "Provider API keys and live provider accounts are optional release evidence "
    "and never block publication."
)
if required_policy not in checklist:
    fail("v14 checklist must declare provider credentials non-blocking")
if "Publish `v14.0.0` directly from the verified `main` commit" not in checklist:
    fail("v14 checklist must require direct stable publication")

release_surfaces = [
    ".github/workflows/release.yml",
    "Justfile",
    "scripts/copr_submit_build.sh",
    "scripts/verify_exact_tag.sh",
]
provider_secret_markers = (
    "OPENROUTER_API_KEY",
    "LITELLM_API_KEY",
    "CEREBRAS_API_KEY",
    "FIREWORKS_API_KEY",
    "kwallet-query",
)
for relative in release_surfaces:
    text = (ROOT / relative).read_text()
    for marker in provider_secret_markers:
        if marker in text:
            fail(f"{relative} requires provider credential marker {marker}")

promotion_gate = (ROOT / "scripts/verify_exact_tag.sh").read_text()
for timed_wait_marker in ("minimum_soak", "soak_seconds", "7 * 24"):
    if timed_wait_marker in promotion_gate:
        fail(f"stable promotion still contains timed wait marker {timed_wait_marker}")
for prerelease_marker in ("alpha|beta|rc", "rc_tag", "prerelease"):
    if prerelease_marker in promotion_gate:
        fail(f"stable promotion still contains prerelease marker {prerelease_marker}")

if 'type="development"' in (ROOT / "com.github.loofi.aiusagemonitor.metainfo.xml").read_text():
    fail("AppStream still marks the stable version as development")
if "Current development release" in (ROOT / "README.md").read_text():
    fail("README still labels v14 as a development release")

print("Release policy OK: credential-free direct stable gate with no prerelease or soak")
