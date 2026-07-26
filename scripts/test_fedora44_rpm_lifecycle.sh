#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 BASE_RPM CANDIDATE_RPM" >&2
  exit 2
fi

command -v podman >/dev/null || { echo "podman is required" >&2; exit 1; }
base_rpm="$(realpath "$1")"
candidate_rpm="$(realpath "$2")"
[[ -f "$base_rpm" && -f "$candidate_rpm" ]] || { echo "Both RPM paths must exist" >&2; exit 1; }

expected_base="$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$base_rpm")"
expected_candidate="$(rpm -qp --qf '%{VERSION}-%{RELEASE}' "$candidate_rpm")"
base_version="${BASE_VERSION:-14.1.1}"
[[ "$expected_base" == "$base_version-"* ]] || {
  echo "Expected the v${base_version} base RPM, got $expected_base" >&2
  exit 1
}
[[ "$expected_candidate" == "$(<"$(dirname "$0")/../VERSION")-"* ]] || {
  echo "Expected a v$(<"$(dirname "$0")/../VERSION") candidate RPM, got $expected_candidate" >&2
  exit 1
}
image="${FEDORA_CONTAINER_IMAGE:-fedora:44}"

run_lifecycle() {
  podman run --rm \
    -v "$base_rpm:/rpms/base.rpm:ro,Z" \
    -e EXPECTED_BASE="$expected_base" \
    -v "$candidate_rpm:/rpms/candidate.rpm:ro,Z" \
    -e EXPECTED_CANDIDATE="$expected_candidate" \
    "$image" bash -lc '
      set -euo pipefail
      dnf install -y --setopt=install_weak_deps=False /rpms/base.rpm
      dnf install -y --setopt=install_weak_deps=False sqlite
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_BASE"
      install -d /root/.local/share/plasma-ai-usage-monitor /root/.local/share/kwalletd /root/.config
      history_db=/root/.local/share/plasma-ai-usage-monitor/usage_history.db
      sqlite3 "$history_db" <<SQL
PRAGMA user_version = 4;
CREATE TABLE observations (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  provider TEXT NOT NULL,
  observed_at_utc DATETIME NOT NULL,
  metric_kind TEXT NOT NULL,
  unit TEXT NOT NULL,
  value REAL NULL,
  semantic TEXT NOT NULL,
  source TEXT NOT NULL,
  data_quality TEXT NOT NULL,
  scope TEXT NOT NULL,
  window TEXT NOT NULL,
  correlation_id TEXT NOT NULL
);
INSERT INTO observations
  (provider, observed_at_utc, metric_kind, unit, value, semantic, source,
   data_quality, scope, window, correlation_id)
VALUES
  (1, 1, 1, 1, NULL, 1, 1, 1, 1, 1, 1),
  (1, 2, 1, 1, 0, 1, 1, 1, 1, 1, 2);
SQL
      cat > /root/.config/plasma-org.kde.plasma.desktop-appletsrc <<CONFIG
[Containments][1][Applets][1][Configuration][General]
historyEnabled=true
openaiEnabled=true
compactMode=lowestQuota
CONFIG
      printf "base-wallet-fixture\n" > /root/.local/share/kwalletd/kdewallet.kwl
      fixture_hashes="$(sha256sum \
        "$history_db" \
        /root/.config/plasma-org.kde.plasma.desktop-appletsrc \
        /root/.local/share/kwalletd/kdewallet.kwl)"

      assert_user_data() {
        test "$fixture_hashes" = "$(sha256sum \
          "$history_db" \
          /root/.config/plasma-org.kde.plasma.desktop-appletsrc \
          /root/.local/share/kwalletd/kdewallet.kwl)"
        test "$(sqlite3 "$history_db" "PRAGMA user_version")" = 4
        test "$(sqlite3 "$history_db" \
          "SELECT value IS NULL FROM observations WHERE id = 1")" = 1
        test "$(sqlite3 "$history_db" \
          "SELECT value = 0 FROM observations WHERE id = 2")" = 1
        grep -Fq "historyEnabled=true" /root/.config/plasma-org.kde.plasma.desktop-appletsrc
        grep -Fxq "base-wallet-fixture" /root/.local/share/kwalletd/kdewallet.kwl
      }

      dnf install -y --setopt=install_weak_deps=False /rpms/candidate.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_CANDIDATE"
      assert_user_data
      test -f /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      test -f /usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/libaiusagemonitorplugin.so
      grep -Fq "\"Id\": \"com.github.loofi.aiusagemonitor\"" \
        /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      dnf downgrade -y --setopt=install_weak_deps=False /rpms/base.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_BASE"
      assert_user_data
      dnf install -y --setopt=install_weak_deps=False /rpms/candidate.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_CANDIDATE"
      assert_user_data
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      assert_user_data
      dnf install -y --setopt=install_weak_deps=False /rpms/candidate.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_CANDIDATE"
      assert_user_data
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      assert_user_data
    '
}

run_clean_install() {
  podman run --rm \
    -v "$candidate_rpm:/rpms/candidate.rpm:ro,Z" \
    -e EXPECTED_CANDIDATE="$expected_candidate" \
    "$image" bash -lc '
      set -euo pipefail
      ! rpm -q plasma-ai-usage-monitor
      dnf install -y --setopt=install_weak_deps=False /rpms/candidate.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_CANDIDATE"
      test -f /usr/share/plasma/plasmoids/com.github.loofi.aiusagemonitor/metadata.json
      test -f /usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/libaiusagemonitorplugin.so
      install -d /root/.local/share/plasma-ai-usage-monitor
      printf "clean-install-history\n" > /root/.local/share/plasma-ai-usage-monitor/usage_history.db
      dnf remove -y --no-autoremove plasma-ai-usage-monitor
      ! rpm -q plasma-ai-usage-monitor
      grep -Fxq "clean-install-history" /root/.local/share/plasma-ai-usage-monitor/usage_history.db
      dnf install -y --setopt=install_weak_deps=False /rpms/candidate.rpm
      test "$(rpm -q --qf "%{VERSION}-%{RELEASE}" plasma-ai-usage-monitor)" = "$EXPECTED_CANDIDATE"
      grep -Fxq "clean-install-history" /root/.local/share/plasma-ai-usage-monitor/usage_history.db
    '
}

run_lifecycle
run_clean_install
echo "Fedora 44 RPM lifecycle PASS: base upgrade, rollback, re-upgrade, clean candidate install, reinstall, removal, and user-data preservation"
