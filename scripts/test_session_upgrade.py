#!/usr/bin/env python3
"""Isolated application initialization proof; RPM lifecycle is tested separately.

Requires system PyQt6 and installed Qt/KF runtime dependencies. Loads the public
base RPM module and candidate build module in separate processes without install.
All configuration, identity keys, sessions, imports and exports use scratch paths.
"""

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

MODULE = Path("com/github/loofi/aiusagemonitor")
QML = b"""
import QtQml
import com.github.loofi.aiusagemonitor 1.0
QtObject {
    id: root
    signal completed(string payload)
    property string phase
    property string projectDirectory
    property string projectId
    property string exportPath
    property int fixtureRetention: 47
    property var results: ({})
    property int pending: 0
    property var info: AppInfo
    property UsageDatabase database: UsageDatabase { retentionDays: root.fixtureRetention }
    property LocalSessionModel sessions: LocalSessionModel {
        retentionDays: root.fixtureRetention
        onSummaryReady: (id, data) => root.accept(id, data)
        onSessionsReady: (id, data) => root.accept(id, data)
        onDailyReady: (id, data) => root.accept(id, data)
        onExportReady: (id, error) => root.accept(id, {errorKey: error})
    }
    property Connections errors: Connections {
        target: root.sessions
        ignoreUnknownSignals: true
        function onQueryFailed(id, error) { root.completed(JSON.stringify({error: error, requestId: id})) }
    }
    function accept(id, data) {
        results[id] = data
        pending -= 1
        if (pending === 0) completed(JSON.stringify(results))
    }
    function run() {
        if (!database.init()) { completed(JSON.stringify({error: database.errorKey})); return }
        results = {version: info.version, pluginPath: info.pluginPath,
                   retention: sessions.retentionDays}
        if (phase === "base") {
            results.projectId = sessions.createProjectAlias("file://" + projectDirectory, "Upgrade fixture")
            completed(JSON.stringify(results)); return
        }
        results.alias = sessions.projectAlias(projectId)
        results.optIns = [sessions.codexEnabled, sessions.claudeCodeEnabled]
        pending = 8
        const from = new Date("2026-10-10T00:00:00.000Z")
        const to = new Date("2026-10-11T00:00:00.000Z")
        sessions.requestSummary("legacySummary", "codex-cli", from, to)
        sessions.requestSessions("legacySessions", "codex-cli", from, to)
        sessions.requestSummary("exactSummary", "codex-cli", from, to, projectId, {modelId: "model-a"})
        sessions.requestSessions("exactSessions", "codex-cli", from, to, projectId, 0, 100, {modelId: "model-a"})
        sessions.requestSummary("unknownSummary", "codex-cli", from, to, projectId, {modelId: ""})
        sessions.requestSessions("unknownSessions", "codex-cli", from, to, projectId, 0, 100, {modelId: ""})
        sessions.requestDaily("exactDaily", "codex-cli", from, to, projectId, "UTC", {modelId: "model-a"})
        sessions.requestExport("export", "codex-cli", from, to, projectId, "json", exportPath, {modelId: "model-a"})
    }
}
"""


def child(args):
    from PyQt6.QtCore import QMetaObject, QTimer, Qt, QUrl
    from PyQt6.QtGui import QGuiApplication
    from PyQt6.QtQml import QQmlComponent, QQmlEngine

    app = QGuiApplication([sys.argv[0]])
    engine = QQmlEngine()
    engine.addImportPath(str(args.module_root))
    component = QQmlComponent(engine)
    component.setData(QML, QUrl("upgrade-fixture.qml"))
    root = component.create()
    if root is None:
        raise RuntimeError("; ".join(error.toString() for error in component.errors()))
    for key, value in {"phase": args.phase, "projectDirectory": str(args.state / "project"),
                       "projectId": args.project_id, "exportPath": str(args.state / "export.json")}.items():
        root.setProperty(key, value)
    output = []
    root.completed.connect(lambda payload: (output.append(json.loads(payload)), app.quit()))
    QTimer.singleShot(20000, app.quit)
    QTimer.singleShot(0, lambda: QMetaObject.invokeMethod(root, "run", Qt.ConnectionType.DirectConnection))
    app.exec()
    if not output:
        raise RuntimeError("Application upgrade query timed out")
    result = output[0]
    if "error" in result:
        raise RuntimeError(f"Application query failed: {result}")
    expected_plugin = (args.module_root / MODULE / "libaiusagemonitorplugin.so").resolve()
    if Path(result["pluginPath"]).resolve() != expected_plugin:
        raise RuntimeError(f"Unexpected native plugin: {result['pluginPath']}")
    if result["version"] != args.expected_version:
        raise RuntimeError(f"Unexpected version: {result['version']}")
    print(json.dumps(result, sort_keys=True))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_phase(args, state, module_root, phase, version, project_id=""):
    environment = os.environ.copy()
    environment.update(HOME=str(state / "home"), XDG_DATA_HOME=str(state / "data"),
                       XDG_CONFIG_HOME=str(state / "config"), XDG_CACHE_HOME=str(state / "cache"),
                       XDG_STATE_HOME=str(state / "state"), XDG_RUNTIME_DIR=str(state / "runtime"),
                       CODEX_HOME=str(state / "codex"), CLAUDE_CONFIG_DIR=str(state / "claude"),
                       QT_QPA_PLATFORM="offscreen", QML_DISK_CACHE_PATH=str(state / "qml-cache"))
    # Do not let caller-supplied QML overrides select another native module.
    for name in ("QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_PLUGIN_PATH"):
        environment.pop(name, None)
    command = [sys.executable, str(Path(__file__).resolve()), "--phase", phase,
               "--state", str(state), "--module-root", str(module_root),
               "--expected-version", version, "--project-id", project_id]
    process = subprocess.run(command, env=environment, text=True, capture_output=True, timeout=30)
    (state.parent / f"{phase}.stdout.log").write_text(process.stdout)
    (state.parent / f"{phase}.stderr.log").write_text(process.stderr)
    if process.returncode:
        raise RuntimeError(f"{phase} failed: {process.stderr.strip()}")
    return json.loads(process.stdout.strip().splitlines()[-1])


def verify(args, work):
    state = work / "state"
    for folder in ("home", "data", "config", "cache", "state", "runtime", "codex", "claude", "project"):
        (state / folder).mkdir(parents=True, mode=0o700)
    base_root = work / "base"
    base_root.mkdir()
    prefix = "./usr/lib64/qt6/qml/"
    files = [prefix + str(MODULE / name) for name in
             ("qmldir", "libaiusagemonitorplugin.so", "aiusagemonitorplugin.qmltypes")]
    archive = subprocess.Popen(["rpm2cpio", str(args.base_rpm)], stdout=subprocess.PIPE)
    try:
        extracted = subprocess.run(["cpio", "--extract", "--make-directories", "--quiet",
                                    "--no-absolute-filenames", "--no-preserve-owner", *files],
                                   stdin=archive.stdout, cwd=base_root, capture_output=True, timeout=30)
        archive.stdout.close()
        if archive.wait(timeout=10) or extracted.returncode:
            raise RuntimeError(f"RPM extraction failed: {extracted.stderr.decode()}")
    finally:
        if archive.poll() is None:
            archive.kill()
            archive.wait()
    old_modules = base_root / "usr/lib64/qt6/qml"
    for name in files:
        path = base_root / name
        if not path.is_file() or path.is_symlink():
            raise RuntimeError(f"Missing or unsafe RPM module member: {name}")
    config = state / "config/plasma-org.kde.plasma.desktop-appletsrc"
    config.write_text("[Containments][1][Applets][1][Configuration][General]\n"
                      "codexSessionHistoryEnabled=true\nclaudeCodeSessionHistoryEnabled=false\n"
                      "historyRetentionDays=47\n")
    base = run_phase(args, state, old_modules, "base", args.base_version)
    project = base["projectId"]
    assert len(project) == 64, "Base application did not create project identity"
    database = state / "data/plasma-ai-usage-monitor/usage_history.db"
    key = state / "data/plasma-ai-usage-monitor/session-identity.key"
    before = {"config": digest(config), "key": digest(key)}
    with sqlite3.connect(database) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 9
        db.execute("INSERT INTO local_sessions(session_id,tool_id,project_id,started_at_utc,last_seen_at_utc) "
                   "VALUES('upgrade-session','codex',?,'2026-10-10T12:00:00.000Z','2026-10-10T12:01:00.000Z')", (project,))
        db.executemany("INSERT INTO local_usage_events(event_id,session_id,observed_at_utc,model_id,input_tokens,"
                       "output_tokens,parser_revision) VALUES(?,'upgrade-session','2026-10-10T12:00:00.000Z',?,?,?,1)",
                       [("known", "model-a", 12, 3), ("unknown", "", None, None)])
        records = {table: db.execute(f"SELECT * FROM {table} ORDER BY 1").fetchall()
                   for table in ("local_sessions", "local_usage_events", "local_project_aliases")}
    candidate_root = args.build_dir / "plugin"
    candidate = run_phase(args, state, candidate_root, "candidate", args.expected_version, project)
    assert candidate["alias"] == "Upgrade fixture"
    assert candidate["retention"] == 47
    assert candidate["optIns"] == [False, False], "Native defaults must remain opt-in"
    assert candidate["legacySummary"]["eventCount"] == 2
    assert len(candidate["legacySessions"]) == 1
    assert candidate["exactSummary"]["inputTokens"] == 12
    assert candidate["exactSummary"]["eventCount"] == 1
    assert candidate["exactSessions"][0]["inputTokens"] == 12
    assert candidate["exactDaily"][0]["inputTokens"] == 12
    assert candidate["unknownSummary"]["eventCount"] == 1
    assert candidate["unknownSessions"][0]["eventCount"] == 1
    assert candidate["unknownSummary"]["inputTokens"] in (None, ""), "Unknown token totals must not become zero"
    assert {option["modelId"] for option in candidate["exactSummary"]["modelOptions"]} == {"", "model-a"}
    assert candidate["export"]["errorKey"] == ""
    exported = json.loads((state / "export.json").read_text())["sessions"]
    assert len(exported) == 1 and exported[0]["inputTokens"] == 12
    assert exported[0]["modelId"] == "model-a"
    assert "Upgrade fixture" not in (state / "export.json").read_text()
    assert before == {"config": digest(config), "key": digest(key)}
    parser = configparser.ConfigParser()
    parser.read_string(config.read_text().replace("[Containments][1][Applets][1][Configuration][General]", "[General]"))
    assert parser["General"].getboolean("codexSessionHistoryEnabled")
    assert not parser["General"].getboolean("claudeCodeSessionHistoryEnabled")
    with sqlite3.connect(database) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 9
        for table, rows in records.items():
            assert db.execute(f"SELECT * FROM {table} ORDER BY 1").fetchall() == rows, table
    report = {"result": "passed", "baseVersion": base["version"], "candidateVersion": candidate["version"],
              "schemaVersion": 9, "preserved": ["sessions", "events", "aliases", "identity key", "KConfig fixture"],
              "proof": "isolated native application initialization and QML query compatibility",
              "limitations": ["RPM lifecycle is separate", "No physical Plasma desktop qualification",
                              "KConfig bytes preserved; live Plasma setting binding not exercised"]}
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-rpm", type=Path)
    parser.add_argument("--base-version", default="24.2.0")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--phase", choices=("base", "candidate"), help=argparse.SUPPRESS)
    parser.add_argument("--module-root", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--state", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--project-id", default="", help=argparse.SUPPRESS)
    parser.add_argument("--expected-version", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.phase:
        child(args)
        return
    if not args.base_rpm or not args.build_dir:
        parser.error("--base-rpm and --build-dir are required")
    args.base_rpm = args.base_rpm.resolve(strict=True)
    args.build_dir = args.build_dir.resolve(strict=True)
    args.expected_version = (Path(__file__).resolve().parents[1] / "VERSION").read_text().strip()
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix="session-upgrade-", dir=args.work_dir.resolve()))
        print(f"Retained evidence: {work}", flush=True)
        verify(args, work)
    else:
        with tempfile.TemporaryDirectory(prefix="session-upgrade-") as scratch:
            verify(args, Path(scratch))


if __name__ == "__main__":
    main()
