# V25 — Searchable and reliable session history

- **Target release:** `25.0.0`
- **Status:** implemented and locally qualified; see the V25 release checklist for evidence and manual limits.

## Goal and boundaries

Prepare a local 25.0.0 candidate from current origin/main, including the catalog
fix following 24.2.0, without changing older checkouts. Answer which sessions and
reported tokens belong to a selected project and model. Preserve independent
Codex/Claude Code consent, retention, privacy and nullable token semantics.
No commit, push, publication or host installation is part of this task.

## Query and interface changes

- Add optional model filters to LocalSessionModel query/export calls; old calls
  retain all-model behavior. Apply exact IDs or unknown-model selection to
  usage events before aggregation. Count unique matching sessions.
- Share tool/project/period/model filters across summaries, daily series,
  sessions, details, breakdowns and schema-v1 manual JSON/CSV export.
- Report queryFailed(requestId, errorKey) separately from valid empty results.
  End loading on failure, allow retry and reject superseded asynchronous replies.
- Keep project/model breakdowns bounded at 100 rows with complete total counts,
  stable ties and untruncated totals. Query complete project choices separately.
- Enable Next only if the same query generation's summary reports more sessions.
  Reset page and session selection on filter changes.

## Interaction

Search all project choices by local alias or displayed anonymous label without
case sensitivity. Offer All models, exact reported IDs and Unknown model from
the selected tool/project/period before applying the model filter. Show the
active selection and reset action. Open filtered History sessions from analysis
rows while retaining other filters, and restore navigation context/focus.
Translate every importer coverage state and unknown-state fallback into useful
localized explanations; never expose raw codes.

## Compatibility and delivery

Preserve SQLite schema v9, configuration schema v3, session-export schema v1,
existing provider exports, settings, aliases and imported history. Old saved
views without a model filter use All models. Exclude aliases, search text and
paths from exports. Do not infer price, subscription use or productivity from
tokens. No new providers, period comparisons or broad UI redesign.
Update metadata, user guide, generated wiki and deterministic synthetic media.

## Acceptance

Cover open/SQL failures, empty results, retry, late replies, all coverage states,
unknown coverage, 0/99/100/101/200-session paging, more than 100 projects/models,
multi-model sessions, unknown model, unavailable token categories, combined
filters, export parity, alias updates, no search matches and old view state.
Run Debug CTest, just check, just qml-lint and just release-check. Extend the
100,000-event performance fixture while retaining its ten-second limit.
Verify an isolated 24.2.0 upgrade preserving state. Record narrow-popup, scaling
and keyboard evidence separately from physical desktop qualification.
