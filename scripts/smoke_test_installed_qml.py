#!/usr/bin/env python3
"""Load the installed native QML plugin with isolated, disposable user state."""
import os
from pathlib import Path
import sys
import tempfile


def main():
    expected = (Path(__file__).resolve().parent.parent / "VERSION").read_text().strip()
    with tempfile.TemporaryDirectory(prefix="aiusage-installed-qml-") as home:
        for key, suffix in (("HOME", ""), ("XDG_CONFIG_HOME", "config"),
                            ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache"),
                            ("XDG_STATE_HOME", "state"), ("CODEX_HOME", "codex"),
                            ("CLAUDE_CONFIG_DIR", "claude"),
                            ("XDG_RUNTIME_DIR", "runtime")):
            target = Path(home) / suffix
            target.mkdir(exist_ok=True, mode=0o700)
            os.environ[key] = str(target)
        os.environ["QT_QPA_PLATFORM"] = "offscreen"
        from PyQt6.QtCore import QUrl
        from PyQt6.QtGui import QGuiApplication
        from PyQt6.QtQml import QQmlComponent, QQmlEngine
        app = QGuiApplication(sys.argv[:1])
        engine = QQmlEngine()
        engine.setImportPathList(["/usr/lib64/qt6/qml", "/usr/lib/qt6/qml"])
        component = QQmlComponent(engine)
        component.setData(b'''import QtQml
import com.github.loofi.aiusagemonitor 1.0 as AiMonitor
QtObject {
    property string version: AiMonitor.AppInfo.version
    property string pluginPath: AiMonitor.AppInfo.pluginPath
}
''', QUrl("installed-smoke.qml"))
        obj = component.create()
        if obj is None:
            raise RuntimeError("\n".join(error.toString() for error in component.errors()))
        actual = obj.property("version")
        if actual != expected:
            raise RuntimeError(f"Installed plugin version {actual!r}, expected {expected!r}")
        plugin_path = Path(obj.property("pluginPath")).resolve()
        roots = [Path(root) / "com/github/loofi/aiusagemonitor" for root in engine.importPathList()]
        if not any(plugin_path.is_relative_to(root.resolve()) for root in roots):
            raise RuntimeError(f"Plugin loaded from unexpected location: {plugin_path}")
        print(f"Installed native QML plugin: {actual} at {plugin_path} PASS (isolated user state)")
        del obj, component, engine, app


if __name__ == "__main__":
    main()
