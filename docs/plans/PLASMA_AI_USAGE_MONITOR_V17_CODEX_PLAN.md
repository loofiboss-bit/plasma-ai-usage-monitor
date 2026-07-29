# Plasma AI Usage Monitor v17.0.0 — Codex Execution Plan

| Field | Value |
|---|---|
| Document type | Codex implementation plan and execution prompt |
| Status | Ready for execution |
| Repository | `loofiboss-bit/plasma-ai-usage-monitor` |
| Baseline | `v16.0.1` / `7e0e1c38a87b762989ddd80014b774e5142ec657` |
| Target | `v17.0.0` |
| Release name | Runway Guardrails |
| Platform | KDE Plasma 6, C++20, Qt 6, KF6, QML, CMake |

## 1. Mission

Build the complete v17 release as one coherent update. v17 must turn existing usage, quota, cost, model, project, and workspace data into deterministic runway guardrails that answer:

1. Will synchronized quota last until its reset?
2. Will the configured monthly budget be exceeded?
3. Which provider, model, project, or workspace is driving the risk?
4. How strong is the available evidence?

The implementation must remain local-first, read-only toward providers, deterministic, privacy-preserving, and useful without an LLM or cloud backend.

The OpenAI pagination correctness work described in Phase 0 is part of v17. Do not create or release v16.0.2.

## 2. Repository Ground Truth

Before editing, Codex must inspect the current repository and verify every assumption in this document against the checked-out code.

Known baseline:

- Current stable version: `16.0.1`.
- Current main baseline commit: `7e0e1c38a87b762989ddd80014b774e5142ec657`.
- The product currently supports 18 providers and 7 subscription tools.
- The configuration surface contains 213 active non-secret keys.
- Existing database schema v4 supports `model_scope` and `project_scope`.
- `SourceDetailModel` currently exposes `kind`, `available`, `value`, `unit`, `currency`, `source`, `quality`, `semantic`, `scope`, `window`, and `resetAt`, but not `modelScope` or `projectScope`.
- Anthropic already publishes daily metrics with model, workspace, and service-tier dimensions.
- OpenAI usage and cost handling currently produces aggregate results and does not expose the required grouping.
- `IntelligenceEngine.qml` is instantiated or aliased but is not used by the current Analyst experience and includes obsolete subjective “prompt efficiency” terminology.
- `TrendSummary.qml` appears unused.
- `EfficiencyMetricCard.qml` is used and should be renamed to the neutral `OutputInputRatioCard.qml`.
- Large existing components include `UsageDatabase`, `DailyStateModel`, `ProviderBackend`, and `NativeMonitor.qml`. Avoid making them broader catch-all components.

Establish a clean baseline before implementation:

```bash
git status --short
git rev-parse HEAD
cat VERSION
just doctor
just build-debug
just test
just check
just qml-lint
```

If a baseline command is unavailable or already failing, record the exact command and output. Do not conceal baseline failures and do not attribute them to v17 changes.

## 3. Non-Negotiable Product Invariants

The release must preserve:

- Package ID, QML URI, notification identity, AppStream identity, and distribution identity.
- Existing KWallet folder names and secret identifiers.
- Existing user configuration, history, provider catalogs, and subscription catalogs.
- Direct stable release policy.
- Read-only provider behavior. No automated provider mutation.
- Local-first operation and optional integrations.
- Strict distinction between missing data and numeric zero.
- Strict distinction between actual and estimated values.
- Strict currency compatibility. Never sum mixed currencies.
- Exact UTC half-open time intervals internally.
- Accessibility, keyboard navigation, narrow and wide layouts, and non-color-only state communication.
- Existing user changes in the worktree.

Forecasts are derived interpretations. They must never be written into raw observation history as if they were provider-reported facts.

## 4. Explicitly Out of Scope

Do not add:

- New providers or subscription tools.
- A fourth primary tab.
- A hosted backend, account system, team workspace, or cloud synchronization.
- LLM inference, prompt analysis, semantic analysis, or “AI confidence”.
- Currency conversion.
- Automatic model switching.
- Automatic provider writes or quota-management actions.
- A broad Browser Sync expansion.
- A broad rewrite of `ProviderManager` or the provider architecture.
- Model-level OpenAI cost attribution when the API does not report it.
- High-cardinality project or model labels in default external metrics.

If an out-of-scope change appears necessary, stop and report the dependency instead of silently expanding the release.

## 5. Delivery Strategy

Recommended work branch:

```text
v17/runway-guardrails
```

Implement in the phases below. Each phase has an exit gate. Do not move forward while its correctness gate is failing.

Keep `VERSION` at `16.0.1` throughout implementation. Change all release-facing version surfaces to `17.0.0` only in the final release phase.

Do not commit, push, tag, publish, install on the host, or create a GitHub release unless the user explicitly authorizes that action.

## 6. Phase 0 — Baseline and OpenAI Pagination Correctness

### Objective

Remove a release-blocking correctness risk before building forecasts.

`OpenAIProvider::fetchMonthlyCosts()` currently queries from month start to now without a sufficiently explicit bucket limit and without following `next_page`. The OpenAI Costs API can return only the first page of buckets, so a monthly total can become incomplete after the first week while appearing valid.

### Required implementation

Create bounded pagination for:

- OpenAI usage.
- OpenAI daily costs.
- OpenAI monthly costs.

The pagination implementation must provide:

- Explicit page limits supported by the endpoint.
- Cursor-based traversal through every page.
- Cursor-loop detection.
- Duplicate-page or duplicate-bucket handling.
- A documented maximum request budget.
- Existing request cancellation and generation safety.
- Correct retry behavior for rate limiting.
- Exact UTC boundaries.
- No silent conversion of partial results into complete totals.

If pagination ends partially because of cancellation, a malformed response, a cursor loop, a request-budget limit, or a terminal network failure:

- Do not publish a new “complete” total.
- Retain the prior successful value as stale when the current contract supports it, or publish unavailable with a typed reason.
- Preserve diagnostic context without leaking secrets.

### Required tests

Use mock HTTP responses and fixtures. No live API key is required.

Cover:

- More than seven returned buckets.
- Multiple valid pages.
- Missing `next_page`.
- Repeated cursor.
- Duplicate page.
- Duplicate bucket at a page boundary.
- HTTP 429 and retry.
- Cancellation between pages.
- Superseded request generation.
- Malformed intermediate page.
- Terminal failure after one or more valid pages.
- Months with 28, 29, 30, and 31 days.
- UTC start and end boundaries.

### Exit gate

- All aggregate totals are complete or explicitly stale/unavailable.
- No pagination failure can masquerade as a complete monthly value.
- Focused provider tests pass.
- Existing provider behavior remains compatible.

## 7. Phase 1 — Forecast Contract and Schema v5

### 7.1 Forecast Contract v1

Define one typed forecast contract used by the calculation layer, QML-facing model, UI, notifications, and integrations.

Required fields:

| Field | Required meaning |
|---|---|
| `kind` | `quota_exhaustion` or `budget_overrun` |
| `state` | `unavailable`, `safe`, `warning`, or `critical` |
| `sourceId` | Stable local source identifier |
| `sourceKind` | Provider/source category |
| `window` | Stable quota or budget window |
| `scope` | Aggregate scope represented by the result |
| `currentValue` | Latest compatible observed value |
| `projectedValue` | Derived end-of-window value |
| `limitValue` | Quota or configured budget limit |
| `unit` | Compatible unit |
| `currency` | Currency or null for non-currency units |
| `predictedAt` | Predicted exhaustion/overrun time or null |
| `periodEnd` | Reset or budget-period end |
| `sampleCount` | Count of compatible observations |
| `coveragePercent` | Coverage computed by the method |
| `evidenceGrade` | Mechanical evidence classification |
| `methodId` | Versioned deterministic calculation method |
| `reasonKey` | Typed localized reason |
| `generatedAt` | Forecast calculation time |
| `valueClass` | `actual` or `estimated` |

Contract rules:

- Missing or unavailable values are never coerced to zero.
- Actual and estimated values never share one total or forecast.
- Mixed currencies are never summed.
- Unsupported combinations return `unavailable` with a typed `reasonKey`.
- The contract contains no subjective “AI confidence” score.
- Evidence grades are mechanically derived from sample count, coverage, freshness, and volatility.

Use a versioned method identifier such as:

```text
quota-runway-v1
budget-pacing-v1
```

### 7.2 Schema v5

Add a separate `guardrail_events` table for persisted state transitions. Do not store a row on every refresh.

Suggested columns:

| Column | Notes |
|---|---|
| `id` | Database primary key |
| `stable_id` | Stable source/risk/window identity |
| `risk_kind` | Quota exhaustion or budget overrun |
| `window_key` | Stable window identity |
| `transition` | `warning`, `critical`, or `recovered` |
| `observed_at_utc` | Transition observation time |
| `predicted_at_utc` | Nullable predicted event time |
| `period_end_utc` | Reset or budget-period end |
| `method_id` | Versioned calculation method |
| `evidence_grade` | Mechanical grade |
| `current_value` | Value at transition |
| `projected_value` | Projected value at transition |
| `limit_value` | Applicable limit |
| `unit` | Unit |
| `currency` | Nullable currency |

Requirements:

- Deduplicate by source, risk, window, and transition identity.
- Persist enough state to suppress repeated notifications across restarts.
- Provide transactional v4-to-v5 migration.
- Back up before migration using the project’s existing safe migration pattern.
- Roll back cleanly after migration failure.
- Leave existing observations and history unchanged.
- Document retention and export behavior.
- Keep forecast results out of the observations table.

### Exit gate

- Contract tests cover every state and unavailable reason.
- Migration succeeds from representative v4 fixtures.
- Injected migration failure rolls back without history loss.
- Reopening the database preserves transition-deduplication state.

## 8. Phase 2 — Scope and Attribution

### Objective

Expose compatible provider dimensions without inventing attribution or double-counting.

### Required implementation

- For OpenAI Usage, use documented grouping for model and `project_id`.
- For OpenAI Costs, use only dimensions reported by the endpoint, such as `project_id`, line item, or API key when supported and appropriate.
- Never infer model-level OpenAI cost if the endpoint does not report it.
- Reuse Anthropic’s existing model, workspace, and service-tier dimensions.
- Add a dedicated `ScopeBreakdownQuery` rather than placing more SQL into `UsageDatabase`.
- Expose `modelScope` and `projectScope` through `SourceDetailModel`.
- Surface model, project/workspace, service-tier, and actual-versus-estimated breakdowns where data supports them.
- Keep current provider aggregate metrics for Daily State.
- Prevent aggregate rows and scoped rows from being summed together.
- Keep raw scope identifiers local. Do not include them in webhook or Prometheus payloads by default.

Use stable display rules for missing/deleted project names and opaque IDs. Do not leak API keys or secret fragments into labels, logs, exports, or UI.

### Exit gate

- Grouped fixtures reconcile to the provider aggregate exactly once.
- Unsupported dimensions remain absent rather than inferred.
- QML roles expose scope values with explicit availability.
- No scope identifier enters external payloads by default.

## 9. Phase 3 — Runway Engine

### Architecture

Prefer these focused components:

```text
plugin/runwayquery.{h,cpp}
plugin/guardrailmodel.{h,cpp}
plugin/scopebreakdownquery.{h,cpp}
```

Responsibilities:

- `RunwayQuery`: pure data access and deterministic calculations.
- `GuardrailModel`: asynchronous, superseding, QML-facing results and state.
- `ScopeBreakdownQuery`: compatible scoped aggregation.

Do not execute SQL in QML. Do not enlarge `UsageDatabase` with all new queries; add only minimal orchestration there if an existing public contract makes it unavoidable.

### 9.1 Quota runway method

Calculate quota exhaustion only when:

- Quota is provider-reported or derived from authenticated synchronized provider data.
- Samples refer to the same stable quota window and reset.
- At least four compatible samples exist.
- The observation span meets a fixed and documented minimum.
- The latest sample is fresh under a fixed and documented threshold.

Use a robust deterministic slope estimator, such as median pairwise slope or Theil–Sen, for consumption over time.

Handle explicitly:

- Stable decline.
- No consumption.
- A provider reset.
- Non-monotonic remaining quota.
- Sparse gaps.
- Stale latest data.
- Missing values.
- A real numeric zero.
- Changed window or reset time.

Return safe or unavailable with a typed reason when the data cannot support a forecast. Report risk only when predicted exhaustion occurs before the reset.

### 9.2 Budget runway method

Calculate monthly budget pacing only when:

- Spend class is consistently actual or consistently estimated.
- All values use one currency.
- The configured budget uses the same currency.
- At least five distinct compatible days exist.
- Coverage is at least 70 percent under the documented method.

Method:

1. Use exact UTC half-open periods.
2. Exclude the incomplete current day from the daily baseline.
3. Compute compatible month-to-date spend.
4. Compute median compatible daily spend.
5. Multiply the daily baseline by remaining calendar days.
6. Add that projection to month-to-date spend.

Do not treat a missing day as zero.

### 9.3 Evidence grade

Define grades mechanically. A suitable minimum contract is:

| Grade | Meaning |
|---|---|
| `unavailable` | Preconditions fail |
| `usable` | Minimum sample, coverage, and freshness thresholds pass |
| `strong` | Higher fixed sample and coverage thresholds pass and volatility is within a documented bound |

Do not use probabilistic or anthropomorphic language. Every grade must be reproducible from the same input.

### 9.4 Concurrency and performance

- Queries run away from the UI thread.
- A new request supersedes obsolete work.
- Completion from an obsolete generation cannot update the model.
- Workers and database connections are released after completion.
- Add a 100,000-observation performance case.
- Target runway query completion at or below 750 ms in the project’s representative test environment.

### Exit gate

- Golden fixtures produce deterministic results.
- All unavailable reasons are typed and localized.
- No forecast result is stored as raw history.
- Superseded work cannot update UI state.
- Worker and connection cleanup tests pass.

## 10. Phase 4 — UI and UX

### Product placement

Do not add a new primary tab.

Daily Focus may show the most important proactive risk, but current failures, exhausted quota, authentication errors, and unavailable synchronized state must outrank forecasts.

### Source Detail

Add a Runway card that can show:

- Safe, warning, critical, or unavailable.
- Predicted exhaustion or overrun time.
- Reset or period-end comparison.
- Current, projected, and limit values.
- Evidence grade.
- Sample count and coverage.
- Calculation method in accessible detail.
- A clear unavailable reason.
- Primary scope drivers.
- Links or actions into relevant History views.

### Analyst

Add a “Pacing and runway” section using the same typed forecast contract.

### Settings

- Rename the visible “Budget” settings category to “Guardrails”.
- Preserve existing configuration keys and migration compatibility.
- Add only a small configuration surface:
  - Forecast UI enabled, default `true`.
  - Forecast notifications enabled, default `false`.
  - Forecast lead time: `1`, `6`, `24`, or `48` hours.
- Reuse existing per-provider notification and budget-threshold controls.

### Panel

An optional compact panel mode named `runway` may be added. It must not replace the current default attention mode.

### Neutral terminology and cleanup

- Rename `EfficiencyMetricCard.qml` to `OutputInputRatioCard.qml`.
- Update every import, reference, registration, test, screenshot contract, and translation source.
- Remove `IntelligenceEngine.qml` and `TrendSummary.qml` only after repository-wide search and tests prove they are not public or runtime contracts.
- Remove subjective “prompt efficiency” wording.

### Accessibility

Verify:

- Keyboard traversal.
- Screen-reader labels and state descriptions.
- Narrow and wide popup layouts.
- High-contrast and light/dark themes.
- States distinguishable without color.
- Long localized labels and timestamps.

### Exit gate

- No fourth tab exists.
- Current incidents outrank forecasts.
- Missing data is visibly distinct from zero.
- UI uses one shared forecast contract.
- QML lint, accessibility checks, and localization checks pass.

## 11. Phase 5 — Notifications and Integrations

### Transition semantics

Notify only on meaningful transitions:

- Enter warning.
- Enter critical.
- Recover to safe or unavailable under the defined recovery policy.

Persist deduplication across restarts. Refreshing the same state must not notify again.

### Channels

Use the same typed event payload for:

- KDE notifications.
- Slack webhook integration.
- Discord webhook integration.

External payloads must not contain project IDs, workspace IDs, model-level scope IDs, API-key identifiers, or other high-cardinality local identifiers by default.

### Prometheus

Expose low-cardinality metrics such as:

- Current guardrail risk state.
- Seconds until predicted exhaustion when available.

Do not add project or model labels by default.

### Safety

Notifications and integrations are informational. They must never switch models, alter provider budgets, revoke keys, or perform provider-side writes.

### Exit gate

- One transition produces at most one event per configured channel.
- The same state after restart remains deduplicated.
- Recovery is emitted once.
- Payload privacy and Prometheus cardinality tests pass.

## 12. Phase 6 — Documentation, Release Evidence, and v17 Cut

### Documentation

Update:

- README.
- Roadmap.
- Provider capability documentation.
- Configuration reference.
- Privacy and data-flow documentation.
- Database schema and migration notes.
- Notification and integration documentation.
- Troubleshooting.
- Wiki source files maintained by the repository.
- Screenshots or release evidence required by project policy.

Document clearly:

- What quota runway means.
- What budget pacing means.
- Actual versus estimated behavior.
- Coverage and evidence grades.
- Why a forecast may be unavailable.
- Time-zone behavior.
- Currency restrictions.
- Scope privacy.
- Notification defaults.

### Version cut

Only after implementation and broad verification:

- Change `VERSION` and every required version surface to `17.0.0`.
- Use release name `Runway Guardrails`.
- Update changelog and release notes.
- Run exact-tag and version-consistency gates using the repository’s policy.
- Preserve direct stable release policy.

Do not create the tag or publish the release without explicit user authorization.

### Distribution verification

Verify install, update, rollback, removal, and user-data preservation for the supported Fedora path, including an upgrade from v16.0.1 to v17.0.0.

### Exit gate

- Documentation matches shipped behavior.
- Version and release surfaces are consistent.
- Required release evidence is present.
- All final verification commands pass.

## 13. Required Test Matrix

At minimum, add or extend tests for:

| Area | Required scenarios |
|---|---|
| OpenAI pagination | More than seven buckets, cursor loop, missing cursor, duplicate page/bucket, 429, cancellation, malformed page, partial terminal failure |
| Calendar periods | 28-, 29-, 30-, and 31-day months |
| Time zones | UTC, Europe/Stockholm, America/New_York; DST boundaries must not alter UTC storage |
| Quota runway | Stable decline, zero consumption, reset, non-monotonic data, stale data, gaps, changed reset, missing versus numeric zero |
| Budget pacing | Minimum days, 70% coverage boundary, incomplete current day, actual/estimated split, missing days, mixed currencies |
| Scope | Model/project/workspace grouping, unsupported dimensions, aggregate reconciliation without double counting |
| State transitions | Warning, critical, recovery, duplicate refresh, restart deduplication |
| Database | v4-to-v5 migration, rollback, history preservation, export/retention behavior |
| Concurrency | Cancellation, superseding generation, late result suppression, worker cleanup |
| UI | Narrow/wide layouts, keyboard, accessible names, non-color states, localization |
| Distribution | v16.0.1 install/update to v17, rollback, remove, user-data preservation |
| Performance | History and Analyst at 100k observations, runway query at 100k observations |

Maintain existing performance budgets:

- History query at 100,000 observations: under 2,000 ms.
- Analyst query at 100,000 observations: under 1,000 ms.
- Runway query at 100,000 observations: target at or below 750 ms.
- Zero retained workers after completion.
- One database connection after completion.
- Canonical startup median: at or below 427 ms.
- Popup opening: at or below 100 ms.

Release tests must use fixtures and mock HTTP. An optional real read-only provider smoke test may be documented, but it is non-blocking and must not use inference.

## 14. Final Verification Commands

Run focused tests after every phase, then run the complete supported gate:

```bash
just doctor
just build-debug
just test
just check
just qml-lint
just phase7-check
just release-check
PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py \
  --strict \
  --build-dir build/debug \
  --expected-version "$(cat VERSION)"
QT_QPA_PLATFORM=offscreen dbus-run-session -- \
  bash scripts/smoke_test_plasmoid.sh build/debug
just fedora44-check
git diff --check
```

Also inspect:

```bash
git status --short
git diff --stat
git diff
```

Report every skipped command with the exact reason. Do not report a release gate as passing when a required command was skipped.

## 15. Commit and Change Discipline

If the user later authorizes commits:

- Use small, testable, conventional commits.
- Keep correctness, schema, query engine, UI, integrations, cleanup, and release metadata separable.
- Do not mix mass formatting or unrelated cleanup into functional commits.
- Do not amend or rewrite user commits unless explicitly requested.
- Do not push, tag, or publish without separate explicit authorization.

Suggested commit sequence:

```text
fix(openai): paginate usage and cost buckets safely
feat(storage): add guardrail transition schema
feat(usage): expose scoped provider breakdowns
feat(runway): add deterministic quota and budget forecasts
feat(ui): surface runway guardrails
feat(notifications): emit deduplicated guardrail transitions
refactor(ui): use neutral output input terminology
docs: document runway guardrails
chore(release): prepare v17.0.0
```

## 16. Definition of Done

v17 is complete only when:

- OpenAI monthly totals cannot be silently truncated by pagination.
- Quota and budget forecasts follow the documented deterministic contracts.
- Missing, zero, actual, estimated, and currency-incompatible states remain distinct.
- Scope attribution is provider-supported and does not double-count.
- Forecast UI is integrated without a fourth tab.
- Notifications are opt-in, transition-based, private, and restart-safe.
- Database migration is transactional and preserves v16 history.
- Existing identities, secrets, configuration, and distribution contracts remain compatible.
- Performance and cleanup budgets pass.
- Documentation and release evidence match the implementation.
- Every required gate passes or an explicit blocker is reported.
- No commit, push, tag, installation, or release has occurred without authorization.

---

# Copy-Paste Codex Execution Prompt

```text
Implement the complete v17.0.0 “Runway Guardrails” plan in the current
plasma-ai-usage-monitor repository.

First read, in full:
- AGENTS.md and any nested applicable AGENTS.md files
- PLASMA_AI_USAGE_MONITOR_V17_CODEX_PLAN.md
- README.md
- ROADMAP.md
- CONTRIBUTING.md
- Justfile
- the current v16 implementation/release plan
- the existing database, provider, model, QML, test, packaging, and release-policy
  files relevant to this work

Verify the repository state and establish a test baseline before editing. Preserve
all existing user changes and report any pre-existing failures precisely.

Execute the v17 plan phase by phase. Start with Phase 0: implement bounded,
failure-safe pagination for OpenAI usage, daily costs, and monthly costs. This is
a v17 release blocker and must be complete before any forecast consumes those
values. Do not create v16.0.2.

Then implement:
1. Forecast Contract v1 and transactional database schema v5.
2. Provider-supported scope attribution without inferred dimensions or double
   counting.
3. Deterministic quota runway and monthly budget pacing in focused asynchronous
   query/model components.
4. Daily Focus, Source Detail, Analyst, Settings, and optional compact-panel UI
   integration without adding a fourth tab.
5. Opt-in, transition-only, restart-deduplicated notifications and low-cardinality
   integrations.
6. Neutral terminology cleanup, documentation, release evidence, and the final
   v17.0.0 version cut.

Follow the plan’s data contracts, unavailable-state rules, time boundaries,
currency rules, privacy constraints, performance budgets, migration requirements,
test matrix, and explicit out-of-scope list exactly.

Use deterministic local calculations only. Do not add LLM inference, a cloud
backend, currency conversion, provider writes, automatic model switching, new
providers, a new primary tab, or a broad provider-architecture rewrite.

Keep VERSION at 16.0.1 until the final release phase. Do not commit, push, tag,
publish, create a GitHub release, or install on the host unless I explicitly
authorize it.

After each phase:
- run the focused tests and static checks for that phase;
- fix regressions before continuing;
- update progress in the plan or an adjacent implementation checklist;
- record any changed assumption and its code evidence.

At the end, run every final verification command in the plan. If an environment
dependency prevents a command, report the exact blocker and continue with every
independent check that remains possible.

Use best judgment for implementation details that stay inside the approved scope.
Ask only when a material product decision, destructive action, new authority, or
scope expansion is required.

Deliver a final handoff containing:
- the implemented behavior by phase;
- important files and contracts changed;
- migrations and compatibility notes;
- tests and exact commands run;
- performance results;
- remaining blockers or skipped checks;
- git status and a concise diff summary;
- confirmation that no unauthorized commit, push, tag, installation, or release
  occurred.
```
