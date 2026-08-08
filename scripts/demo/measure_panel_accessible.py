#!/usr/bin/env python3
"""Measure an AI Usage Monitor popup through the Plasma accessibility tree."""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from pathlib import Path

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402


def probe_progress(message: str) -> None:
    print(f"AI_USAGE_ATSPI_PROBE {message}", file=sys.stderr, flush=True)


def descendants(node):
    try:
        yield node
        for index in range(node.get_child_count()):
            yield from descendants(node.get_child_at_index(index))
    except Exception:
        return


def process_id(node) -> int:
    try:
        return int(node.get_process_id())
    except Exception:
        try:
            return int(node.get_application().get_process_id())
        except Exception:
            return -1


def is_showing(node) -> bool:
    try:
        current = node
        for _ in range(12):
            if current.get_role_name() in {"application", "desktop frame"}:
                break
            states = current.get_state_set()
            # Plasma may apply SHOWING or VISIBLE to the popup subtree. Cached
            # geometry survives after close, but both accessibility states are
            # removed; application-level visibility is excluded above.
            if states.contains(Atspi.StateType.SHOWING) or states.contains(
                Atspi.StateType.VISIBLE
            ):
                return True
            parent = current.get_parent()
            if parent is None or parent == current:
                break
            current = parent
        return False
    except Exception:
        return False


def named_nodes(prefix: str, pid: int):
    desktop = Atspi.get_desktop(0)
    for node in descendants(desktop):
        try:
            if (node.get_name() or "").startswith(prefix) and process_id(node) == pid:
                yield node
        except Exception:
            continue


def action_names(node) -> list[str]:
    try:
        actions = node.get_action_iface()
        return [
            actions.get_action_name(index).lower()
            for index in range(actions.get_n_actions())
        ]
    except Exception:
        return []


def node_details(node) -> dict:
    details = {
        "name": node.get_name() or "",
        "role": node.get_role_name() or "",
        "pid": process_id(node),
        "actions": action_names(node),
    }
    try:
        extents = node.get_component_iface().get_extents(Atspi.CoordType.SCREEN)
        details["extents"] = {
            "x": extents.x,
            "y": extents.y,
            "width": extents.width,
            "height": extents.height,
        }
    except Exception:
        details["extents"] = None
    return details


def exactly_named_nodes(name: str, role: str, pid: int, showing_only: bool = False):
    desktop = Atspi.get_desktop(0)
    for node in descendants(desktop):
        try:
            if (
                (node.get_name() or "") == name
                and node.get_role_name() == role
                and process_id(node) == pid
                and (not showing_only or is_showing(node))
            ):
                yield node
        except Exception:
            continue


def wait_for_node(prefix: str, pid: int, timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidates = list(named_nodes(prefix, pid))
        actionable = [
            node for node in candidates
            if any(action in {"press", "click"} for action in action_names(node))
        ]
        if len(actionable) == 1:
            return actionable[0]
        time.sleep(0.025)
    candidates = [
        node_details(node)
        for node in descendants(Atspi.get_desktop(0))
        if (node.get_name() or "").startswith(prefix)
    ]
    raise RuntimeError(
        f"Timed out waiting for accessible node {prefix!r} from PID {pid}; "
        f"candidates={json.dumps(candidates, sort_keys=True)}"
    )


def wait_for_popup(pid: int, timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        node = next(
            exactly_named_nodes(
                "AI Usage Monitor", "heading", pid
            ),
            None,
        )
        if node is not None:
            return node
        time.sleep(0.025)
    visible = []
    for node in descendants(Atspi.get_desktop(0)):
        try:
            name = node.get_name() or ""
            role = node.get_role_name() or ""
            if name and role not in {"filler", "unknown"}:
                visible.append(f"{role}:{name}")
        except Exception:
            continue
    raise RuntimeError(
        "Timed out waiting for the accessible panel popup; tree="
        + " | ".join(visible[-80:])
    )


def wait_for_popup_closed(pid: int, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if next(exactly_named_nodes("AI Usage Monitor", "heading", pid, True), None) is None:
            return
        time.sleep(0.025)
    remaining = list(exactly_named_nodes("AI Usage Monitor", "heading", pid))
    details = []
    for node in remaining:
        try:
            details.append({
                "states": [str(value) for value in node.get_state_set().get_states()],
                "showing": is_showing(node),
            })
        except Exception:
            continue
    raise RuntimeError(f"Timed out waiting for the panel popup to close; nodes={details}")


def focus(node) -> None:
    probe_progress("focus-start")
    actions = node.get_action_iface()
    focus_index = next(
        (
            index
            for index in range(actions.get_n_actions())
            if actions.get_action_name(index).lower() == "setfocus"
        ),
        None,
    )
    if focus_index is None or not actions.do_action(focus_index):
        raise RuntimeError(
            f"Accessible node {node.get_name()!r} could not receive exact focus"
        )
    probe_progress("focus-complete")
    time.sleep(0.1)


def request_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8").splitlines() if line)


def wait_for_log_marker(
    path: Path, marker: str, offset: int, timeout: float
) -> int:
    started = time.monotonic()
    deadline = started + timeout
    while time.monotonic() < deadline:
        if path.exists():
            with path.open("r", encoding="utf-8", errors="replace") as stream:
                stream.seek(offset)
                if any(marker in line for line in stream):
                    return round((time.monotonic() - started) * 1000)
        time.sleep(0.005)
    detail = path.read_text(encoding="utf-8", errors="replace")[-4000:]
    raise RuntimeError(
        f"Timed out waiting for {marker!r}; "
        f"log tail={detail}"
    )


def wait_for_log_duration(
    path: Path, marker: str, offset: int, timeout: float
) -> int:
    deadline = time.monotonic() + timeout
    pattern = re.compile(re.escape(marker) + r"[^\n]*?([0-9]+(?:\.[0-9]+)?)\s*ms")
    while time.monotonic() < deadline:
        if path.exists():
            with path.open("r", encoding="utf-8", errors="replace") as stream:
                stream.seek(offset)
                match = pattern.search(stream.read())
                if match:
                    return round(float(match.group(1)))
        time.sleep(0.005)
    detail = path.read_text(encoding="utf-8", errors="replace")[-4000:]
    raise RuntimeError(
        f"Timed out waiting for timed marker {marker!r}; log tail={detail}"
    )


def open_popup(
    compact_prefix: str, pid: int, session_log: Path, timeout: float
) -> tuple[int, int]:
    compact = wait_for_node(compact_prefix, pid, timeout)
    focus(compact)
    offset = session_log.stat().st_size if session_log.exists() else 0
    try:
        elapsed = wait_for_log_duration(
            session_log, "AI_USAGE_POPUP_FIRST_FRAME", offset, timeout
        )
    except RuntimeError as error:
        raise RuntimeError(
            f"{error}; selected={json.dumps(node_details(compact), sort_keys=True)}"
        ) from error
    wait_for_popup(pid, timeout)
    return elapsed, offset


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-log", type=Path, required=True)
    parser.add_argument("--session-log", type=Path, required=True)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--timeout", type=float, default=35.0)
    args = parser.parse_args()

    probe_progress("started")
    compact_prefix = "AI Usage Monitor:"
    wait_for_node(compact_prefix, args.pid, args.timeout)
    probe_progress("compact-ready")
    time.sleep(1)
    startup_requests = request_count(args.request_log)

    before_popup = request_count(args.request_log)
    first_open_ms, sequence_offset = open_popup(
        compact_prefix, args.pid, args.session_log, args.timeout
    )
    probe_progress("first-open-complete")
    time.sleep(1)
    fresh_popup_requests = request_count(args.request_log) - before_popup

    wait_for_log_marker(
        args.session_log, "AI_USAGE_POPUP_CLOSED", sequence_offset, args.timeout
    )
    wait_for_node(compact_prefix, args.pid, args.timeout)
    warm_open_ms = wait_for_log_duration(
        args.session_log,
        "AI_USAGE_POPUP_WARM_FRAME",
        sequence_offset,
        args.timeout,
    )
    probe_progress("warm-open-complete")

    print(
        json.dumps(
            {
                "first_panel_popup_ms": first_open_ms,
                "warm_panel_popup_ms": warm_open_ms,
                "startup_network_requests": startup_requests,
                "fresh_popup_network_requests": fresh_popup_requests,
            }
        )
    )


if __name__ == "__main__":
    main()
