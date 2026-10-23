#!/usr/bin/env python3
"""Ensure Fedora gate arguments reject unsupported targets before host checks."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class FedoraGateArguments(unittest.TestCase):
    def run_gate(self, name, *args, **env):
        return subprocess.run(
            ["bash", str(ROOT / "scripts" / name), *args],
            env={**os.environ, **env}, capture_output=True, text=True, check=False,
        )

    def test_release_gate_rejects_unsupported_versions(self):
        for version in ("43", "46", "rawhide", "45 extra"):
            with self.subTest(version=version):
                result = self.run_gate("fedora44_check.sh", version)
                self.assertEqual(result.returncode, 2)
                self.assertIn("Usage:", result.stderr)
                self.assertNotIn("RPM package", result.stdout)

    def test_lifecycle_selects_version_and_keeps_signature_default(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = Path(temp)
            (fixture / "base.rpm").touch()
            (fixture / "candidate.rpm").touch()
            rpm = fixture / "rpm"
            rpm.write_text('#!/bin/bash\nif [[ "$*" == *base.rpm* ]]; then echo 24.2.0-1.fc44; else echo 25.0.0-1.fc45; fi\n')
            rpm.chmod(0o755)
            podman = fixture / "podman"
            podman.write_text('#!/bin/bash\nprintf "%s\\n" "$@" >> "$GATE_CAPTURE"\n')
            podman.chmod(0o755)
            for version, image in ((None, "custom-fedora44-env"), ("45", "registry.fedoraproject.org/fedora@sha256:" + "a" * 64)):
                with self.subTest(version=version, image=image):
                    capture = fixture / "calls.txt"
                    capture.write_text("")
                    args = [str(fixture / "base.rpm"), str(fixture / "candidate.rpm")]
                    if version:
                        args.append(version)
                    result = self.run_gate(
                        "test_fedora44_rpm_lifecycle.sh", *args,
                        PATH=f"{fixture}:{os.environ['PATH']}", GATE_CAPTURE=str(capture),
                        FEDORA_CONTAINER_IMAGE=image,
                        ALLOW_UNSIGNED_LOCAL_TEST_RPMS="0", BASE_VERSION="24.2.0",
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    calls = capture.read_text()
                    self.assertIn(f"EXPECTED_FEDORA={version or '44'}", calls)
                    self.assertIn("ALLOW_UNSIGNED_LOCAL_TEST_RPMS=0", calls)
                    self.assertIn(image, calls)
                    self.assertEqual(calls.count("--rm\n"), 2)

    def test_local_signature_policy_overrides_custom_image_defaults(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = Path(temp)
            (fixture / "base.rpm").touch()
            (fixture / "candidate.rpm").touch()
            (fixture / "os-release").write_text("ID=fedora\nVERSION_ID=45\n")
            rpm = fixture / "rpm"
            rpm.write_text('#!/bin/bash\nif [[ "$*" == *base.rpm* ]]; then echo 24.2.0-1.fc44; else echo 25.0.0-1.fc45; fi\n')
            rpm.chmod(0o755)
            dnf = fixture / "dnf"
            # Stop at the first package command, before any fixture/user writes.
            dnf.write_text('#!/bin/bash\nprintf "%s\\n" "$@" > "$DNF_CAPTURE"\nexit 42\n')
            dnf.chmod(0o755)
            podman = fixture / "podman"
            podman.write_text("""#!/usr/bin/python3
import os
import sys
args = sys.argv[1:]
for index, arg in enumerate(args[:-1]):
    if arg == '-e':
        key, value = args[index + 1].split('=', 1)
        os.environ[key] = value
script = args[-1].replace('source /etc/os-release', 'source "$GATE_OS_RELEASE"')
os.execve('/usr/bin/bash', ['bash', '-c', script], os.environ)
""")
            podman.chmod(0o755)
            for flag, expected in (("0", "True"), ("1", "False")):
                with self.subTest(unsigned=flag):
                    capture = fixture / "dnf-args.txt"
                    result = self.run_gate(
                        "test_fedora44_rpm_lifecycle.sh", str(fixture / "base.rpm"),
                        str(fixture / "candidate.rpm"), "45",
                        PATH=f"{fixture}:{os.environ['PATH']}",
                        GATE_OS_RELEASE=str(fixture / "os-release"),
                        DNF_CAPTURE=str(capture), FEDORA_CONTAINER_IMAGE="custom-fedora45",
                        ALLOW_UNSIGNED_LOCAL_TEST_RPMS=flag, BASE_VERSION="24.2.0",
                    )
                    self.assertEqual(result.returncode, 42, result.stderr)
                    options = capture.read_text().splitlines()
                    self.assertIn(f"--setopt=localpkg_gpgcheck={expected}", options)
                    self.assertNotIn(f"--setopt=localpkg_gpgcheck={'False' if expected == 'True' else 'True'}", options)

    def test_lifecycle_rejects_unsupported_target(self):
        result = self.run_gate("test_fedora44_rpm_lifecycle.sh", "base.rpm", "candidate.rpm", "46")
        self.assertEqual(result.returncode, 2)
        self.assertIn("Only Fedora 44 and 45", result.stderr)

    def test_lifecycle_rejects_ambiguous_signature_opt_in(self):
        result = self.run_gate(
            "test_fedora44_rpm_lifecycle.sh", "base.rpm", "candidate.rpm", "45",
            ALLOW_UNSIGNED_LOCAL_TEST_RPMS="yes",
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("must be 0 or 1", result.stderr)


if __name__ == "__main__":
    unittest.main()
