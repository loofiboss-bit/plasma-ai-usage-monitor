#!/usr/bin/env python3
"""Keep release validation credential-free and free of timed promotion waits."""

from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"Release policy FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


checklist = (ROOT / "docs/release/v13.0.0-checklist.md").read_text()
required_policy = (
    "Provider API keys and live provider accounts are optional release evidence "
    "and never block publication."
)
if required_policy not in checklist:
    fail("v13 checklist must declare provider credentials non-blocking")

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

if 'type="development"' in (ROOT / "com.github.loofi.aiusagemonitor.metainfo.xml").read_text():
    fail("AppStream still marks the stable version as development")
if "Current development release" in (ROOT / "README.md").read_text():
    fail("README still labels v13 as a development release")

print("Release policy OK: credential-free deterministic gates and no timed soak")
