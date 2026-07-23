# Plasma AI Usage Monitor v15.0.0 — Daily Control

**Document type:** Codex implementation plan  
**Status:** Proposed  
**Target repository:** `loofiboss-bit/plasma-ai-usage-monitor`  
**Planning baseline:** `main` at `43ae7ff7537b052740c2b935449a6f49d2823821`  
**Current release:** `14.1.1`  
**Target release:** `15.0.0`  
**Primary platform:** Fedora 44 KDE Plasma 6  
**Primary audience:** Individual developers who monitor AI API spend, coding-tool activity, subscription quota, and reset windows from the Plasma panel

---

## 1. Mission

v14 made first setup understandable and verifiable. v15 must make the monitor trustworthy and useful every day after setup.

The release must answer five questions quickly:

1. Does any configured source require attention now?
2. Which quota or budget is closest to its limit?
3. Which quota resets next, and when?
4. Which values are actual, estimated, stale, unavailable, or connectivity-only?
5. What changed over time without inventing zeroes or unsupported conclusions?

The top priority is **daily truth and actionability across the panel, Overview, History, Analyst, and notifications**.

Do not optimize v15 for a larger provider count. Of the 18 hosted providers, only a small subset currently reports actual usage, spend, balance, or gateway totals. Connectivity-only integrations are useful for diagnostics but must not dominate the main experience.

---

## 2. Implementation principles

Codex must follow these rules throughout implementation:

- Inspect the current implementation and tests before editing a component.
- Preserve the existing typed refresh lifecycle, Provider Metric Contract v2, Source Readiness Model, SQLite schema v4, KWallet behavior, and distribution architecture.
- Treat unavailable values as unavailable from backend to UI. Never convert missing data to zero for display, charting, alerts, exports, or summary calculations.
- Keep actual provider data, gateway data, account balances, local estimates, published limits, connectivity checks, and fixed subscription fees separate.
- Never sum incompatible currencies or add automatic currency conversion.
- Scheduled requests must remain read-only and non-billable.
- Keep COPR/source installations as the complete supported distribution. Keep KDE Store wording honest about the compiled plugin requirement.
- Make small, testable changes. Complete and verify each phase before moving to the next phase.
- Do not rewrite working architecture merely to introduce a different pattern.
- Do not mix unrelated cleanup into a phase.
- Preserve existing configuration keys and history during upgrades unless a migration is explicitly specified and tested.
- Use localized strings for all user-visible text.
- Follow KDE Human Interface Guidelines and existing Kirigami conventions.

---

## 3. Current-state assessment

### 3.1 Foundations to preserve

- Typed provider refresh lifecycle with coalescing, generation checks, typed errors, Retry-After handling, freshness, and cancellation.
- Provider Metric Contract v2 with nullable values and source, quality, scope, currency, reset, window, and observation metadata.
- SQLite schema v4 with normalized observations and compatibility projections.
- Transactional KWallet changes in Settings.
- Source Readiness Model shared by onboarding, Settings, Overview, and Diagnostics.
- Guided First Success and dependency-safe startup.
- Read-only provider catalog contracts and generated provider capability documentation.
- Local history, comparison queries, exports, Prometheus, notifications, and webhook integrations.
- Fedora 44 CI, COPR packaging, reproducible artifacts, release policy checks, SBOM generation, and KDE Store frontend packaging.
- No telemetry, hosted backend, or cloud database.

### 3.2 Verified repository state

At the planning baseline:

- `VERSION` is `14.1.1`.
- There are 18 provider profiles and 7 subscription/local-tool profiles.
- There are no open issues or pull requests.
- All repository-owned static checks pass when run individually:
  - version consistency;
  - no hardcoded semantic versions in QML;
  - provider and subscription catalog validation;
  - catalog-driven model defaults and pickers;
  - no hardcoded pricing;
  - configuration portability;
  - KCM contracts;
  - QML registered-type consistency;
  - non-invasive monitoring;
  - provider capability documentation;
  - demo contract;
  - release policy;
  - release media;
  - exact-tag policy.
- The v14.1.1 release evidence reports 39/39 tests passing.

### 3.3 User-facing defects and gaps

#### A. Old connection-count semantics remain

`FullRepresentation.qml` and `CompactRepresentation.qml` still summarize `providers connected`, even though v14 introduced a more accurate source-quality model. Local tools are not represented consistently in the panel state.

This creates visible contradictions. A screen can say one source reports useful data and three sources are connectivity-only, while the footer simply says four providers are connected.

#### B. Compact modes can fabricate zeroes

Several compact calculations use `0` as fallback when cost, requests, remaining quota, or limits are unavailable. A missing value may therefore appear as `$0`, `0 req`, or a healthy state.

#### C. History hides retained data

`HistoryView.qml` builds selectors from currently enabled providers and tools. If a source is disabled, its SQLite data remains but cannot be selected in the UI.

Chart extraction also converts missing metric fields to zero, which can create false dips and misleading comparisons.

#### D. Analyst overstates what the data proves

The current Analyst implementation:

- runs several database queries synchronously from QML;
- presents insufficient data as `0.0%` rather than unavailable;
- labels output/input token ratio as `Prompt Efficiency` and gives prescriptive advice unsupported by that ratio;
- may use the active 30-day overview currency state while building a 7-day report;
- mixes provider diagnostics into the analytical workflow;
- uses different semantic sources for daily trends and top-driver estimates, producing internally inconsistent cards.

#### E. Empty heatmap cells look populated

`ActivityHeatmap.qml` uses the theme hover color for zero-value days. In some themes, an empty 365-day heatmap appears fully colored.

#### F. Daily surfaces lack direct behavior tests

There is strong coverage for backend contracts, onboarding, readiness, and provider Settings. Direct behavior coverage is missing or weak for:

- CompactRepresentation;
- FullRepresentation;
- HistoryView;
- AnalystTab;
- NotificationController;
- daily source prioritization;
- unavailable/stale/mixed-currency UI behavior.

#### G. Documentation has competing sources of truth

Both `docs/wiki/` and `wiki/` contain overlapping but different documentation. The two trees already differ in structure and wording. Some newer content does not match the generated provider capability contract precisely.

#### H. Performance evidence is incomplete

The performance baseline still contains pending canonical Plasma measurements. Analyst queries remain synchronous and the release screenshots primarily exercise a wide `plasmawindowed` presentation rather than the normal narrow panel popup.

### 3.4 Maintainability pressure

The following files are large and should be split only where required by v15 behavior and testability:

- `package/contents/ui/configSubscriptions.qml`
- `package/contents/ui/ProviderCard.qml`
- `package/contents/ui/NativeMonitor.qml`
- `package/contents/ui/configDiagnostics.qml`
- `package/contents/ui/AnalystTab.qml`
- `package/contents/ui/RuntimeCoordinator.qml`

Do not perform a broad file-size refactor. Extract focused models and components as phases require them.

---

## 4. Scope

### 4.1 Included

- source-based daily status shared by panel, Overview, History, Analyst, and notifications;
- truthful unavailable, stale, actual, estimated, balance, connectivity, and reset semantics;
- action-first Overview focused on quota, reset, budget, freshness, and recovery;
- panel modes for daily monitoring rather than provider connection count;
- retained history access for disabled sources;
- unified provider and tool history navigation;
- metric-aware charts that preserve data gaps;
- asynchronous Analyst data loading;
- evidence-bound Analyst cards and reports;
- correct empty, low-sample, mixed-currency, and stale states;
- QML behavior tests for all changed daily surfaces;
- 1k, 10k, and 100k history performance verification;
- one canonical documentation tree;
- Fedora 44 upgrade and release verification from v14.1.1.

### 4.2 Explicitly excluded

- new connectivity-only providers;
- scraping undocumented web dashboards for additional subscription quota;
- automatic currency conversion;
- hosted accounts, cloud sync, telemetry, or a remote backend;
- team dashboards or multi-user storage;
- a full theme or branding redesign;
- replacement of CMake, QML, or the native plugin architecture;
- a database schema revision unless a verified query or correctness requirement cannot be satisfied by schema v4;
- Flatpak, AppImage, Windows, macOS, or broad multi-distribution packaging;
- LLM-generated recommendations or claims that cannot be derived deterministically from stored metrics;
- inference requests on the background schedule.

---

## 5. Release strategy

Implement Phase 0 as a separately releasable correctness checkpoint. If the defects are confirmed against the installed v14.1.1 package, publish them as `14.1.2` before landing the larger v15 architecture.

After Phase 0:

- create a dedicated v15 implementation branch;
- keep one logical commit checkpoint per phase;
- do not bump all version surfaces until runtime implementation and tests are complete;
- use a final release-preparation commit for version, changelog, AppStream, RPM, docs, and release checklist changes.

Recommended branch names:

```text
fix/v14.1.2-daily-truth
feat/v15-daily-control
```

---

## 6. Phase 0 — Daily truth baseline

### Objective

Remove current contradictions and false visual signals before building new daily-state architecture.

### Likely components

- `package/contents/ui/FullRepresentation.qml`
- `package/contents/ui/CompactRepresentation.qml`
- `package/contents/ui/ActivityHeatmap.qml`
- `package/contents/ui/EfficiencyMetricCard.qml`
- `package/contents/ui/AnalystTab.qml`
- `package/contents/ui/Utils.js`
- `package/contents/ui/views/HistoryView.qml`
- `package/contents/ui/UsageChart.qml`
- `docs/user-guide/`
- `docs/wiki/`
- `wiki/`
- test fixtures and QML test registration

### Tasks

1. Replace `providers connected` footer and accessibility text with a source-based summary.
2. Ensure tool-only configurations can produce an active, healthy panel state.
3. Replace zero fallbacks with unavailable values where availability is not proven.
4. Make compact cost, daily cost, request, and critical modes render an em dash or localized Unknown state when no compatible metric exists.
5. Ensure a green badge requires a verified useful or explicitly connectivity-only state, not merely an object with default scalar fields.
6. Render heatmap days without data using a neutral, visibly empty color distinct from any activity intensity.
7. Rename `Prompt Efficiency` to `Output / Input Ratio`.
8. Remove `Highly Efficient`, `Low Efficiency`, prompt-quality, and concise-prompt claims.
9. Show the ratio only when both compatible input and output samples exist.
10. Resolve the duplicate documentation trees:
    - designate `docs/user-guide/` as the canonical task-based source;
    - keep `docs/provider-capabilities.md` generated from the catalog;
    - either generate `docs/wiki/` from canonical content or remove the duplicate mirror;
    - remove the parallel root `wiki/` tree unless an explicit publishing workflow requires it;
    - add a check that fails when two maintained copies diverge.
11. Add regression tests for all corrected states.

### Acceptance criteria

- A provider-only, tool-only, and mixed setup has accurate panel and footer text.
- No unavailable cost or remaining-request value renders as zero.
- An empty heatmap contains no activity-colored cells.
- Output/input ratio has neutral language and is hidden when unavailable.
- Documentation has one declared source of truth.
- Existing v14.1.1 configuration and history remain unchanged.

### Verification

```bash
just build-debug
just test
just check
just qml-lint
PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py \
  --strict --build-dir build/debug --expected-version "$(< VERSION)"
dbus-run-session -- xvfb-run -a bash scripts/smoke_test_plasmoid.sh build/debug
```

### Commit checkpoint

```text
fix(daily-ui): preserve unavailable and source status semantics
```

---

## 7. Phase 1 — Unified Daily State Model

### Objective

Create one typed, testable daily summary contract so panel, Overview, notifications, and later analytical surfaces stop deriving partially different status from legacy scalar properties.

### Architecture

Introduce a dedicated `DailyStateModel` or equivalently named model. Do not overload `SourceReadinessModel` with all presentation and prioritization behavior.

The model may be implemented in C++ as a `QAbstractListModel` if that provides the strongest typed contract and cross-surface reuse. A QML state model is acceptable only if it can be isolated, fixture-driven, and fully tested without loading the entire runtime.

The model consumes:

- Source Readiness snapshots;
- provider Metric Contract v2 metrics;
- subscription-tool quota windows;
- configured budgets and alert thresholds;
- freshness and last-success state;
- catalog monitoring level and source identity.

### Required per-source fields

Each source row must expose stable, non-localized keys for:

- `stableId`;
- `displayName`;
- `sourceKind`;
- `monitoringLevel`;
- `readinessState`;
- `qualityClass`;
- `freshnessState`;
- `lastSuccess`;
- `lastAttempt`;
- `lastErrorKind`;
- `nextActionKey`;
- `hasUsefulData`;
- `hasActualData`;
- `hasEstimatedData`;
- `hasBalance`;
- `connectivityOnly`;
- `attentionSeverity`;
- `attentionReasonKey`;
- `primaryMetricKind`;
- `primaryMetricAvailable`;
- `primaryMetricValue`;
- `primaryMetricUnit`;
- `percentUsedAvailable`;
- `percentUsed`;
- `percentRemainingAvailable`;
- `percentRemaining`;
- `resetAtAvailable`;
- `resetAt`;
- `currency`;
- `costAvailable`;
- `costValue`;
- `costSource`;
- `budgetAvailable`;
- `budgetPercentUsed`.

Do not populate numeric fields with zero merely to satisfy QML typing. Pair every optional scalar with availability or expose it as a nullable `QVariant`.

### Required aggregate fields

Expose a summary containing:

- enabled source count;
- reporting-useful source count;
- actual source count;
- estimated/local source count;
- balance source count;
- connectivity-only source count;
- attention source count;
- stale source count;
- current highest severity;
- most urgent source;
- lowest remaining quota with source and window;
- nearest reset with source and window;
- currency-separated actual spend totals;
- currency-separated estimated totals;
- fixed subscription fees separately;
- last aggregate refresh completion.

### Prioritization rules

Use deterministic priority:

1. explicit critical/error state requiring user action;
2. quota exhausted or limit reached;
3. quota below critical threshold before reset;
4. budget above critical threshold;
5. authentication, permission, configuration, or schema error;
6. stale previously useful data;
7. warning threshold;
8. ready to verify;
9. connectivity-only healthy state;
10. normal reporting state.

Ties must be stable and documented. Prefer higher severity, then lower remaining percentage, then earlier reset, then stable source order.

### Tasks

1. Define model roles and stable enums/keys.
2. Build adapters from provider metrics and subscription-tool quota rows.
3. Keep source identity aligned with Provider Catalog and Subscription Catalog IDs.
4. Preserve mixed currencies as separate totals.
5. Encode unavailable and stale explicitly.
6. Add fixtures for provider-only, tool-only, mixed, unavailable, stale, mixed-currency, connectivity-only, balance, warning, and critical states.
7. Add table-driven unit or QML tests for prioritization.
8. Document the daily-state contract in `docs/architecture/`.

### Acceptance criteria

- Every enabled source appears exactly once.
- Panel and Overview can consume the same aggregate summary without independent source loops.
- Unknown, stale, and available zero are distinguishable.
- Tool-only configurations produce complete summary state.
- Mixed currencies remain separate.
- Source priority is deterministic across runs.

### Commit checkpoint

```text
feat(state): add unified daily source summary model
```

---

## 8. Phase 2 — Action-first Overview and panel

### Objective

Make the normal popup and panel immediately useful without requiring users to interpret every provider card.

### Overview information hierarchy

Render these sections in order:

1. **Needs attention** — only actionable problems or urgent limits.
2. **Quota and resets** — nearest reset and lowest remaining compatible window.
3. **Spend and budgets** — actual, estimated, and fixed fees kept separate.
4. **Sources** — compact source cards grouped by meaningful data, local estimate, balance, and connectivity-only.

### Tasks

1. Replace the five equally weighted count tiles with a compact headline and two or three relevant facts.
2. Hide zero-count categories instead of reserving permanent width for them.
3. Show one top action with a direct recovery button when attention exists.
4. Add a quota/reset summary that uses actual quota windows only.
5. Do not treat published documentation limits as live remaining quota.
6. Keep connectivity-only providers collapsed and visually secondary.
7. Reduce empty vertical space in provider and subscription cards.
8. Remove the redundant footer connection count. Retain database size only in Diagnostics unless it materially helps the daily workflow.
9. Update refresh tooltip from `providers` to `configured sources`.
10. Drive all summary state from the Daily State Model.

### Compact panel modes

Support these modes:

- `icon` — icon plus severity badge;
- `attention` — highest-priority source or all-clear state;
- `lowest-quota` — remaining percentage and source when actual/synced quota exists;
- `next-reset` — relative time until the nearest actual reset;
- `actual-spend` — compatible provider-reported spend only;
- `active-sources` — number of enabled useful sources, not connected providers.

Migrate old modes safely:

- `count` → `active-sources`;
- `critical` → `attention`;
- `cost` → `actual-spend`;
- `dailycost` may remain only if it is based on an available compatible daily metric;
- `requests` must show unavailable when no valid remaining-request metric exists.

Preserve old stored values with a compatibility mapping. Do not break existing configuration import.

### Responsive behavior

- Design for a real Plasma panel popup first.
- Validate the existing default implicit width and height.
- Use wide `plasmawindowed` screenshots only as secondary evidence.
- Avoid five horizontal columns in narrow mode.
- Ensure text remains readable at 125%, 150%, and 200% scaling.

### Acceptance criteria

- The most important problem or reset is visible without scrolling.
- A source that only proves connectivity cannot make the overall state look like usage is reporting.
- A tool-only setup has correct panel status.
- No panel mode displays a fabricated zero.
- Connectivity-only sources remain accessible but do not dominate.
- Keyboard focus order follows visual order.

### Commit checkpoint

```text
feat(overview): prioritize quota resets and actionable status
```

---

## 9. Phase 3 — Unified retained History

### Objective

Make local history complete, source-aware, and truthful even after sources are disabled.

### Source discovery

Build selectable sources from the union of:

- configured provider descriptors;
- configured subscription tools;
- `UsageDatabase::getProviders()`;
- `UsageDatabase::getToolNames()`.

Deduplicate by stable history identity. Mark each source as enabled, disabled, or history-only.

### Metric discovery

Do not expose a metric tab merely because the component supports it. Determine available metric kinds from stored observations or compatible snapshots.

At minimum support:

- provider cost;
- provider tokens;
- provider requests;
- provider rate-limit utilization when a valid limit/remaining pair exists;
- tool usage count;
- tool percent used;
- tool remaining quota when derivable from a real or explicitly configured limit;
- reset-window series where semantically meaningful.

### Missing-data semantics

- Preserve missing points as gaps.
- Distinguish an available numeric zero from unavailable.
- Do not draw a line through incompatible source changes.
- Do not sum cumulative gauges as interval totals.
- Do not relabel rolling-window gauges as calendar-day totals.
- Keep currency per series and reject silent mixed-currency sums.

### Query contract

Extend the existing asynchronous request pattern. Return metadata with each result:

- source ID and display name;
- source kind;
- metric kind and unit;
- currency;
- source quality classes present;
- sample count;
- available point count;
- first and last observation;
- bucket size;
- whether the series contains gaps;
- whether it is stale or history-only.

### UI tasks

1. Replace provider-only detail selection with unified source selection.
2. Add visible status for disabled/history-only sources.
3. Filter metric choices based on actual stored compatibility.
4. Keep comparison limited to compatible units, semantics, and currencies.
5. Add 90-day range.
6. Add a bounded custom range only if it can be implemented without making the popup layout unstable.
7. Show sample and coverage metadata below the chart.
8. Improve repeated time labels for short windows.
9. Use a file export action as the primary export path.
10. Keep copy-to-clipboard as an explicit secondary action.
11. Preserve existing scheduled export functionality.

### Tests

Add cases for:

- disabled provider with retained history;
- disabled tool with retained history;
- history-only unknown source name;
- available zero versus unavailable;
- gaps in a series;
- one data point;
- two data points;
- mixed currencies;
- incompatible comparison request;
- rolling quota window;
- cancellation or supersession of an older async request;
- 100k observation database.

### Acceptance criteria

- Disabling a source does not remove access to its history.
- Provider and tool detail modes use one navigation model.
- Unsupported metrics are absent rather than plotted as zero.
- Comparisons reject incompatible units or currencies with a clear explanation.
- Large queries do not block the UI thread.
- Existing schema v4 databases open without migration or data loss unless an index-only migration is proven necessary.

### Commit checkpoint

```text
feat(history): unify retained provider and tool timelines
```

---

## 10. Phase 4 — Trustworthy asynchronous Analyst

### Objective

Turn Analyst into a reliable interpretation layer over sufficient compatible history instead of a collection of always-visible KPI cards.

### Backend contract

Add an asynchronous request API such as:

```cpp
Q_INVOKABLE void requestAnalyst(const QString &requestId,
                                const QDateTime &from,
                                const QDateTime &to,
                                const QString &currency = QString());
```

Return one internally consistent snapshot containing:

- request ID;
- requested range;
- generated timestamp;
- currency or mixed-currency status;
- data coverage;
- actual versus estimated sample counts;
- daily compatible spend series;
- activity series;
- output/input ratio series;
- top compatible drivers;
- anomaly candidates;
- method/threshold metadata;
- explicit availability for every derived KPI.

Use a worker-owned database connection as the existing History and export paths do. Never access the UI object's SQL connection from another thread.

### Statistical and semantic rules

1. **Average daily spend** requires compatible interval-total cost observations.
2. **Week-over-week change** requires adequate coverage in both complete comparison windows.
3. **Volatility** requires a documented minimum number of non-missing daily samples.
4. **Anomaly detection** must state its baseline and minimum absolute threshold.
5. **Top drivers** must use compatible semantics with the headline spend period.
6. **Output/input ratio** is descriptive only. It is not a quality, productivity, prompt clarity, or efficiency score.
7. **Mixed currencies** pause only cost-derived cards. Token and activity analysis may remain available.
8. **Estimated and actual costs** must be shown separately or clearly labeled.
9. **Connectivity probes** must never contribute to spend or activity analysis.
10. **Insufficient data** must display an unavailable explanation, not `0.0`.

### UI structure

Use progressive sections:

1. data coverage and period;
2. spend trend, only when available;
3. activity trend, only when available;
4. top drivers, only when compatible;
5. anomalies, only when enough data exists;
6. deterministic written summary with method-aware language.

Remove Provider Diagnostics from Analyst. Diagnostics remains the single place for endpoint, installation, database, KWallet, readiness, and support reports.

### Reports

- Build 7-day and 30-day reports from their own analyst snapshot.
- Do not reuse global UI state from another period.
- Include data coverage, source quality, currency, and unavailable sections.
- Do not include raw endpoints, credentials, cookies, account identifiers, or unrestricted backend error strings.
- Preserve localization in UI copy. Exported technical field keys may remain stable English identifiers where appropriate.

### Tests

Add cases for:

- no data;
- one sample;
- fewer than the required daily samples;
- complete 7-day data;
- complete 14-day comparison data;
- missing days;
- available zero spend;
- actual and estimated cost mixed;
- multiple currencies;
- connectivity-only data;
- tool activity without provider cost;
- report period isolation;
- async request supersession;
- 100k observation fixture.

### Acceptance criteria

- Opening Analyst never performs synchronous heavy database work on the UI thread.
- Every derived card has an availability decision and sample requirement.
- No insufficient-data result appears as a real zero.
- Output/input ratio contains no unsupported quality claims.
- 7-day and 30-day reports use their own period data.
- Mixed currencies do not disable unrelated activity analysis.
- Provider Diagnostics is removed from Analyst.

### Commit checkpoint

```text
refactor(analyst): make insights asynchronous and evidence-bound
```

---

## 11. Phase 5 — Daily notification alignment

### Objective

Ensure notifications use the same source priority, availability, quota, and freshness semantics as the visible UI.

### Tasks

1. Consume Daily State Model severity and action keys.
2. Do not alert on unavailable values as if they were zero or exhausted.
3. Group multiple quota-window warnings from one source.
4. Include source, window, remaining percentage, and reset time when available.
5. Distinguish actual live quota from local estimates and configured limits.
6. Suppress duplicate warnings during the existing cooldown.
7. Preserve DND behavior.
8. Keep webhook payloads compact, redacted, and source-explicit.
9. Add reconnect/recovery notifications only when the previous state was a real failure.
10. Test stale snapshot behavior so a temporary refresh failure does not emit false quota changes.

### Acceptance criteria

- Overview and notification severity agree for the same fixture.
- Unknown values never produce quota-exhausted warnings.
- Reset text appears only when returned or safely derived by the source contract.
- Grouped warnings remain within one notification per source and cooldown window.

### Commit checkpoint

```text
refactor(alerts): align notifications with daily source state
```

---

## 12. Phase 6 — Maintainability extraction

### Objective

Extract only the components required to make v15 daily behavior independently testable.

### Recommended extractions

- split Analyst visual sections from its controller/state;
- split compact metric selection from CompactRepresentation rendering;
- introduce a reusable metric-availability formatter;
- split History controller/query state from charts;
- split subscription settings into source list, plan editor, Antigravity editor, Copilot editor, and Browser Sync Labs editor if v15 changes touch those regions;
- keep RuntimeCoordinator as orchestration rather than presentation logic.

### Constraints

- Do not rename public QML module imports unnecessarily.
- Keep package payload checks updated.
- Preserve QML type registration consistency.
- Avoid a large unrelated ProviderCard rewrite.
- Add tests before or with each extraction.

### Acceptance criteria

- Daily models can be tested without launching the whole plasmoid.
- Analyst and History state can be fixture-driven.
- No behavior changes beyond the v15 specification are introduced by extraction.

### Commit checkpoint

```text
refactor(ui): isolate daily history and analyst components
```

---

## 13. Phase 7 — Performance and accessibility gates

### Performance targets

Measure on the canonical Fedora 44 KDE Plasma environment.

Record:

- cold `plasmawindowed` startup;
- first real panel-popup open;
- warm panel-popup open;
- Analyst initial render;
- History query completion at 1k, 10k, and 100k observations;
- Analyst query completion at 1k, 10k, and 100k observations;
- idle CPU and RSS over 15 minutes;
- database connection count before and after repeated tab switching;
- pending worker count after request supersession;
- network request counts during startup and fresh popup open.

Required behavior:

- no synchronous database query causing visible UI freeze;
- no sustained worker, watcher, connection, or memory growth;
- no duplicate startup refresh per source;
- no network refresh on a fresh popup when cached data is still valid;
- async results from superseded requests must not overwrite newer UI state.

Do not invent numeric baselines. Record actual results and set release thresholds from the v14.1.1 baseline on the same VM.

### Accessibility tests

Validate:

- complete keyboard navigation;
- visible focus indicators;
- meaningful accessible names for panel modes and source actions;
- screen-reader summary that includes severity and source rather than only count;
- color-independent warning and critical distinctions;
- 100%, 125%, 150%, and 200% scale;
- narrow panel popup and wide `plasmawindowed`;
- long localized strings;
- high-contrast and alternate Plasma themes.

### Commit checkpoint

```text
test(daily-ui): add performance and accessibility release gates
```

---

## 14. Phase 8 — Documentation, media, and release preparation

### Documentation updates

Update:

- `README.md`;
- `ROADMAP.md`;
- `CHANGELOG.md`;
- `docs/architecture/`;
- canonical user guide;
- generated wiki mirror if retained;
- provider capability documentation only through its generator;
- AppStream metadata;
- KDE Store description and submission checklist;
- Fedora RPM metadata;
- security/privacy documentation if report or notification fields change.

Document:

- daily source status semantics;
- actual versus estimated versus unavailable;
- lowest quota and next-reset rules;
- retained history for disabled sources;
- compatible comparisons;
- Analyst sample requirements and methodology;
- mixed-currency behavior;
- tool-only setup behavior;
- panel display modes;
- migration from v14 compact modes.

### Release screenshots

Capture truthful screenshots for:

- real narrow Overview popup;
- attention state;
- quota/reset state;
- tool-only Overview;
- retained disabled-source History;
- History with a real gap;
- Analyst with sufficient data;
- Analyst insufficient-data state;
- compact panel lowest-quota or next-reset mode;
- Settings only if changed materially.

Do not use data that makes unavailable values appear actual. Record the fixture session and screenshot manifest as current release tooling requires.

### Version preparation

Use the repo-owned bump tooling only after runtime and verification are complete:

```bash
just bump 15.0.0
```

Confirm version consistency across:

- `VERSION`;
- CMake project version;
- package metadata;
- AppStream metadata;
- RPM spec;
- README;
- changelog;
- release checklist;
- catalog release metadata where required by existing policy.

### Commit checkpoint

```text
chore(release): prepare v15.0.0 daily control
```

---

## 15. Required test matrix

### Daily-state fixtures

| Fixture | Expected result |
| --- | --- |
| No sources configured | Guided setup or clear empty state |
| One connectivity-only provider | Connected check, no useful usage claim |
| OpenAI actual spend | Actual source and compatible spend |
| OpenRouter key usage | Key-scoped usage with correct scope label |
| LiteLLM gateway spend | Gateway-reported data, not provider billing |
| DeepSeek balance | Balance, not historical spend |
| Codex live quota only | Tool-only active state with quota and reset |
| Claude local estimate only | Estimated/local label |
| Antigravity live model quota | Dynamic quota rows plus nearest reset |
| Provider actual + tool estimate | Sources remain separate |
| Stale previous success | Stale useful data plus retry action |
| Authentication failure | Actionable auth error |
| Permission failure | Permission-specific action |
| Unsupported metric | Connectivity or unsupported state, not zero |
| Available numeric zero | Real zero remains visible as zero |
| Unavailable numeric value | Em dash/Unknown, never zero |
| Mixed currencies | Separate totals and cost-analysis guard |
| Critical quota | Highest-priority attention state |
| Multiple critical sources | Stable deterministic priority |

### History fixtures

| Fixture | Expected result |
| --- | --- |
| Disabled source with history | Selectable as history-only |
| Missing middle interval | Visible chart gap |
| One sample | No misleading trend line |
| Compatible provider comparison | Valid shared axis |
| Incompatible units | Comparison blocked with explanation |
| Mixed currencies | No summed cost line |
| Tool percent history | Tool detail chart available |
| Rolling quota | Correct gauge/window semantics |

### Analyst fixtures

| Fixture | Expected result |
| --- | --- |
| Empty database | Empty state, no zero KPIs |
| Insufficient samples | Coverage explanation |
| Complete 7-day actual costs | Valid period snapshot |
| Complete 14-day actual costs | Valid week-over-week comparison |
| Estimated-only costs | Explicit estimated analysis |
| Mixed actual and estimated | Separated or clearly labeled |
| Mixed currencies | Cost cards paused, activity preserved |
| Token activity without costs | Activity analysis only |
| Connectivity-only history | No spend analysis |
| 100k observations | Async completion without UI block |

---

## 16. Full verification workflow

Run after each phase where relevant:

```bash
just build-debug
ctest --preset debug --output-on-failure
just check
just qml-lint
PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py \
  --strict --build-dir build/debug --expected-version "$(< VERSION)"
dbus-run-session -- xvfb-run -a bash scripts/smoke_test_plasmoid.sh build/debug
```

Before release:

```bash
just release-check
just fedora44-check
cmake --preset sanitizers
cmake --build --preset sanitizers --parallel "$(nproc)"
ctest --preset sanitizers --output-on-failure
```

Also verify:

- ShellCheck;
- AppStream validation;
- rpmlint;
- source and plasmoid reproducibility;
- package payload;
- exact-tag policy;
- SBOM generation;
- COPR Fedora 44 build;
- v14.1.1 → v15 upgrade;
- rollback and v15 re-upgrade;
- clean install;
- removal with user history preservation according to existing package policy;
- KDE Store frontend against matching and mismatched native plugins.

---

## 17. Upgrade and compatibility requirements

- Preserve all v14.1.1 KConfig keys.
- Map legacy compact modes to their v15 equivalents without resetting user choice.
- Preserve KWallet folder and credential keys.
- Preserve SQLite schema v4 data and backup behavior.
- Preserve provider and subscription stable IDs.
- Preserve catalog-driven model choices.
- Preserve History retention settings and scheduled export settings.
- Preserve notifications, DND, and webhook configuration.
- Preserve Guided First Success completion state.
- Do not delete stale local installation layers automatically.
- Keep config export/import backward compatible where feasible; version the export format only if new required fields cannot be optional.

Add explicit migration tests for every changed configuration value.

---

## 18. Definition of Done

v15.0.0 is complete only when all statements below are true:

- Panel, Overview, History, Analyst, and notifications use the same daily source truth.
- Tool-only installations work without requiring an enabled API provider.
- Unavailable is distinguishable from an available zero everywhere.
- No chart inserts false zeroes for missing observations.
- The nearest reset and lowest quota come only from compatible, available quota windows.
- Connectivity-only sources never imply usage or spend.
- Disabled sources remain accessible in History while retained data exists.
- Provider and tool history share one navigation and compatibility model.
- Analyst performs no heavy synchronous database query on the UI thread.
- Every derived Analyst metric has a documented sample requirement.
- Output/input ratio contains no unsupported quality or productivity claims.
- Mixed currencies are never silently converted or summed.
- Actual and estimated cost remain distinct.
- Empty heatmaps look empty.
- Daily QML surfaces have direct behavior tests.
- 1k, 10k, and 100k performance evidence is recorded.
- Narrow popup and scaling checks pass.
- There is one canonical user-documentation source.
- All current static, build, test, QML, package, sanitizer, Fedora, and release gates pass.
- v14.1.1 configuration, secrets, and history survive upgrade.
- Release screenshots reflect actual v15 behavior and data semantics.

---

## 19. Work that must remain deferred

Record these as future candidates, not hidden v15 scope:

- fully unified Providers and Subscriptions KCM architecture;
- new provider billing endpoints after deterministic contract fixtures exist;
- broader distro packaging;
- optional custom dashboard pinning beyond the v15 compact modes;
- richer export file dialogs if the Plasma runtime makes them unreliable in this release;
- schema v5 only if a future feature requires new persisted semantics;
- team or cross-machine aggregation;
- documented vendor quota APIs that do not yet exist.

Do not create placeholder or partially functional implementations for deferred work.

---

## 20. Codex execution checklist

Before implementation:

- [ ] Confirm `main` and baseline SHA.
- [ ] Read `AGENTS.md`, `README.md`, `ROADMAP.md`, `Justfile`, architecture docs, current v14 plan, and release checklist.
- [ ] Run and record baseline build, tests, checks, QML smoke, and package payload.
- [ ] Record a clean git status.
- [ ] Reproduce Phase 0 defects with fixtures or screenshots.

For every phase:

- [ ] Add or update tests first where practical.
- [ ] Implement the smallest complete behavior.
- [ ] Run focused tests.
- [ ] Run full local checks appropriate to the phase.
- [ ] Inspect the actual narrow Plasma popup.
- [ ] Review unknown/zero/stale/mixed-currency behavior.
- [ ] Update architecture docs when a contract changes.
- [ ] Commit with one conventional commit message.
- [ ] Do not proceed with a dirty or failing phase unless the blocker is documented.

Before final release:

- [ ] Complete the full verification workflow.
- [ ] Capture canonical performance evidence.
- [ ] Validate upgrade, rollback, clean install, and removal.
- [ ] Update all version surfaces once.
- [ ] Regenerate derived documentation and media.
- [ ] Confirm no duplicate wiki truth remains.
- [ ] Confirm no open release-blocking issue remains.
- [ ] Tag only the exact verified commit according to repository release policy.

---

## 21. Final instruction to Codex

Implement this plan phase by phase. Preserve the working v12–v14 reliability foundations and make daily monitoring truthful before adding breadth.

If repository evidence contradicts a task in this plan:

1. stop that task;
2. document the concrete conflict with file and test evidence;
3. choose the smallest change that preserves the mission and existing contracts;
4. update the plan or release checklist before continuing.

Do not reinterpret the mission as a provider expansion, generic visual redesign, or architectural rewrite. The release succeeds when daily state, quota, resets, spend, history, and analysis are consistent, actionable, and evidence-bound.
