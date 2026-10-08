#!/usr/bin/env python3
"""Capture a real isolated Plasma panel matrix; never operate on the host session."""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = ("media-panel", "media-panel-disconnected", "media-panel-zero", "media-panel-full", "plugin-recovery")


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


def identity():
    return subprocess.check_output([sys.executable, str(ROOT / "scripts/demo/source_identity.py")], text=True).strip()


def inner(args):
    # Both the private HOME and session marker must be established by outer().
    if os.environ.get("AI_USAGE_MATRIX_ISOLATED") != "1" or not Path(os.environ["HOME"], ".matrix-isolated").is_file():
        raise SystemExit("Refusing to operate outside a matrix-owned isolated session")
    services = []
    try:
        run(["dbus-update-activation-environment", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE"])
        services.append(subprocess.Popen(["/usr/libexec/at-spi-bus-launcher", "--launch-immediately", "--a11y=1"]))
        time.sleep(1)
        services.append(subprocess.Popen(["/usr/libexec/at-spi2-registryd", "--dbus-name", "org.a11y.atspi.Registry"]))
        plasma = subprocess.Popen(["plasmashell"])
        services.append(plasma)
        def evaluate(script):
            return json.loads(subprocess.check_output(["busctl", "--json=short", "--user", "call", "org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript", "s", script], text=True, stderr=subprocess.STDOUT))["data"][0].strip()
        for attempt in range(60):
            if plasma.poll() is not None:
                raise RuntimeError("Isolated plasmashell exited")
            try:
                evaluate("print('ready')")
                break
            except subprocess.CalledProcessError:
                time.sleep(1)
        else:
            raise RuntimeError("Isolated Plasma scripting did not become ready")
        time.sleep(8)  # Allow the initial desktop containment and screen mapping to settle.
        for orientation, thickness in itertools.product(args.orientations, args.sizes):
            location = "bottom" if orientation == "horizontal" else "left"
            script = '''var p=panels(); for(var i=0;i<p.length;i++) p[i].remove();
var d=desktops(); for(var j=0;j<d.length;j++) { var w=d[j].widgets(); for(var k=0;k<w.length;k++) w[k].remove(); d[j].wallpaperPlugin="org.kde.color"; d[j].currentConfigGroup=["Wallpaper","org.kde.color","General"]; d[j].writeConfig("Color","#202326"); }
var panel=new Panel; panel.screen=0; panel.location=%s; panel.height=%d;
var monitor=panel.addWidget("com.github.loofi.aiusagemonitor");
monitor.currentConfigGroup=["General"]; monitor.writeConfig("compactDisplayMode","lowest-quota"); monitor.writeConfig("alertsEnabled",false);
print("matrix-panel-created");''' % (json.dumps(location), thickness)
            creation = evaluate(script)
            print("Panel creation:", creation, flush=True)
            time.sleep(3)
            # PID binding demonstrates the widget is exposed by this session's shell.
            marker = {"plugin-recovery": "AI Usage Monitor needs its native plugin",
                      "media-panel-zero": "AI Usage Monitor: 0%",
                      "media-panel-full": "AI Usage Monitor: 100%",
                      "media-panel": "AI Usage Monitor: 28%"}.get(args.scenario, "AI Usage Monitor:")
            accessible_command = [sys.executable, str(ROOT / "scripts/demo/wait_accessible.py"), "--pid", str(plasma.pid), "--target", marker, "--timeout", "20"]
            try:
                before = subprocess.check_output(accessible_command, text=True).strip()
            except subprocess.CalledProcessError:
                from wait_accessible import Atspi, descendants
                desktop = Atspi.get_desktop(0)
                for i in range(desktop.get_child_count()):
                    application = desktop.get_child_at_index(i)
                    print("AT-SPI application", application.get_process_id(), application.get_name(), flush=True)
                    if application.get_process_id() == plasma.pid:
                        for node in descendants(application, False, ""):
                            print("AT-SPI node", node.get_role_name(), repr(node.get_name()), flush=True)
                run(["spectacle", "--fullscreen", "--background", "--nonotify", "--output", str(args.output / "failed-panel-diagnostic.png")])
                raise
            name = f"{args.scenario}-{args.theme}-{args.scale:g}-{orientation}-{thickness}"
            image = args.output / f"{name}.png"
            run(["spectacle", "--fullscreen", "--background", "--nonotify", "--output", str(image)])
            for attempt in range(40):
                if image.is_file() and image.stat().st_size:
                    break
                time.sleep(0.25)
            data = image.read_bytes()
            if data[:8] != b"\x89PNG\r\n\x1a\n":
                raise RuntimeError("Capture is not a PNG")
            after = subprocess.check_output(accessible_command, text=True).strip()
            geometry = evaluate('var p=panels(); print(JSON.stringify(p.map(function(x){return {location:x.location,height:x.height};})));')
            geometry = json.loads(geometry)
            if len(geometry) != 1 or geometry[0]["location"] != location or geometry[0]["height"] != thickness:
                raise RuntimeError(f"Panel geometry mismatch: {geometry}, expected {location}/{thickness}")
            evidence = dict(scenario=args.scenario, theme=args.theme, scale=args.scale,
                            orientation=orientation, requestedThickness=thickness,
                            panelGeometry=geometry, panelCreation=creation,
                            plasmashellPid=plasma.pid, markerBefore=before, markerAfter=after,
                            image=image.name, sha256=hashlib.sha256(data).hexdigest(),
                            physicalDesktop="unverified", keyboardNavigation="unverified",
                            visualInspection="unverified", accessibility="PID-bound AT-SPI name before and after capture")
            (args.output / f"{name}.json").write_text(json.dumps(evidence, indent=2) + "\n")
            print(f"Captured {name}", flush=True)
    finally:
        for process in reversed(services):
            process.terminate()
        for process in reversed(services):
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def outer(args):
    args.output.mkdir(parents=True, exist_ok=True)
    if list(args.output.glob("*.json")):
        raise SystemExit("Use a new output directory; existing evidence must not be reused")
    before = identity()
    run(["cmake", "-S", str(ROOT), "-B", str(args.build_dir)])
    run(["cmake", "--build", str(args.build_dir), "--target", "aiusagemonitorplugin"])
    with tempfile.TemporaryDirectory(prefix="aiusage-panel-matrix-") as directory:
        temporary = Path(directory)
        prefix = temporary / "prefix"
        run(["cmake", "--install", str(args.build_dir), "--prefix", str(prefix)], stdout=subprocess.DEVNULL)
        qml = next(prefix.glob("**/qt6/qml"))
        # Read the package metadata instead of starting plasmashell outside a
        # session. In container runners the shell can abort while initializing
        # Qt, even though the installed Plasma package is usable by KWin.
        plasma_version = subprocess.check_output(
            ["rpm", "-q", "--qf", "%{VERSION}", "plasma-workspace"], text=True
        ).strip()
        if not plasma_version:
            raise RuntimeError("Could not determine the installed Plasma version")
        sessions = list(itertools.product(args.scenarios, args.scales, args.themes))
        for index, (scenario, scale, theme) in enumerate(sessions):
            session = temporary / f"session-{index}"
            for folder in ("home", "config", "cache", "data", "runtime"):
                (session / folder).mkdir(parents=True, mode=0o700)
            (session / "home/.matrix-isolated").touch()
            (session / "config/plasma-welcomerc").write_text(f"[General]\nLastSeenVersion={plasma_version}\n")
            (session / "config/kded6rc").write_text(
                "[Module-plasma-welcome]\nautoload=false\n[Module-kded_plasma_welcome]\nautoload=false\n"
            )
            colors = "BreezeDark" if theme == "dark" else "BreezeLight"
            shutil.copyfile(f"/usr/share/color-schemes/{colors}.colors", session / "config/kdeglobals")
            (session / "config/plasmarc").write_text(f"[Theme]\nname={'breeze-dark' if theme == 'dark' else 'default'}\n")
            env = dict(os.environ, HOME=str(session / "home"), XDG_CONFIG_HOME=str(session / "config"),
                       XDG_CACHE_HOME=str(session / "cache"), XDG_DATA_HOME=str(session / "data"),
                       XDG_RUNTIME_DIR=str(session / "runtime"), XDG_DATA_DIRS=f"{prefix}/share:/usr/local/share:/usr/share",
                       QML_IMPORT_PATH=str(qml), QML2_IMPORT_PATH=str(qml),
                       KDE_COLOR_SCHEME_PATH=f"/usr/share/color-schemes/{colors}.colors",
                       PLASMA_AI_MONITOR_DEMO="1", PLASMA_AI_MONITOR_SMOKE_VIEW=scenario,
                       QT_QUICK_BACKEND="software", LIBGL_ALWAYS_SOFTWARE="1", QT_IM_MODULE="", GTK_IM_MODULE="",
                       LANG="C.UTF-8", LANGUAGE="en", LC_ALL="C.UTF-8",
                       QT_ACCESSIBILITY="1", QT_LINUX_ACCESSIBILITY_ALWAYS_ON="1",
                       QT_LOGGING_TO_CONSOLE="1", QT_QPA_PLATFORM="wayland",
                       XDG_CURRENT_DESKTOP="KDE", XDG_SESSION_TYPE="wayland", AI_USAGE_MATRIX_ISOLATED="1")
            if scenario == "plugin-recovery":
                env["QML_IMPORT_PATH"] = f"{ROOT}/scripts/fixtures/bootstrap/plugin-older:{qml}"
                env["QML2_IMPORT_PATH"] = env["QML_IMPORT_PATH"]
            command = [sys.executable, str(Path(__file__).resolve()), "--inner", "--output", str(args.output),
                       "--scenario", scenario, "--scale", str(scale), "--theme", theme,
                       "--sizes", *map(str, args.sizes), "--orientations", *args.orientations]
            launcher = session / "launch"
            launcher.write_text("#!/bin/sh\nexec " + shlex.join(command) + "\n")
            launcher.chmod(0o700)
            log = args.output / f"session-{scenario}-{theme}-{scale:g}.log"
            with log.open("w") as stream:
                run(["dbus-run-session", "--", "kwin_wayland", "--virtual", "--socket", "wayland-panel-matrix",
                     "--width", "1600", "--height", "1000", "--scale", str(scale), "--no-lockscreen",
                     "--no-global-shortcuts", "--exit-with-session", str(launcher)], env=env,
                    stdout=stream, stderr=subprocess.STDOUT, timeout=240)
            # KWin exit alone is not evidence of inner completion.
            for orientation, size in itertools.product(args.orientations, args.sizes):
                path = args.output / f"{scenario}-{theme}-{scale:g}-{orientation}-{size}.json"
                if not path.is_file():
                    raise RuntimeError(f"Missing panel evidence: {path}; inspect {log}")
        after = identity()
        if before != after:
            raise RuntimeError("Source changed during matrix capture; no qualification manifest written")
        manifest = dict(sourceTreeSha256=before, candidateCommit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            cases=len(sessions)*len(args.orientations)*len(args.sizes),
            scenarios=args.scenarios, scales=args.scales, themes=args.themes, sizes=args.sizes,
            orientations=args.orientations, artifacts={path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                for path in sorted(args.output.glob("*.json"))}, physicalDesktop="unverified", keyboardNavigation="unverified")
        (args.output / "matrix-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"Panel matrix PASS: {manifest['cases']} isolated real Plasma captures")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/debug")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scenarios", nargs="+", choices=SCENARIOS, default=list(SCENARIOS))
    parser.add_argument("--scales", nargs="+", type=float, default=[1, 1.25, 1.5, 2])
    parser.add_argument("--themes", nargs="+", choices=("light", "dark"), default=["light", "dark"])
    parser.add_argument("--sizes", nargs="+", type=int, choices=(24, 32, 48, 64), default=[24, 32, 48, 64])
    parser.add_argument("--orientations", nargs="+", choices=("horizontal", "vertical"), default=["horizontal", "vertical"])
    parser.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--scenario", choices=SCENARIOS, help=argparse.SUPPRESS)
    parser.add_argument("--scale", type=float, help=argparse.SUPPRESS)
    parser.add_argument("--theme", help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.build_dir = args.build_dir.resolve()
    (inner if args.inner else outer)(args)


if __name__ == "__main__":
    main()
