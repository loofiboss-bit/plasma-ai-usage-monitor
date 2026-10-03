# V22 — Reliable Everyday Control

- **Target release:** `22.0.0`

Status: implementation qualified in PR #67; publication in progress under the
explicit V22 release request. This plan targets reliable
existing everyday workflows; new providers, cloud sync, new dependencies and
broad UI redesign are excluded. Qualification is tracked in
[the V22 checklist](../release/v22.0.0-checklist.md).

## Configuration and first success

- Prepare validated v2/v3 configuration import drafts without writing stores.
- Apply owner-isolated policies transactionally before KConfig; failure leaves
  saved values unchanged. Discard releases the draft.
- Keep drafts alive across failed page-switch Apply using a window-owned object
  created from library context. Guard native OK's immediate failed-Apply close,
  without preventing later deliberate Cancel.
- Expose localized backup, import and save results; keep secrets in KWallet.
- Resume setup against current readiness with a fresh timeout for an active
  check or return an interrupted check to configuration. Distinguish actual
  quota, local estimate, waiting activity and connection-only results.

## Local watching

- Configure paths before installation detection, then establish a baseline.
- Debounce event-driven scans outside the UI thread, bound discovery to 4,000
  entries per root, refresh watches for new sessions/replaced files and report
  incomplete coverage through source health.
- Cancel scanning/debounce/watchers on disable and reject late generations.
  Re-enable establishes a new baseline; activity remains an estimate.

## History and export

- Initialize settings storage and make database/WAL statistics and pruning
  asynchronous, with request IDs, stable errors and deleted-row counts.
- Show busy, success/partial/failure, written paths and the latest scheduled
  export outcome. Prevent overlapping operations and write atomically per file.
- Export policy transitions and channel outcomes via full-history JSON schema
  v7 and separate policy-events CSV with explicit safe field lists. The
  selected-series JSON remains v6; config backups remain v3 with v2 import.

## Budget delivery and persistence

- Upgrade real SQLite v7 to v8 transactionally after checkpoint and a
  `.v21-backup`, preserving observations/policies/events and inventing no old
  channel receipts. Older binaries must not open migrated storage.
- Persist KDE, Slack and Discord outcomes per event/channel. KDE acceptance
  means desktop submission; webhooks require HTTP 2xx. Neither proves reading.
- Use internal policy/event/channel correlation and independent cooldown.
  Temporary failures allow at most three attempts with Retry-After; permanent
  errors require configuration repair. Retry only unacknowledged channels and
  suppress stale events/disabled channels with recorded reasons.
- Expose recent delivery results in Alerts without a new main page.

## Acceptance and rollout boundary

Regression coverage includes imports, native OK/Cancel/sidebar lifecycle,
setup resume/timeouts, bounded/cancelled watches, populated history maintenance,
export errors/privacy, migration rollback, concurrent policies and restart with
partially accepted channels. Full CTest, static checks, QML lint, sanitizers,
performance budgets and Fedora packaging/lifecycle checks form the local gate.

Physical Plasma and manual screen-reader qualification remain separate evidence.
Rollback uses an isolated copy of pre-migration schema v7; preserve the live v8
history. Implementation does not authorize commits, installation or public
publication. Public GitHub, COPR, KDE Store and wiki claims require their own
independent readback after authorization.
