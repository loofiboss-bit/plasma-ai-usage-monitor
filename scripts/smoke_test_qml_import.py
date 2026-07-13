#!/usr/bin/env python3
import sys
import os
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

def main():
    parser = argparse.ArgumentParser(description="Smoke test QML import.")
    parser.add_argument("--strict", action="store_true", help="Fail hard if PyQt6 is missing or module cannot be imported.")
    parser.add_argument(
        "--expected-version",
        default=(ROOT / "VERSION").read_text(encoding="utf-8").strip(),
        help="Expected version string (defaults to VERSION).",
    )
    parser.add_argument("--build-dir", default="build", help="Configured CMake build directory.")
    args = parser.parse_args()

    try:
        from PyQt6.QtCore import QUrl
        from PyQt6.QtQml import QQmlEngine, QQmlComponent
        from PyQt6.QtGui import QGuiApplication
    except ImportError:
        print("PyQt6 not installed.")
        if args.strict:
            sys.exit(1)
        else:
            print("Skipping QML smoke test.")
            sys.exit(0)

    # To use QQmlEngine, we need a QCoreApplication or QGuiApplication
    # We slice sys.argv so Qt doesn't consume our custom arguments
    # Create a temporary QML module directory structure so QQmlEngine can find it
    import shutil
    
    temp_qml_dir = os.path.abspath(os.path.join(args.build_dir, "qml-smoke"))
    if os.path.exists(temp_qml_dir):
        shutil.rmtree(temp_qml_dir)
    try:
        module_path = os.path.join(temp_qml_dir, "com", "github", "loofi", "aiusagemonitor")
        os.makedirs(module_path, exist_ok=True)
        
        # We need qmldir and the compiled .so
        plugin_root = Path(args.build_dir).resolve() / "plugin"
        qmldir_candidates = list(plugin_root.rglob("qmldir"))
        qmldir_src = str(qmldir_candidates[0]) if qmldir_candidates else ""
        candidates = list(plugin_root.rglob("libaiusagemonitorplugin.so"))
        so_src = str(candidates[0]) if candidates else str(plugin_root / "libaiusagemonitorplugin.so")
        
        if qmldir_src and os.path.exists(qmldir_src):
            shutil.copy(qmldir_src, os.path.join(module_path, "qmldir"))
        if os.path.exists(so_src):
            shutil.copy(so_src, os.path.join(module_path, "libaiusagemonitorplugin.so"))
            
        app = QGuiApplication(sys.argv[:1])
        engine = QQmlEngine()
        
        # Force the local build path ahead of any installed module so release
        # checks catch the repo build, not a stale system/user install.
        engine.setImportPathList([temp_qml_dir])
        if args.strict:
            print(f"QML smoke import paths: {engine.importPathList()}")
        
        # We want to check if the plugin is installed and importable.
        
        qml_content = b"""import QtQuick
import com.github.loofi.aiusagemonitor 1.0 as AiMonitor

Item {
    property string version: AiMonitor.AppInfo.version
}
"""
        component = QQmlComponent(engine)
        component.setData(qml_content, QUrl("smoke_test.qml"))

        if component.isError():
            errors = component.errors()
            error_msgs = "\n".join([e.toString() for e in errors])

            # Check if the error is specifically about not finding the module
            if "is not installed" in error_msgs and "com.github.loofi.aiusagemonitor" in error_msgs:
                if args.strict:
                    print("Error: The QML module 'com.github.loofi.aiusagemonitor' could not be found (strict mode).")
                    print(f"Full QML error trace:\n{error_msgs}")
                    sys.exit(1)
                else:
                    print("Warning: The QML module 'com.github.loofi.aiusagemonitor' could not be found.")
                    print("Skipping runtime version check. Are you sure the plugin is built and installed? Run 'just install'.")
                    sys.exit(0)
            else:
                print(f"QML Component Error:\n{error_msgs}")
                sys.exit(1)

        obj = component.create()
        if obj is None:
            print("Error: Could not instantiate the QML component.")
            sys.exit(1)

        version = obj.property("version")
        print(f"Smoke test successful. Plugin loaded, version: {version}")

        if version != args.expected_version:
            print(f"Error: Expected version {args.expected_version}, but loaded plugin reports {version}.")
            print("You might have a stale plugin shadowing the new build.")
            sys.exit(1)
    finally:
        shutil.rmtree(temp_qml_dir, ignore_errors=True)

if __name__ == '__main__':
    main()
