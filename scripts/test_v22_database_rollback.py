#!/usr/bin/env python3
"""Verify V22 migration and V21 rollback using isolated database copies."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile

CHILD = r'''
import json, sys
from PyQt6.QtCore import QUrl
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlComponent, QQmlEngine
app = QGuiApplication([sys.argv[0]])
engine = QQmlEngine()
engine.addImportPath(sys.argv[1])
component = QQmlComponent(engine)
component.setData(b"""import QtQml
import com.github.loofi.aiusagemonitor as Monitor
QtObject {
    property string version: Monitor.AppInfo.version
    property var database: Monitor.UsageDatabase {}
    property bool seed: false
    Component.onCompleted: {
        database.init()
        if (seed) database.recordSnapshot("Rollback fixture", 10, 5, 1, 1, 1, 1, 0, 0, 0, 0)
    }
}""", QUrl("rollback.qml"))
obj = component.createWithInitialProperties({"seed": sys.argv[2] == "seed"})
if obj is None:
    raise RuntimeError([error.toString() for error in component.errors()])
print(json.dumps({"version": obj.property("version")}))
'''


def inspect(path: Path) -> tuple[int, int]:
    with sqlite3.connect(f"file:{path}?mode=ro", uri=True) as database:
        return (database.execute("PRAGMA user_version").fetchone()[0],
                database.execute("SELECT COUNT(*) FROM usage_snapshots WHERE provider='Rollback fixture'").fetchone()[0])


def run(build: Path, data: Path, action: str) -> str:
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", PYTHONNOUSERSITE="1", XDG_DATA_HOME=str(data))
    result = subprocess.run([sys.executable, "-c", CHILD, str(build / "plugin"), action],
                            env=environment, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    return json.loads(result.stdout.strip().splitlines()[-1])["version"]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--v21-build-dir", required=True, type=Path)
    parser.add_argument("--v22-build-dir", default=Path("build/debug"), type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ai-monitor-v22-rollback-") as temporary:
        root = Path(temporary)
        data = root / "migration"
        path = data / "plasma-ai-usage-monitor/usage_history.db"
        version = run(args.v21_build_dir.resolve(), data, "seed")
        assert version.startswith("21."), version
        assert inspect(path) == (7, 1), inspect(path)
        version = run(args.v22_build_dir.resolve(), data, "open")
        assert version.startswith("22."), version
        assert inspect(path) == (8, 1), inspect(path)
        backup = Path(str(path) + ".v21-backup")
        assert inspect(backup) == (7, 1), inspect(backup)
        rollback = root / "rollback"
        rollback_path = rollback / "plasma-ai-usage-monitor/usage_history.db"
        rollback_path.parent.mkdir(parents=True)
        shutil.copy2(backup, rollback_path)
        version = run(args.v21_build_dir.resolve(), rollback, "open")
        assert version.startswith("21."), version
        assert inspect(rollback_path) == (7, 1), inspect(rollback_path)
        assert inspect(path) == (8, 1), inspect(path)
    print("Isolated rollback PASS: V21 v7 -> V22 v8; preserved v7 backup reopened only by V21.")


if __name__ == "__main__":
    main()
