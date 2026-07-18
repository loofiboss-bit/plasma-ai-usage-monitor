#!/usr/bin/env python3
"""Activate named controls in one application window through AT-SPI."""

from __future__ import annotations

import argparse
import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402


def descendants(node, inside_window: bool, window_name: str):
    try:
        name = node.get_name() or ""
        role = node.get_role_name() or ""
        inside_window = inside_window or (role == "frame" and name == window_name)
        if inside_window:
            yield node
        for index in range(node.get_child_count()):
            yield from descendants(node.get_child_at_index(index), inside_window, window_name)
    except Exception:
        return


def press(window_name: str, target_prefix: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        desktop = Atspi.get_desktop(0)
        for node in descendants(desktop, False, window_name):
            try:
                if not (node.get_name() or "").startswith(target_prefix):
                    continue
                actions = node.get_action_iface()
                for index in range(actions.get_n_actions()):
                    if actions.get_action_name(index).lower() == "press":
                        if not actions.do_action(index):
                            raise RuntimeError(f"AT-SPI rejected {target_prefix!r}")
                        return
            except Exception:
                continue
        time.sleep(0.25)
    raise SystemExit(
        f"Timed out waiting for {target_prefix!r} in window {window_name!r}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--window", required=True)
    parser.add_argument("--target", action="append", required=True)
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args()

    for target in args.target:
        press(args.window, target, args.timeout)
        time.sleep(1)


if __name__ == "__main__":
    main()
