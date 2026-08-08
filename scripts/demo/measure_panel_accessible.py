#!/usr/bin/env python3
"""Measure an AI Usage Monitor popup through the Plasma accessibility tree."""

from __future__ import annotations

import argparse
import json
import threading
import time
from pathlib import Path

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402


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
        states = node.get_state_set()
        if states.contains(Atspi.StateType.SHOWING) or states.contains(
            Atspi.StateType.VISIBLE
        ):
            return True
        rect = node.get_component_iface().get_extents(Atspi.CoordType.SCREEN)
        return rect.width > 0 and rect.height > 0
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
        node = next(named_nodes(prefix, pid), None)
        if node is not None:
            return node
        time.sleep(0.025)
    candidates = []
    for node in descendants(Atspi.get_desktop(0)):
        try:
            name = node.get_name() or ""
            if name.startswith(prefix):
                candidates.append({"name": name, "pid": process_id(node)})
        except Exception:
            continue
    raise RuntimeError(
        f"Timed out waiting for accessible node {prefix!r} from PID {pid}; "
        f"candidates={candidates}"
    )


def wait_for_popup(pid: int, timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        node = next(
            exactly_named_nodes(
                "AI Usage Monitor", "heading", pid, showing_only=True
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


def press(node) -> None:
    actions = node.get_action_iface()
    for index in range(actions.get_n_actions()):
        action = actions.get_action_name(index).lower()
        if action in {"press", "click"} and actions.do_action(index):
            return
    raise RuntimeError(f"Accessible node {node.get_name()!r} has no usable press action")


def press_and_wait_for_event(node, pid: int, event_type: str, timeout: float) -> int:
    matched = False

    def callback(event, *_user_data) -> None:
        nonlocal matched
        if process_id(event.source) == pid:
            matched = True
            Atspi.event_quit()

    listener = Atspi.EventListener.new(callback)
    if not listener.register(event_type):
        raise RuntimeError(f"Could not register AT-SPI event {event_type}")
    timer = threading.Timer(timeout, Atspi.event_quit)
    timer.start()
    started = time.monotonic()
    try:
        press(node)
        Atspi.event_main()
    finally:
        timer.cancel()
        listener.deregister(event_type)
    if not matched:
        raise RuntimeError(
            f"Timed out waiting for {event_type} from plasmashell PID {pid}"
        )
    return round((time.monotonic() - started) * 1000)


def request_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8").splitlines() if line)


def open_popup(compact_prefix: str, pid: int, timeout: float) -> int:
    compact = wait_for_node(compact_prefix, pid, timeout)
    if next(
        exactly_named_nodes(
            "AI Usage Monitor", "heading", pid, showing_only=True
        ),
        None,
    ) is not None:
        raise RuntimeError("A stale popup was showing before the sample")
    started = time.monotonic()
    press(compact)
    wait_for_popup(pid, timeout)
    return round((time.monotonic() - started) * 1000)


def close_popup(compact_prefix: str, pid: int, timeout: float) -> None:
    compact = wait_for_node(compact_prefix, pid, timeout)
    press(compact)
    wait_for_popup_closed(pid, timeout)
    wait_for_node(compact_prefix, pid, timeout)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-log", type=Path, required=True)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    compact_prefix = "AI Usage Monitor:"
    wait_for_node(compact_prefix, args.pid, args.timeout)
    time.sleep(1)
    startup_requests = request_count(args.request_log)

    before_popup = request_count(args.request_log)
    first_open_ms = open_popup(compact_prefix, args.pid, args.timeout)
    time.sleep(1)
    fresh_popup_requests = request_count(args.request_log) - before_popup

    close_popup(compact_prefix, args.pid, args.timeout)
    time.sleep(0.25)
    warm_open_ms = open_popup(compact_prefix, args.pid, args.timeout)
    if warm_open_ms < 20:
        raise RuntimeError(
            "Warm popup sample is below 20 ms and may have matched a stale node"
        )

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
