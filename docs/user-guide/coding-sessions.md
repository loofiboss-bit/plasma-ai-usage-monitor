# Coding sessions and projects

V24 can keep a local history of reported token usage from Codex and Claude
Code. This is separate from the tools' quota cards: session tokens describe
what the local tool recorded, and are not a subscription percentage, bill, or
productivity measure.

## Enable a tool

Open **Settings → History**, enable **Codex session data** or **Claude Code
session data**, then press **Apply**. Each tool has its own opt-in. The first
import scans supported local log files from up to the last 30 calendar days,
bounded by the History retention setting. Import and parsing run in the
background and can resume after the widget restarts. The widget never installs
hooks or launches either tool. Enabled tools are rescanned every minute,
with at most one importer active. Open session views refresh when import
finishes. Failed database writes roll back the file transaction, including
counters and checkpoints, so the next scan can retry.

The settings page shows whether the import completed, is partial, or could not
read a supported log format. Unsupported, truncated, unreadable, or ambiguous
records remain excluded from verified totals. **Rescan local sessions** asks
the importer to continue discovery. **Delete session history** removes the
shared local session records and project aliases; it does not remove the
original tool logs.

## Project identities

Projects are represented with anonymous local identities, not filesystem
paths. Optional aliases can be assigned in Settings and are stored only on
this computer. Clearing session history also clears aliases. Filters use the
same anonymous project identity in Source Detail and History. Explore sessions,
daily totals, and projects in **History → Sessions**, using **Sessions**,
**By Day**, and **By Project**. Source Detail provides a compact summary and
a direct link to History.

## Read the token values

Input, output, cache-read and cache-write are separate categories. A value can
be unavailable when the source did not report it; unavailable is not zero.
Repeated Codex cumulative snapshots do not add usage again. Counter resets or
unverified fork/agent lineage mark coverage as partial and start a new safe
baseline. Claude Code messages are deduplicated by message identity, and a
later complete usage record updates that observation instead of adding a
second copy. A conflicting Claude message identity remains excluded even if
the original value reappears later, until session history is deleted. Codex
models come from session or turn context; usage remains available when the
model is unknown. Claude cache-read and cache-write values are input subtotals and
are not counted a second time on top of input.

Local calendar-day filters use the system time zone, with UTC boundaries in the
database. **Today**, **7 days**, and **30 days** use the same filters and
summary rules in all session views. Account quotas remain separate and retain
their existing provider-reported meaning.

## Export

The session export action writes the selected tool, period, and project filter
as JSON or CSV using a dedicated schema version 1. Each export assigns its own
anonymous IDs. It excludes aliases, local paths, credentials, and raw tool or
session identities. Export is manual and asynchronous; the widget shows the
result or an error after the write finishes. Session data is excluded from
configuration backups and scheduled provider-history exports.

## Data handling

Only supported session metadata and usage fields are retained. Prompts,
assistant replies, conversation titles, and full project paths are never
stored or written to diagnostics. Imported session records, aliases, and
private identity material stay in local application storage. See [Privacy and
security](privacy-and-security.md) for the wider data boundary.
