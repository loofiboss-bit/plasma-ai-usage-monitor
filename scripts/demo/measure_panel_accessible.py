#!/usr/bin/env python3
"""Measure an AI Usage Monitor popup through the Plasma accessibility tree."""

from __future__ import annotations

import argparse
import json
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


def named_nodes(prefix: str):
    desktop = Atspi.get_desktop(0)
    for node in descendants(desktop):
        try:
            if (node.get_name() or "").startswith(prefix):
                yield node
        except Exception:
            continue


def exactly_named_nodes(name: str, role: str):
    desktop = Atspi.get_desktop(0)
    for node in descendants(desktop):
        try:
            if (node.get_name() or "") == name and node.get_role_name() == role:
                yield node
        except Exception:
            continue


def wait_for_node(prefix: str, timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        node = next(named_nodes(prefix), None)
        if node is not None:
            return node
        time.sleep(0.025)
    raise RuntimeError(f"Timed out waiting for accessible node {prefix!r}")


def wait_for_popup(timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        node = next(exactly_named_nodes("AI Usage Monitor", "heading"), None)
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


def press(node) -> None:
    actions = node.get_action_iface()
    for index in range(actions.get_n_actions()):
        action = actions.get_action_name(index).lower()
        if action in {"press", "click"} and actions.do_action(index):
            return
    raise RuntimeError(f"Accessible node {node.get_name()!r} has no usable press action")


def request_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8").splitlines() if line)


def open_popup(compact_prefix: str, timeout: float) -> int:
    compact = wait_for_node(compact_prefix, timeout)
    started = time.monotonic()
    press(compact)
    wait_for_popup(timeout)
    return round((time.monotonic() - started) * 1000)


def close_popup(compact_prefix: str, timeout: float) -> None:
    Atspi.generate_keyboard_event(
        9, None, Atspi.KeySynthType.PRESSRELEASE
    )
    wait_for_node(compact_prefix, timeout)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request-log", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    compact_prefix = "AI Usage Monitor:"
    wait_for_node(compact_prefix, args.timeout)
    time.sleep(1)
    startup_requests = request_count(args.request_log)

    before_popup = request_count(args.request_log)
    first_open_ms = open_popup(compact_prefix, args.timeout)
    time.sleep(1)
    fresh_popup_requests = request_count(args.request_log) - before_popup

    close_popup(compact_prefix, args.timeout)
    time.sleep(0.25)
    warm_open_ms = open_popup(compact_prefix, args.timeout)

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
