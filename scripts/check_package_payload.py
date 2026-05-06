#!/usr/bin/env python3
import os
import re
import sys
import json
import tarfile

def expected_version_from_cmake():
    try:
        with open("CMakeLists.txt", "r", encoding="utf-8") as f:
            match = re.search(r"project\(plasma-ai-usage-monitor VERSION ([0-9]+\.[0-9]+\.[0-9]+)\)", f.read())
            if match:
                return match.group(1)
    except OSError:
        pass
    return "8.0.0"

def main():
    expected_version = expected_version_from_cmake()
    
    expected_files_in_source = [
        "package/metadata.json",
        "package/contents/ui/main.qml",
        "package/contents/ui/SetupWizard.qml",
        "package/contents/catalog/providers-v3.json",
        "package/contents/catalog/subscriptions-v1.json",
        "package/contents/icons/providers/openai.svg",
        "package/contents/icons/tools/copilot.svg",
        "plugin/qmldir",
        "com.github.loofi.aiusagemonitor.metainfo.xml"
    ]
    
    missing = False
    
    for f in expected_files_in_source:
        if not os.path.exists(f):
            print(f"Error: Missing expected source file {f}")
            missing = True

    if missing:
        sys.exit(1)
        
    try:
        with open("package/metadata.json", "r") as f:
            metadata = json.load(f)
            version = metadata.get("KPlugin", {}).get("Version", metadata.get("Version"))
            if version != expected_version:
                print(f"Error: package/metadata.json version is {version}, expected {expected_version}")
                missing = True
    except Exception as e:
        print(f"Error reading package/metadata.json: {e}")
        missing = True

    # Look for a .tar.gz package in dist or root
    package_name = f"plasma-ai-usage-monitor-{expected_version}.tar.gz"
    package_path = os.path.join(os.getcwd(), package_name)
    if not os.path.exists(package_path):
        package_path = os.path.join("dist", package_name)
        
    if os.path.exists(package_path):
        print(f"Found built package at {package_path}, verifying contents...")
        try:
            with tarfile.open(package_path, "r:gz") as tar:
                names = tar.getnames()
                
                # Check for key files in the tarball
                tar_expected = [
                    f"plasma-ai-usage-monitor-{expected_version}/package/metadata.json",
                    f"plasma-ai-usage-monitor-{expected_version}/package/contents/catalog/providers-v3.json",
                    f"plasma-ai-usage-monitor-{expected_version}/package/contents/catalog/subscriptions-v1.json",
                    f"plasma-ai-usage-monitor-{expected_version}/package/contents/icons/providers/openai.svg",
                    f"plasma-ai-usage-monitor-{expected_version}/package/contents/icons/tools/copilot.svg",
                    f"plasma-ai-usage-monitor-{expected_version}/plugin/qmldir",
                    f"plasma-ai-usage-monitor-{expected_version}/CMakeLists.txt"
                ]
                
                for expected in tar_expected:
                    if expected not in names:
                        print(f"Error: Tarball is missing {expected}")
                        missing = True
        except Exception as e:
            print(f"Error reading tarball: {e}")
            missing = True
    else:
        print(f"Note: No built package {package_name} found to verify payload. Run 'just package' to build one.")

    if missing:
        sys.exit(1)
    else:
        print("Package payload verification OK.")

if __name__ == '__main__':
    main()
