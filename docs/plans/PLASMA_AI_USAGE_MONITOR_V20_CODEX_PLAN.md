# Plasma AI Usage Monitor v20 — Reliable Daily Quotas

| Field | Decision |
| --- | --- |
| Proposed release | 20.1.0 |
| Review date | 2026-09-05 |
| Plan finalized | 2026-09-07 |
| Reviewed main | `9942664` — current release 19.0.1 |
| Baseline work | Reuse the existing fixes in PRs #49 and #50; integrate them before final candidate qualification |
| Status | Final implementation plan; implementation and release qualification remain pending |
| Product objective | A panel that reliably answers: what can I use now, when does it reset, and can I trust this value? |

## Recommendation

Make v20 a focused quota reliability and everyday usability release. The project
already has provider adapters, local tool monitoring, guided setup, history,
analysis, scoped budgets, deterministic forecasts, signed pricing catalogs,
diagnostics, exports, notifications, and native packaging. More providers or
another dashboard would add maintenance before resolving the daily trust gaps.

Build on `DailyStateModel`, `SourceReadinessModel`, `DailyOverviewState`, and the
existing scheduler. Keep the current three primary views. Use current Qt 6,
KDE Frameworks 6, and the repository's CMake presets. Do not introduce a second
state store, a hosted service, or new inference requests.

## Delivery decision — 2026-09-07

The owner deferred the remaining checks to the v20 release stage. Completing
this plan and starting implementation do not depend on finishing the current
media/CI investigation. Resume full qualification against the final v20 candidate
in Phases 4–5; deferred checks must not be reported as passed.

PR #49 remains open at `2b11b84451cb6598df97e1c8ee12e1bfd574040e` and
PR #50 remains open at `0195fe8a8bcc91e874a88a98935ea73398829b5d`.
Their branches already contain the Protobuf discovery fix, subscription evidence
refresh, compact layout and normalized tooltip fixes, regression tests, and
capture-workflow improvements. Reuse this work rather than implementing it again.
No merge or release is recorded. Repository branch protection has not been changed.

Existing evidence: the latest inspected complete native runs passed 71/73 tests,
with both failures in release-media qualification. QML lint passed after typing
the loaded representation; packaging, localization, and accessibility checks also
passed in inspected runs. This is partial evidence, not final candidate approval.
Fresh exact-source screenshots, a physical vertical-panel test, and independent
FreeBSD validation remain outstanding. The capture investigation reached a real
Plasma window, but Spectacle produced no screenshot in the CI container. Verify
the compositor's rendering and screenshot support when resuming that work.

Release scope, in priority order:

1. **Required:** quota freshness, independent reset selection, and a shared clock.
2. **Required:** consistent panel/tooltip/source detail and reliable panel sizing.
3. **Required:** honest subscription evidence, allowances, and price ranges.
4. **Required before launch:** reproducible qualification, installation checks,
   final screenshots, and release documentation.

If scope must shrink, defer CI caching and expanded drift-report automation.
Keep quota correctness, clear unavailable states, panel usability, and final
release qualification in v20.

## Current implementation review

The review covered repository instructions, README and contributor workflow,
architecture contracts, v19 plan and release policy, both PR diffs, CI jobs,
panel/bootstrap QML, readiness and daily state projection, subscription catalog
consumers, scheduler policy, and relevant native/QML tests. This is a targeted
engineering review, not an exhaustive security audit or a physical desktop test.

### What is already strong

- Native Qt/KDE architecture and a clear local-first boundary.
- Typed source readiness, metric availability, provenance, and currency separation.
- A shared daily-state projection already consumed by most important views.
- Meaningful native, mocked HTTP, QML, performance, and packaging tests.
- Version-pinned release evidence and explicit separation between frontend and plugin.

### Findings that determine v20 scope

| ID | Priority | Finding and evidence | User impact | Resolution |
| --- | --- | --- | --- | --- |
| R1 | P1 | `plugin/dailystatemodel.cpp`, `buildToolRow()`, treats a valid last sync or activity timestamp as fresh unless a separate stale error is set. `plugin/sourcereadinessmodel.cpp` similarly accepts a previous sync as reporting actual. | Old quota can remain apparently current; new local activity can make old authenticated quota appear fresh. | Phase 1: separate quota verification time from local activity time and apply a source-specific freshness policy. |
| R2 | P1 | `DailyStateModel::buildSummary()` selects nearest actual reset from each source's `_actualQuota`, which was selected for lowest remaining percentage. | With a low weekly quota resetting later and a higher five-hour quota resetting sooner, the earlier reset can be omitted. | Phase 1: independently select lowest quota and earliest future reset across all eligible windows. |
| R3 | P1 | Original #50 converted raw `percentRemaining` through JavaScript `Number()` and maintained a separate source allowlist. `Number(null)` and `Number("")` become zero. | Missing data could become a false exhausted quota, and panel/tooltip decisions could disagree. | PR #50: consume normalized daily windows, reject unavailable values, retain real zero, add regression tests. Complete provider tooltip consolidation in Phase 2. |
| R4 | P1 | `subscriptions-v1.json` was reviewed 2026-07-18, while `check_subscription_catalog.py` requires review within 30 days. The scheduled sanitizer run on September 1 already failed this check on main. | Unrelated maintenance PRs cannot pass; stale public assumptions also reach the UI. | Maintenance prerequisite: source review and corrections. Phase 3: per-entry freshness, lifecycle, and actionable review results. |
| R5 | P2 | `CompactRepresentation.qml` had no natural text width exposed through `RuntimeRepresentationLoader.qml`; #50 addresses the nested layout boundary. | Panel labels can overlap neighbors. Fixed natural width also needs an explicit vertical-panel policy. | PR #50 plus Phase 2's orientation, scaling, localization, and real-panel matrix. |
| R6 | P2 | `CompactMetricState.relativeReset()` and `DailyOverviewState.relativeReset()` obtain wall time when evaluated but do not themselves subscribe to a presentation clock. | Countdown text can remain unchanged until another dependency changes. | Phase 1: one shared clock and deterministic clock injection in tests. |
| R7 | P2 | Local activity fallback limits remain hardcoded in `SubscriptionToolBackend`; range-priced subscriptions are reduced to a minimum fee by existing code and tests. | Activity counts and price ranges are less precise than the panel's language can suggest. | Phase 3: explicit account entitlement vs local activity target, range-aware fixed-fee presentation. |
| R8 | P2 | The release-media manifest binds screenshots to source hashes, and normal PR CI runs those exact-source release checks. Both original PR trees fail that check after legitimate source changes. | A small maintenance change requires fresh desktop evidence, but the original CI does not produce it. | Phase 4: finish isolated capture and candidate qualification before v20 launch. This does not block plan completion. |

R1, R2, R6, and R7 are code-review findings; their missing regression cases must be
written before the v20 implementation. Do not claim their runtime behavior has
been physically reproduced yet.

## Implementation phases

### Phase 0 — Reuse the prepared baseline

1. Reuse #49: prefer Protobuf's native package configuration so imported
   targets retain transitive dependencies. Carry independent FreeBSD validation
   into the final qualification checklist.
2. Reuse #50: forward layout constraints, expose text width, use normalized
   live quota in the tooltip, preserve real zero, and reject unavailable values.
3. Correct stale subscription evidence with official source references; preserve
   explicit uncertainty where a public page does not prove an account allowance.
4. Record the exact implementation branch and included PR commits. Integrate
   the prepared fixes once, preserving the existing work.
5. Carry unfinished media, CI, and physical desktop checks into Phases 4–5.

Exit: the implementation baseline and deferred checks are documented. Green
checks and completed PR merges are not prerequisites for finalizing this plan.
No release tag or publication is part of this phase.

### Phase 1 — One freshness and reset contract

Likely files: `plugin/dailystatemodel.{h,cpp}`,
`plugin/sourcereadinessmodel.{h,cpp}`, `plugin/subscriptiontoolbackend.{h,cpp}`,
`plugin/refreshschedulermodel.{h,cpp}`, and their tests.

- Represent local activity time and authenticated quota observation time
  separately. File activity must not refresh quota evidence.
- Expose stable freshness and availability reasons through the existing models.
- Select live quota only from valid normalized, sufficiently fresh windows.
  Retain older observations for history and explicit last-known detail.
- Calculate earliest future reset independently of lowest remaining quota.
- A passed reset means awaiting fresh data, not an invented full allowance.
- Refresh once after resume/network recovery within single-flight and retry
  policy. Respect Retry-After; do not retry authentication failures continuously.
- Use a single presentation clock for reset countdowns. It must not trigger
  HTTP requests every minute or cause per-row timers.

Acceptance cases:

- Weekly 10% remaining/reset in five days plus five-hour 80%/reset in one hour:
  lowest quota is 10%; next reset is one hour.
- Advancing an injected clock without backend signals ages the quota correctly.
- New local activity does not refresh an old authenticated quota timestamp.
- Missing, null, malformed, and expired values never become zero; actual zero
  remains visible as exhausted.
- Suspend/resume, timeout, 401, 429, and network recovery cannot duplicate a
  refresh or emit repeated quota notifications.

### Phase 2 — A consistent panel, tooltip, and source detail

Likely files: `CompactRepresentation.qml`, `RuntimeRepresentationLoader.qml`,
`NativeMonitor.qml`, `components/CompactMetricState.qml`,
`components/DailyOverviewState.qml`, and QML tests.

- Finish moving tooltip content to normalized daily-state rows. Remove raw
  provider cost/limit loops from tooltip presentation.
- Show the same source, window, remaining quota, reset, and freshness in the
  panel, tooltip, Overview, and Source Detail.
- Label actual quota, local activity, and unknown data explicitly.
- Horizontal panels request natural text width. Vertical panels remain within
  the allocated width with a readable abbreviated/elided value; the full text
  remains accessible in the tooltip and screen-reader label.
- Preserve an icon that follows panel thickness. Verify recovery-to-runtime and
  runtime-to-recovery representation swaps.
- Offer one clear recovery action for stale or unauthenticated sources through
  existing Source Detail/Settings navigation.

Acceptance matrix: horizontal and vertical panels; 24/32/48/64 px thickness;
100/125/150/200% scaling; light/dark themes; long translated labels; unavailable
and 0/100% quota; loading; disconnected source; plugin unavailable; mode changes;
keyboard activation and screen-reader labels. No painted overlap, binding loops,
or tooltip disagreement.

### Phase 3 — Maintainable subscription truth

Likely files: `subscriptions-v1.json`, `subscriptionplancatalog.*`,
`subscriptiontoolbackend.*`, catalog validation scripts, existing Diagnostics,
and `docs/user-guide/subscriptions.md`.

- Give evidence entries explicit reviewed/effective/expiry policy. A recent
  top-level catalog date must not certify an older unreviewed row.
- Distinguish price, qualitative allowance, account entitlement, and locally
  configured activity target. Never divide credit usage by a request cap.
- Keep Copilot AI credits separate from historical premium-request semantics.
  Scope current work to verified active billing contracts.
- Present subscription price ranges as ranges; unknown organization/team totals
  stay unavailable rather than becoming a guessed monthly fee.
- Keep brand/account transitions such as Windsurf/Devin in review until their
  supported read-only contract is tested. Do not add a new provider just for a
  renamed marketing page.
- Extend scheduled drift reporting to subscriptions with a clear reason and
  official reference per actionable item. No runtime pricing-page scraping.
- Add date-boundary and malformed-evidence tests. Keep release freshness gates
  strict; design reproducible structural tests separately from dated evidence.

Exit: outdated public claims are unavailable or visibly under review, and valid
account telemetry continues to work when static catalog evidence expires.

### Phase 4 — Reproducible desktop qualification

Likely files: `.github/workflows/{build-test,qml,static,sanitizers,package}.yml`,
`scripts/demo/capture_v18_media.sh`, `scripts/check_release_media.py`,
`scripts/test_release_media_evidence.py`, and release documentation.

- Produce isolated Plasma screenshots with process/window identity and source
  hashes from the candidate source tree. Upload diagnostic logs when capture
  fails; publish qualified image artifacts only after successful capture.
- Make capture results reusable by the other qualification jobs for the same
  source tree. Do not substitute old screenshots or rewrite capture provenance.
- Define the distinction between ordinary development checks, candidate
  qualification, and publication. Keep all required protected checks meaningful.
- Cover Protobuf config discovery and its module path with a configure/generate/
  link smoke fixture. Obtain real FreeBSD evidence before expanding support claims.
- Cancel superseded CI runs and use appropriately keyed build caches after
  measuring where time is spent. Never cache credentials or user databases.

Exit: the exact candidate passes native tests, QML lint/import/smoke, sanitizers,
static policy, media checks, and package payload checks. Artifacts identify the
candidate commit and contain both required native/frontend installation evidence.

### Phase 5 — Documentation and release

- Update the canonical user guide and regenerate `docs/wiki/`.
- Add the v20 changelog, versioned checklist, and known limitations.
- Capture the final UI matrix, including real horizontal and vertical panels.
- Validate clean Fedora installation, upgrade from the current supported
  package, uninstall, and user-data preservation. This is data safety, not a
  requirement to preserve obsolete APIs or libraries.
- Run the repository's exact-version and exact-tag checks before a separately
  authorized publication. Independently read back GitHub, COPR, and KDE Store.

## Validation commands

Run in a Fedora KDE environment with the documented development dependencies:

```bash
just doctor
just build-debug
ctest --test-dir build/debug --output-on-failure \
  -R 'dailystatemodel|sourcereadinessmodel|subscription|dailycontrol_qml|dependencybootstrap_qml|refreshscheduler|notifications_qml'
just test
just check
just qml-lint
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
just phase7-check
just release-check
just fedora44-check
```

Use fake clocks and deterministic mocked quota fixtures for regression tests.
Do not use personal accounts, billable inference, or a user's active panel as
an automated test fixture. Record manual checks as unverified until performed.

## Delivery order and boundaries

Suggested implementation slices: freshness and reset selection; panel and tooltip;
subscription evidence lifecycle; desktop qualification; release documentation.
Each slice should have focused tests and a reviewable diff.

A rough planning allowance is 12–20 engineering days plus access to a real
Fedora/Plasma test session. Re-estimate after Phase 0. The estimate is not a
delivery promise; capture infrastructure and vendor contract changes are the
largest uncertainties.

Defer new providers, cloud synchronization, team dashboards, a fourth primary
tab, automatic currency conversion, additional forecasting features, and broad
refactoring. Do not rewrite already-working budget/history/cost-engine modules.
Existing user history and secrets must remain intact.

Success means a user's panel and tooltip agree, stale values stop looking live,
the next reset is actually the next eligible reset, and maintainers can prove
the shipped behavior with repeatable evidence.
