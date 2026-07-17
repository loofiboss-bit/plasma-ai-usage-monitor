#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 V12_RPM V13_RPM" >&2
  exit 2
fi

command -v podman >/dev/null || { echo "podman is required" >&2; exit 1; }
v12_rpm="$(realpath "$1")"
v13_rpm="$(realpath "$2")"
[[ -f "$v12_rpm" && -f "$v13_rpm" ]] || { echo "Both RPM paths must exist" >&2; exit 1; }

expected_v12="$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$v12_rpm")"
expected_v13="$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$v13_rpm")"
[[ "$expected_v12" == 12.0.3-* ]] || {
  echo "Expected a v12.0.3 RPM, got $expected_v12" >&2
  exit 1
}
[[ "$expected_v13" == "$(<"$(dirname "$0")/../VERSION")-"* ]] || {
  echo "Expected a v$(<"$(dirname "$0")/../VERSION") RPM, got $expected_v13" >&2
  exit 1
}
image="${FEDORA_CONTAINER_IMAGE:-fedora:44}"

run_lifecycle() {
  podman run --rm \
    -v "$v12_rpm:/rpms/v12.rpm:ro,Z" \
    -v "$v13_rpm:/rpms/v13.rpm:ro,Z" \
    -e EXPECTED_V12="$expected_v12" \
    -e EXPECTED_V13="$expected_v13" \
    "$image" bash -lc '
      set -euo pipefail
      dnf install -y /rpms/v12.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V12"
      install -d /root/.local/share/plasma-ai-usage-monitor /root/.config
      : > /root/.local/share/plasma-ai-usage-monitor/user-fixture.db
      : > /root/.config/plasma-ai-usage-monitor-fixture.conf
      dnf install -y /rpms/v13.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V13"
      test -f /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      test -f /usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/libaiusagemonitorplugin.so
      dnf downgrade -y /rpms/v12.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V12"
      dnf install -y /rpms/v13.rpm
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      test -f /root/.local/share/plasma-ai-usage-monitor/user-fixture.db
      test -f /root/.config/plasma-ai-usage-monitor-fixture.conf
    '
}

run_clean_install() {
  podman run --rm \
    -v "$v13_rpm:/rpms/v13.rpm:ro,Z" \
    -e EXPECTED_V13="$expected_v13" \
    "$image" bash -lc '
      set -euo pipefail
      ! rpm -q plasma-ai-usage-monitor
      dnf install -y /rpms/v13.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V13"
      test -f /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      test -f /usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/libaiusagemonitorplugin.so
      install -d /root/.local/share/plasma-ai-usage-monitor
      : > /root/.local/share/plasma-ai-usage-monitor/user-fixture.db
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      test -f /root/.local/share/plasma-ai-usage-monitor/user-fixture.db
    '
}

run_lifecycle
run_clean_install
echo "Fedora 44 RPM lifecycle PASS: v12 upgrade, rollback, clean v13 install, removal, and user-data preservation"
