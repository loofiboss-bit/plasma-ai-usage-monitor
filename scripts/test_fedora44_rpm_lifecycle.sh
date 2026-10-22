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
base_version="${BASE_VERSION:-24.2.0}"
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
PRAGMA user_version = 9;
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
CREATE TABLE budget_policies (
  owner_id TEXT NOT NULL, policy_id TEXT PRIMARY KEY,
  source_id TEXT NOT NULL, source_kind TEXT NOT NULL,
  scope_mode TEXT NOT NULL, scope_kind TEXT NOT NULL DEFAULT '\'''\'',
  scope_identity TEXT NOT NULL DEFAULT '\'''\'', scope_label TEXT NOT NULL DEFAULT '\'''\'',
  value_class TEXT NOT NULL, limit_minor INTEGER NOT NULL,
  currency TEXT NOT NULL, period_type TEXT NOT NULL, anchor_day INTEGER,
  time_zone_id TEXT NOT NULL, warning_percent INTEGER NOT NULL,
  critical_percent INTEGER NOT NULL, notify_enabled INTEGER NOT NULL,
  enabled INTEGER NOT NULL, created_at_utc TEXT NOT NULL,
  updated_at_utc TEXT NOT NULL, snoozed_until_utc TEXT
);
INSERT INTO budget_policies VALUES (
  '\''fixture-owner'\'','\''00000000-0000-4000-8000-000000000024'\'','\''openai'\'','\''provider'\'',
  '\''aggregate'\'','\'''\'','\'''\'','\'''\'', '\''actual'\'',2500,'\''USD'\'','\''calendar_month'\'',NULL,'\''UTC'\'',80,90,
  1,1,'\''2026-10-01T00:00:00.000Z'\'','\''2026-10-01T00:00:00.000Z'\'',NULL
);
CREATE TABLE local_sessions (
  session_id TEXT PRIMARY KEY, tool_id TEXT NOT NULL, project_id TEXT NOT NULL,
  started_at_utc TEXT NOT NULL, last_seen_at_utc TEXT NOT NULL,
  coverage TEXT NOT NULL DEFAULT '\''reported'\'', source_id TEXT NOT NULL DEFAULT '\'''\''
);
CREATE TABLE local_usage_events (
  event_id TEXT PRIMARY KEY, session_id TEXT NOT NULL, observed_at_utc TEXT NOT NULL,
  model_id TEXT NOT NULL DEFAULT '\'''\'', input_tokens INTEGER, output_tokens INTEGER,
  cache_read_tokens INTEGER, cache_write_tokens INTEGER, parser_revision INTEGER NOT NULL,
  provenance TEXT NOT NULL DEFAULT '\'''\''
);
CREATE TABLE local_project_aliases (project_id TEXT PRIMARY KEY, alias TEXT NOT NULL);
INSERT INTO local_sessions VALUES (
  '\''fixture-session'\'','\''codex'\'','\''fixture-project'\'',
  '\''2026-10-12T12:00:00.000Z'\'','\''2026-10-12T12:00:00.000Z'\'','\''reported'\'','\'''\''
);
INSERT INTO local_usage_events VALUES (
  '\''fixture-event'\'','\''fixture-session'\'','\''2026-10-12T12:00:00.000Z'\'',
  '\''fixture-model'\'',120,30,NULL,0,1,'\'''\''
);
INSERT INTO local_project_aliases VALUES ('\''fixture-project'\'','\''Preserved project'\'');
SQL
      cat > /root/.config/plasma-org.kde.plasma.desktop-appletsrc <<CONFIG
[Containments][1][Applets][1][Configuration][General]
historyEnabled=true
openaiEnabled=true
compactMode=lowestQuota
codexSessionHistoryEnabled=true
claudeCodeSessionHistoryEnabled=false
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
        test "$(sqlite3 "$history_db" "PRAGMA user_version")" = 9
        test "$(sqlite3 "$history_db" \
          "SELECT value IS NULL FROM observations WHERE id = 1")" = 1
        test "$(sqlite3 "$history_db" \
          "SELECT value = 0 FROM observations WHERE id = 2")" = 1
        test "$(sqlite3 "$history_db" \
          "SELECT limit_minor FROM budget_policies WHERE policy_id = '\''00000000-0000-4000-8000-000000000024'\''")" = 2500
        test "$(sqlite3 "$history_db" \
          "SELECT alias FROM local_project_aliases WHERE project_id = '\''fixture-project'\''")" = "Preserved project"
        test "$(sqlite3 "$history_db" \
          "SELECT input_tokens || '\'':'\'' || output_tokens || '\'':'\'' || (cache_read_tokens IS NULL) FROM local_usage_events WHERE event_id = '\''fixture-event'\''")" = "120:30:1"
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
