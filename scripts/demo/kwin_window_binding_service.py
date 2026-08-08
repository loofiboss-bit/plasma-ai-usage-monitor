#!/usr/bin/env python3
"""Accept one exact PID-bound KWin window acknowledgement on a private bus."""

from __future__ import annotations

import argparse
import os
import warnings
from pathlib import Path

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402

BUS_NAME = "com.github.loofi.aiusagemonitor.Phase0Binding"
OBJECT_PATH = "/Phase0Binding"
INTERFACE = BUS_NAME
INTROSPECTION_XML = f"""
<node>
  <interface name="{INTERFACE}">
    <method name="WindowBound">
      <arg type="s" name="windowId" direction="in"/>
      <arg type="s" name="pid" direction="in"/>
      <arg type="b" name="accepted" direction="out"/>
    </method>
  </interface>
</node>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ready", type=Path, required=True)
    args = parser.parse_args()

    loop = GLib.MainLoop()
    connection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    request = connection.call_sync(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "RequestName",
        GLib.Variant("(su)", (BUS_NAME, 0)),
        GLib.VariantType.new("(u)"),
        Gio.DBusCallFlags.NONE,
        -1,
        None,
    )
    if request.unpack()[0] != 1:
        raise RuntimeError(f"could not own private bus name {BUS_NAME}")

    node_info = Gio.DBusNodeInfo.new_for_xml(INTROSPECTION_XML)

    def handle_method_call(
        _connection,
        _sender,
        _object_path,
        _interface_name,
        method_name,
        parameters,
        invocation,
    ) -> None:
        if method_name != "WindowBound":
            invocation.return_dbus_error(
                f"{INTERFACE}.UnknownMethod", "Unsupported method"
            )
            return
        window_id, pid_text = parameters.unpack()
        accepted = pid_text == str(args.pid) and bool(window_id)
        if accepted:
            temporary = args.output.with_name(args.output.name + ".tmp")
            temporary.write_text(f"{window_id} {pid_text}\n", encoding="utf-8")
            os.replace(temporary, args.output)
        invocation.return_value(GLib.Variant("(b)", (accepted,)))
        if accepted:
            GLib.idle_add(loop.quit)

    with warnings.catch_warnings():
        warnings.filterwarnings(
            "ignore",
            message="Gio.DBusConnection.register_object is deprecated",
            category=DeprecationWarning,
        )
        registration_id = connection.register_object(
            OBJECT_PATH,
            node_info.interfaces[0],
            handle_method_call,
            None,
            None,
        )
    if registration_id == 0:
        raise RuntimeError("could not register private binding object")
    args.ready.write_text("ready\n", encoding="utf-8")
    loop.run()
    connection.unregister_object(registration_id)


if __name__ == "__main__":
    main()
