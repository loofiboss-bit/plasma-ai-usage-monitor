#!/usr/bin/env python3
"""Validation helpers for PID-, window-, and AT-SPI-bound media evidence."""

from __future__ import annotations

import re


class EvidenceError(ValueError):
    pass


def validate_capture_evidence(
    evidence: object, required_assets: tuple[str, ...]
) -> None:
    if not isinstance(evidence, dict) or set(evidence) != set(required_assets):
        raise EvidenceError("capture evidence must cover every canonical asset exactly")

    for asset in required_assets:
        row = evidence.get(asset)
        if not isinstance(row, dict):
            raise EvidenceError(f"{asset} capture evidence is not an object")
        pid = row.get("pid")
        accessible_pid = row.get("accessiblePid", pid)
        window_id = row.get("windowId")
        identity = row.get("windowIdentity")
        marker = row.get("marker")
        before = row.get("markerBefore")
        after = row.get("markerAfter")
        if not isinstance(pid, int) or pid <= 1:
            raise EvidenceError(f"{asset} has an invalid capture PID")
        if not isinstance(accessible_pid, int) or accessible_pid <= 1:
            raise EvidenceError(f"{asset} has an invalid accessibility PID")
        if not isinstance(window_id, str) or not window_id:
            raise EvidenceError(f"{asset} has no KWin window identity")
        if not isinstance(identity, str) or not identity:
            raise EvidenceError(f"{asset} has no KWin window metadata")
        if re.search(rf'"pid"\s+\w+\s+{pid}\b', identity) is None:
            raise EvidenceError(f"{asset} window metadata does not match its PID")
        expected_caption = (
            "KDE Wayland Compositor"
            if asset == "panel-lowest-quota.png"
            else "AI Usage Monitor"
        )
        if expected_caption not in identity:
            raise EvidenceError(
                f"{asset} window metadata identifies the wrong foreground window"
            )
        if not isinstance(marker, str) or not marker:
            raise EvidenceError(f"{asset} has no AT-SPI marker")
        for boundary, value in (("before", before), ("after", after)):
            if (
                not isinstance(value, str)
                or marker not in value
                or re.search(rf"\bpid={accessible_pid}\b", value) is None
            ):
                raise EvidenceError(
                    f"{asset} AT-SPI {boundary} evidence does not match "
                    f"PID {accessible_pid} and marker {marker!r}"
                )
