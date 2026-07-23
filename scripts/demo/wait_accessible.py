#!/usr/bin/env python3
"""Wait until an accessible object appears in a named application window."""

from __future__ import annotations

import argparse
import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402


def descendants(node, inside_window: bool, window_prefix: str):
    try:
        name = node.get_name() or ""
        role = node.get_role_name() or ""
        inside_window = inside_window or (
            role == "frame" and name.startswith(window_prefix)
        )
        if inside_window:
            yield node
        for index in range(node.get_child_count()):
            yield from descendants(
                node.get_child_at_index(index), inside_window, window_prefix
            )
    except Exception:
        return


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--window", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args()

    started = time.monotonic()
    deadline = started + args.timeout
    while time.monotonic() < deadline:
        desktop = Atspi.get_desktop(0)
        for node in descendants(desktop, False, args.window):
            if (node.get_name() or "").startswith(args.target):
                elapsed_ms = round((time.monotonic() - started) * 1000)
                print(f"accessible_ready_ms={elapsed_ms}")
                return
        time.sleep(0.025)
    raise SystemExit(
        f"Timed out waiting for {args.target!r} in window {args.window!r}"
    )


if __name__ == "__main__":
    main()
