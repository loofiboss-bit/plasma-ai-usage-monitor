# Plasma AI Usage Monitor v18.0.0 — Budget Control execution plan

## Release identity

| Field | Value |
| --- | --- |
| Target | `v18.0.0` |
| Release name | Budget Control |
| Baseline | `f206ff2c43cc957b9670916a39c5e05a3faec15d` |
| Baseline version | `17.0.0` |
| Development branch | `v18/budget-control` |
| Publication rule | Exact verified `main` commit and annotated tag only |

v18 replaces fixed provider daily/monthly budget settings with local, typed,
instance-owned policies. It preserves v17 quota runway, the read-only provider
boundary, KWallet secrets, local history, exact missing-value semantics,
currency separation, three popup tabs, and supported Fedora packaging.

`VERSION` remains `17.0.0` until the final release candidate passes every
required gate. No test installs into, restarts, or changes the user's active
desktop session.

## Requirements and blocking evidence

| ID | Contract | Primary phase | Blocking evidence |
| --- | --- | ---: | --- |
| REQ-001 | Reproducible popup performance | 0 | Isolated v17/v18 ABBA, valid PID/window/AT-SPI binding, median/p95, trace, zero hidden work |
| REQ-002 | Dynamic budget policies | 1 | Repository CRUD, validation, duplicate, isolation, atomic replacement |
| REQ-003 | Deterministic billing cycles | 1–2 | UTC half-open day/week/month/anchor/reset, time zone and DST matrix |
| REQ-004 | Safe-to-spend and pacing v2 | 2 | Pacing fields, prior period, unavailable matrix, 100k performance |
| REQ-005 | Actual/estimated/currency separation | 2–3 | Mixed-class/currency rejection and checked minor-unit table |
| REQ-006 | Provider-supported scope budgets | 3 | Provider fixtures, reconciliation, `Unattributed`, no inferred cost scope |
| REQ-007 | Schema-v6 migration and rollback | 1 | Backup, data preservation, idempotence, injected rollback, v17 compatibility |
| REQ-008 | Policy UI and settings | 4 | Three lazy tabs, staged Apply/Cancel, responsive and accessible editor |
| REQ-009 | Transition-only notifications and snooze | 5 | Edge matrix, restart, DND, cooldown, reset, failed delivery |
| REQ-010 | Private low-cardinality integrations | 3, 5 | Payload snapshots, label schema, raw-ID exclusions, zero provider writes |
| REQ-011 | Configuration/query maintainability | 1, 6 | Config v2/v3, focused queries, catalog v6, no duplicate inventories |
| REQ-012 | Accessibility, packaging and release | 4, 7 | QML/accessibility matrix, Fedora lifecycle, exact artifacts and public readback |

## Public contracts

- SQLite schema v6 owns `budget_policies`, migration markers, policy state, and
  persist-before-delivery events. A `.v18-backup` precedes v5-to-v6 migration.
- Limits and query results use integer ISO-currency minor units. Unknown
  currencies are unavailable; there is no guessed precision or FX conversion.
- Calendar policies persist their IANA time zone and resolve exact UTC
  half-open intervals. Migrated v17 policies retain UTC semantics.
- `budget-pacing-v2` reports policy identity, period, spent, remaining,
  consumption, projection, predicted overrun, safe today, remaining daily
  allowance, evidence, coverage, samples, previous-period comparison, and one
  typed unavailable reason.
- Risk precedence is unavailable, exceeded, critical, warning, then safe.
- Config export schema v3 includes policies. Schema v2 remains settings-only.
  Explicit local backups may contain scope identities; diagnostics and external
  integrations may not.
- Legacy KConfig budget keys remain hidden and unchanged for v17 rollback. They
  are read only by the one-shot migration and never drive v18 runtime alerts.

## Sequential execution

### Phase 0 — deterministic popup performance (`REQ-001`)

- Build exact v17 and candidate trees into separate temporary prefixes.
- Run at least three warmups and 20 valid first/warm samples per version in one
  boot and nested Plasma session, interleaved ABBA.
- Bind timing to the exact plasmashell PID, applet instance and window. Reject
  stale, focus, virtual-keyboard or unexplained-network runs.
- Record monotonic component, SQLite, guardrail, chart and first-frame marks.
- Load Overview only; History, Source Detail scopes and Insights stay lazy.
- Cache complete canonical guardrail queries by query, policy and observation
  revision, with freshness/period invalidation.

Exit: candidate median does not exceed v17; first/warm median at most 125 ms,
p95 at most 180 ms, startup budget passes, zero hidden work/network, cleanup,
invalidation and supersession tests pass. The 100 ms value is a target, not an
exception mechanism.

### Phase 1 — policy repository and schema v6 (`REQ-002`, `REQ-007`, `REQ-011`)

- Add the repository as the only write boundary and a QML-facing model.
- Validate create, update, duplicate, delete, enable and snooze.
- Isolate every policy and migration marker by stable applet owner.
- Migrate v5 transactionally after a separate backup. Create deterministic
  policies for non-zero legacy daily/monthly values without deleting the keys.
- Validate every config v3 setting and policy before atomic Apply.

Exit: CRUD/roundtrip/isolation, migration preservation/idempotence/failure
rollback, config v2/v3 and v18-to-v17 rollback tests pass.

### Phase 2 — billing cycles and `budget-pacing-v2` (`REQ-003`–`REQ-005`)

- Split the facade into quota, observation, cycle and pacing units.
- Resolve day, Monday ISO week, calendar month, anchor 1–28 and authenticated
  stable provider reset.
- Use compatible complete UTC days only; keep missing distinct from zero and
  actual, estimated, currency and scope classes separate.
- Compare only the same policy/scope/currency/value class in the previous
  period. Keep forecasts ephemeral.

Exit: month/leap-year/anchor/time-zone/DST matrix, incompatibility matrix,
cancellation/supersession, 100k performance, worker/connection teardown and
v17 quota regressions pass.

### Phase 3 — scoped budgets (`REQ-005`, `REQ-006`, `REQ-010`)

- Accept only catalog-declared dimensions in validated provider responses.
- OpenAI cost supports project and line item, never model. Anthropic supports
  reported workspace/model/service-tier usage and reported cost dimensions.
  LiteLLM exposes only validated gateway scopes and otherwise aggregate.
- Keep raw scope identities local and render safe label snapshots.
- Reconcile aggregate to compatible scoped values with explicit
  `Unattributed`; scoped-over-aggregate is unavailable.

Exit: provider pagination/rename/delete fixtures, rounding reconciliation,
actual/estimated boundaries, privacy scans and zero inferred scope pass.

### Phase 4 — Budget Control UI (`REQ-008`, `REQ-012`)

- Keep exactly Overview, History and Insights, with lazy secondary content.
- Prioritize current incidents over future risk and show risk, safe today,
  spent/remaining, reset and one direct action.
- Add Source Detail policy/scope/prior-period rows, at most eight initially.
- Replace the fixed budget settings with a searchable staged master/detail
  policy editor, live preview, capabilities, validation and Apply/Cancel.

Exit: QML/screenshots at 100/125/140/150/200%, narrow/wide, themes, long text,
keyboard, focus, labels/roles/screen reader and hidden-loader tests pass.

### Phase 5 — transitions, snooze and integrations (`REQ-009`, `REQ-010`)

- Persist new state and event atomically before delivery for warning, critical,
  exceeded, recovered and reset transitions.
- Never call risky-to-unavailable recovery; never call unavailable-to-safe a
  recovery. Deduplicate by policy/period/transition across restart.
- Preserve DND/cooldown events as pending/suppressed and expire snooze at reset.
- Keep policy IDs internal to the KDE action. Allow-list webhook fields and use
  fixed low-cardinality metrics without policy/scope labels.

Exit: full edge/restart/DND/cooldown/snooze/reset/failure matrix, payload and
metric snapshots, persist-before-delivery, zero provider writes/billable calls.

### Phase 6 — maintainability and catalog v6 (`REQ-011`)

- Complete the focused runway split without broad provider refactoring.
- Drive budget capability and notification keys from catalog v6.
- Declare policy contract, supported scopes/cycles and current review dates for
  every budget-capable source. Remove duplicate runtime budget inventories.

Exit: static inventory/config/scope contracts, review coverage and all existing
provider/quota notification regressions pass.

### Phase 7 — documentation, qualification and publication (`REQ-001`, `REQ-012`)

- Update canonical docs, generated wiki, release notes, package metadata,
  screenshots and evidence. Explain cycles, time zones, missing data,
  actual/estimated, `Unattributed`, limits, migration/rollback, backup
  sensitivity and the read-only boundary.
- Run `doctor`, debug build/tests, checks, QML lint, phase7/release gates,
  strict import, offscreen smoke, Fedora 44 and diff checks in isolated
  environments.
- Verify v17 upgrade/rollback/re-upgrade, clean/reinstall/remove, preserved user
  data/KWallet/schema backup, config v2/v3, reproducible archives/SBOM/checksums,
  100k teardown and the final Phase 0 ABBA gate.
- Only after every candidate gate is green: bump all surfaces to 18.0.0, commit,
  land verified `main`, create annotated `v18.0.0`, let the tag workflow produce
  a draft, and verify artifact lineage before publication.
- Publish the same tag to GitHub Release, supported COPR targets, KDE Store and
  the wiki. Perform unauthenticated readback and record it in a separate
  post-release evidence commit. Never move the tag.

Exit: every mandatory local/CI/lifecycle/performance gate and every named public
surface has exact lineage and readback evidence. A missing credential or
surface is `blocked`, never silently skipped.

## Phase status

| Phase | Commit | Status |
| ---: | --- | --- |
| 0 | `481a80f` | Complete; final isolated ABBA rerun required in Phase 7 |
| 1 | `dd0061d` | Complete |
| 2 | `6aac5f7` | Complete |
| 3 | `1697a42` | Complete |
| 4 | `42d978c` | Complete |
| 5 | `f774683` | Complete |
| 6 | `fbed8ce` | Complete |
| 7 | — | In progress |

## Non-goals and stop conditions

No new provider, cloud sync, FX, invoice import, prompt content, automatic
model/provider switching, provider write, billable inference or fourth popup
tab is in scope. No release action may install or change the user's live
desktop. Any failed migration, privacy, performance or public-readback gate
blocks the release; it cannot be converted into an exception.
