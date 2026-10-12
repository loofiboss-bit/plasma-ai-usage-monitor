# V24 Coding Sessions & Projects

- **Target release:** `24.2.0` (V24.0.0 was the initial session feature release)
- **Status:** the V24 session feature is published; 24.2.0 structures source setup and usage views and adds explicit project-alias management.

## Goal

Give individual developers a consistent local view of coding sessions,
projects, models, and tool-reported tokens from Codex and Claude Code while
preserving the existing provider integrations, quota cards, budget controls,
and three primary views.

## Product decisions

- Each tool has a separate, explicit local opt-in that defaults off and takes
  effect only after Settings are applied.
- The first scan is limited to the latest 30 days and the configured history
  retention period. Import is incremental, resumable, bounded, and does not
  install hooks or launch the tools.
- Local project identities are anonymous; user aliases stay on the device.
- Sessions can be deleted across widget instances. A deletion watermark
  prevents automatic restoration of older records.
- JSON and CSV session exports use a dedicated schema and export-local
  anonymous identities. They omit aliases, paths, credentials, and raw local
  session identities.
- Quotas remain account-level provider data. Session tokens are tool-reported
  local usage and are not converted to subscription percentages, cost, or
  productivity measures.

## Data and collection

Use bounded C++ JSONL collectors for the configured Codex and Claude Code
directories. Run discovery and parsing in the background, persist progress and
parsed events atomically, and serialize imports across processes. Store only
allowlisted metadata and usage fields. Generate session, project, and file
identities with a random per-user HMAC key; never persist raw paths, prompts,
replies, conversation titles, or tool content.

Codex token-count snapshots are cumulative: validate category counters, add
only their deltas, ignore duplicate snapshots, and mark counter resets or
unverified fork/agent lineage as partial coverage. Claude assistant messages
deduplicate by message ID and may be completed by a later record. Conflicts,
ambiguous identities, unsupported formats, and unavailable usage remain
visible as partial or unavailable data. Cache read and cache write are input
subcategories and are not added twice.

Move the SQLite history schema from v8 to v9 atomically, with a WAL-aware
verified backup. Keep session data, usage events, file checkpoints, project
aliases, and observed quotas in separate tables. Preserve existing provider
history, budget policy behavior, configuration backups, and export formats.
Apply retention to events and indicate when it leaves partial session
coverage.

## User interface

- Settings exposes independent consent, persistent import status, project
  aliases, rescan, and shared deletion.
- Overview cards show today's observed session count and latest activity, and
  open the existing Source Detail view.
- Source Detail places filtered sessions after the quota summary and shows
  dates, model, input/output/cache categories, and coverage.
- History and Analyst add a sessions mode with daily usage, model/project
  breakdowns, and the same filters and aggregation rules.
- Queries are asynchronous, ignore stale responses, and page at 100 sessions.
  Local calendar-day boundaries are converted to UTC for storage and queries.
  Navigation preserves filters, selection, and keyboard focus.

## Qualification

Cover parser deltas, deduplication, partial records, cache semantics, model
changes, forks/agents, rotation, truncation, migration rollback, cross-instance
idempotence, retention, clear-during-import, privacy sentinels, export
allowlists, date boundaries, and late query responses. Run Debug/CTest, project
checks, QML lint, sanitizers, release checks, package-upgrade preservation, and
100,000-event performance checks. Capture deterministic synthetic screenshots
and document remaining physical display, keyboard, and screen-reader checks.

The original V24 implementation task did not include public release. The
V24.0.1 layout hotfix has separate publication authorization and is qualified
by `docs/release/v24.0.1-checklist.md`.
