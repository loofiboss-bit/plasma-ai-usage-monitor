# Coding sessions and projects

V25 can keep a local history of reported token usage from Codex and Claude
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
read a supported log format. Codex and Claude Code each show their current scan,
latest completed scan, imported-event count and any limit or error. A successful
latest scan does not erase a historical coverage gap. Unsupported, truncated,
unreadable, or ambiguous records remain excluded from verified totals. While a
scan is running, the rescan action is disabled. **Rescan local sessions** asks
the importer to continue discovery. **Delete session history** removes the
shared local session records and project aliases; it does not remove the
original tool logs. After deletion, existing Codex sessions establish a new
cumulative baseline before counting later increases. Empty rescans and restarts
do not bring deleted totals back. A session proven by valid creation metadata
to have started after deletion can count its first snapshot; otherwise its
first snapshot is conservatively excluded with partial coverage. No list of
deleted sessions or old counters is retained.

Oversized log records are skipped in bounded chunks, so later supported records
can still import. A skipped or incomplete record marks coverage as partial;
rescans resume without treating a record fragment as a new event. Completed
unchanged files do not consume the per-scan file allowance. Changed files rotate
between scans, with new files receiving priority while pending files retain a
share of each scan. Each scan discovers at most 50,000 files, processes at most
5,000 files and reads at most 256 MiB; an individual file over 100 MiB is
skipped. A reached discovery or processing limit is shown for that tool, and
files beyond the discovery limit are not guaranteed to be found.

## Project identities

Projects are represented with anonymous local identities, not filesystem
paths. In **Settings → History → Project aliases**, choose **Create project**,
select the folder used by your coding tool, and enter your own name. Only saved
aliases appear in this list; imported anonymous projects are not pre-created
entries. The folder is used to match the anonymous identity and is not saved.
You can create an alias before importing sessions, rename it, or remove its name
without deleting session history. Aliases stay on this computer. Clearing
session history also clears aliases. Filters use the
same anonymous project identity in Source Detail and History. Explore sessions,
daily totals, and projects in **History → Sessions**, using **Sessions**,
**By Day**, and **By Project**. Source Detail provides a compact summary and
a direct link to History.

## Search and filter sessions

In History or Insights session mode, search the project chooser by your local
alias or the displayed anonymous project name. Matching ignores case and
searches the complete project list, including projects beyond the first 100
analysis rows. The active project remains visible even when it does not match
the chooser search. The no-match message counts search matches separately from
the pinned selection. A search with no matches does not remove stored history. Renaming
an alias changes its displayed name without changing the anonymous identity.

Search the model chooser without case sensitivity across every reported model
for the selected tool, project and period, including choices beyond the first
100 analysis rows. Typing only narrows the chooser; the active data filter
changes when you choose a model. A chooser search with no matches has its own
message and does not mean that session history is empty.

Choose **All models**, an exact reported model ID, or **Unknown model**. Model
choices reflect the selected tool, project and period before model filtering.
The selected model stays visible when the chooser search hides other choices
or when a different project or period has no matching model.
For a session that used several models, only usage events matching the selected
model contribute to its token values. Session counts count each matching
session once. Summary, daily totals, sessions, details and manual export use the
same active filters. Unavailable token categories remain unavailable.

Changing a filter resets the page and selected session. Changing the tool also
clears the project, model and chooser searches. Changing the project or period
keeps the model filter. If no sessions match, the active filters remain visible
with an explanation and a reset action.

**Next** is available only when the matching summary reports more sessions.
If an import or retention changes the number of pages, a completed query moves
to the last valid page, or the first page for an empty result. Project and model
analysis rows open History sessions with that filter while retaining other
filters. Returning restores the Sessions or Daily usage view, chooser searches,
filters, page and selected session, including its keyboard focus and scroll
position when its row is outside the currently rendered list.

Model and project analysis displays at most 100 rows and reports **Showing 100
of N** when truncated. Overall totals still include every matching event; equal
values use stable ordering. These limits do not restrict project or model chooser search.

## Coverage and query errors

Coverage explains when history is incomplete because records were retained for
only part of the period, a cumulative counter reset, lineage could not be
verified, or records were unsupported, conflicting, truncated or unreadable.
Unknown future coverage states receive a general incomplete-coverage message
instead of an internal code. Coverage describes imported evidence, not missing
usage inferred from account quotas.

A database query error displays a localized explanation and **Try again**.
A successful query with no matching sessions displays an empty result instead.
Loading finishes on either outcome. Retry requests the current selection;
results from superseded selections cannot overwrite it.

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
database. **Today**, **7 days**, and **30 days** remain available. Choose
**Custom** to enter inclusive start and end dates within the configured
retention window and through today. The first import still covers at most 30
days. Today ends at the current time; earlier selected days end at the next
local midnight. The same UTC interval drives summaries, session rows, daily
totals and exports, including across daylight-saving changes. Select a daily
row by mouse or keyboard to open that day's sessions while keeping the current
tool, project and model filters. Return to the daily overview to restore its
filters, focus and scroll position. Account quotas remain separate and retain
their existing provider-reported meaning.

## Export

The session export action writes JSON or CSV using a dedicated schema version 1.
Opening the export dialog fixes the tool, project, model, period boundaries and
format for that export; background history refreshes do not change that selection.
Only one export can run at a time in a session panel. Writing, success and
localized failure feedback remain separate from history loading and survive
automatic refreshes.

Each export assigns its own anonymous IDs. It excludes aliases, project and
model search text, local paths, credentials, and raw tool or session identities.
Rows stream directly from the database to a private, atomically saved file, so
large exports do not collect all events in memory and a failed write preserves
an existing destination. Unavailable JSON token categories remain null. Session
data is excluded from configuration backups and scheduled provider-history exports.

## Data handling

Only supported session metadata and usage fields are retained. Prompts,
assistant replies, conversation titles, and full project paths are never
stored or written to diagnostics. Imported session records, aliases, and
private identity material stay in local application storage. See [Privacy and
security](privacy-and-security.md) for the wider data boundary.

## Upgrade from 24.2.0

V25 uses the same SQLite schema v9, configuration schema v3 and session-export
schema v1. Existing history, local aliases, independent import opt-ins and
settings are retained; no new database migration or reimport is required.
Older saved view state without a model choice opens with **All models**.
Missing view-mode and model-search fields default to Sessions and an empty search.
