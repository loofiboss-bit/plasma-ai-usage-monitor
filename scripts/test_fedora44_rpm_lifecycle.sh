#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 V13_RPM V14_RPM" >&2
  exit 2
fi

command -v podman >/dev/null || { echo "podman is required" >&2; exit 1; }
v13_rpm="$(realpath "$1")"
v14_rpm="$(realpath "$2")"
[[ -f "$v13_rpm" && -f "$v14_rpm" ]] || { echo "Both RPM paths must exist" >&2; exit 1; }

expected_v13="$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$v13_rpm")"
expected_v14="$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$v14_rpm")"
[[ "$expected_v13" == 13.0.0-* ]] || {
  echo "Expected a v13.0.0 RPM, got $expected_v13" >&2
  exit 1
}
[[ "$expected_v14" == "$(<"$(dirname "$0")/../VERSION")-"* ]] || {
  echo "Expected a v$(<"$(dirname "$0")/../VERSION") RPM, got $expected_v14" >&2
  exit 1
}
image="${FEDORA_CONTAINER_IMAGE:-fedora:44}"

run_lifecycle() {
  podman run --rm \
    -v "$v13_rpm:/rpms/v13.rpm:ro,Z" \
    -e EXPECTED_V13="$expected_v13" \
    -v "$v14_rpm:/rpms/v14.rpm:ro,Z" \
    -e EXPECTED_V14="$expected_v14" \
    "$image" bash -lc '
      set -euo pipefail
      dnf install -y /rpms/v13.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V13"
      install -d /root/.local/share/plasma-ai-usage-monitor /root/.local/share/kwalletd /root/.config
      printf "v13-history-fixture\n" > /root/.local/share/plasma-ai-usage-monitor/usage_history.db
      printf "v13-config-fixture\n" > /root/.config/plasma-org.kde.plasma.desktop-appletsrc
      printf "v13-wallet-fixture\n" > /root/.local/share/kwalletd/kdewallet.kwl
      fixture_hashes="$(sha256sum \
        /root/.local/share/plasma-ai-usage-monitor/usage_history.db \
        /root/.config/plasma-org.kde.plasma.desktop-appletsrc \
        /root/.local/share/kwalletd/kdewallet.kwl)"

      assert_user_data() {
        test "$fixture_hashes" = "$(sha256sum \
          /root/.local/share/plasma-ai-usage-monitor/usage_history.db \
          /root/.config/plasma-org.kde.plasma.desktop-appletsrc \
          /root/.local/share/kwalletd/kdewallet.kwl)"
      }

      dnf install -y /rpms/v14.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V14"
      assert_user_data
      test -f /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      test -f /usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/libaiusagemonitorplugin.so
      dnf downgrade -y /rpms/v13.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V13"
      assert_user_data
      dnf install -y /rpms/v14.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V14"
      assert_user_data
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      assert_user_data
      dnf install -y /rpms/v14.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V14"
      assert_user_data
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      assert_user_data
    '
}

run_clean_install() {
  podman run --rm \
    -v "$v14_rpm:/rpms/v14.rpm:ro,Z" \
    -e EXPECTED_V14="$expected_v14" \
    "$image" bash -lc '
      set -euo pipefail
      ! rpm -q plasma-ai-usage-monitor
      dnf install -y /rpms/v14.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V14"
      test -f /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      test -f /usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/libaiusagemonitorplugin.so
      install -d /root/.local/share/plasma-ai-usage-monitor
      printf "clean-install-history\n" > /root/.local/share/plasma-ai-usage-monitor/usage_history.db
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      grep -Fxq "clean-install-history" /root/.local/share/plasma-ai-usage-monitor/usage_history.db
      dnf install -y /rpms/v14.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_V14"
      grep -Fxq "clean-install-history" /root/.local/share/plasma-ai-usage-monitor/usage_history.db
    '
}

run_lifecycle
run_clean_install
echo "Fedora 44 RPM lifecycle PASS: v13 upgrade, rollback, re-upgrade, clean v14 install, reinstall, removal, and user-data preservation"
