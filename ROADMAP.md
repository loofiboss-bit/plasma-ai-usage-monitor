# Roadmap

**Current release:** 21.0.0, Source Health & Data Quality (candidate)

**Last updated:** 2026-09-24

## Current release

### 21.0.0 Source Health & Data Quality (candidate)

- Distinguish a detected local tool waiting for first activity from a source
  reporting an estimate.
- Exclude stale, expired, future-dated, and undated metrics from daily
  availability and aggregates while retaining timestamped last-known values.
- Add capability-aware source health with attempt, successful-check,
  Retry-After, and scheduled-refresh context.
- Remove the mixed monthly exposure metric and export actual spend, estimated
  burn, and fixed subscription fees separately by known currency.
- Qualify the local candidate through tests, package checks, and Fedora
  lifecycle controls before planning publication.

### 20.1.1 Codex Local-Auth Recovery

- Recover automatic Codex quota sync after `codex login` rotates credentials
  without requiring a Plasma restart.
- Ignore missing, malformed, incomplete, or in-flight auth snapshots when
  deciding whether to reset the automatic retry latch.
- Keep browser-based Codex fallback on its explicit sync path.

### 20.1.0 Quota Observability & Platform Flexibility

- Refresh Codex quotas from the user's local CLI login independently of browser sync.
- Add an automatic update check privacy control to gate external release checks and stop periodic polling.
- Export live tool quota remaining and reset timestamps to Prometheus and allow opt-in all-IPv4 network listening.
- Provide platform-aware recovery and version check guidance for Fedora, FreeBSD, and generic Linux.

### 20.0.0 Reliable Daily Quotas

Prepared locally; publication is pending final qualification. Quota observation
freshness, independently selected reset times, normalized tooltip presentation,
and subscription evidence lifecycle are the focus. The versioned checklist
records current evidence and remaining gates.

### 19.0.1 Catalog Drift Reliability

- Separate expected lifecycle, unavailable-pricing, and transient network
  review states from actionable catalog drift.
- Refresh Catalog v7 evidence and current Gemini pricing without enabling
  runtime scraping or automatic price activation.
- Keep the native Google backend default aligned with an active catalog model.

### 19.0.0 Verified Cost Intelligence

- Replace runtime input/output price copies with one exact-match Cost Engine v2.
- Ship Catalog v7 metadata for cache reads/writes, context tiers, service tiers,
  lifecycle, evidence, sequence, and hard expiry.
- Persist estimate provenance in snapshots, normalized observations, and exports.
- Verify signed remote catalogs before atomic activation and reject rollback,
  expiry, schema, and digest failures.
- Explain estimates in Source Detail and surface review, conflict, unknown-price,
  and retirement-watch states in the existing provider surfaces.
- Qualify the exact release commit before GitHub, COPR, and KDE Store publication.

The implementation plan is
[`docs/plans/PLASMA_AI_USAGE_MONITOR_V19_CODEX_PLAN.md`](docs/plans/PLASMA_AI_USAGE_MONITOR_V19_CODEX_PLAN.md).

### 18.0.0 Budget Control

- Replace fixed runtime budgets with validated aggregate or scoped policies.
- Add schema v6, config schema v3, calendar/time-zone-aware billing cycles and
  deterministic minor-unit pacing.
- Show safe today, remaining daily allowance, projection, previous period and
  `Unattributed` scope reconciliation.
- Persist warning, critical, exceeded, recovery and reset transitions before
  delivery, with restart deduplication and period snooze.
- Keep external payloads allow-listed and Prometheus labels low-cardinality.
- Preserve exact v17/v18 popup evidence, Fedora lifecycle, reproducible
  artifacts and public readback as release-blocking proof.

## Previous release

### 17.0.0 Runway Guardrails

- Make OpenAI usage and daily/monthly cost pagination bounded, cursor-complete,
  and failure-safe before forecasts consume totals.
- Add Forecast Contract v1, transactional schema v5, deterministic quota runway,
  and completed-UTC-day monthly budget pacing.
- Show provider-reported model, project, workspace, service-tier, and line-item
  detail without inferring dimensions or double counting aggregates.
- Integrate guardrails into Daily Focus, Source Detail, Analyst, and Settings
  without a fourth primary tab.
- Add opt-in, transition-only, restart-deduplicated notifications and
  aggregate-only Prometheus guardrail metrics.
- Preserve v16 configuration, KWallet secrets, observation history, package
  identity, actual/estimated separation, and currency boundaries.

The implementation plan is
[`docs/plans/PLASMA_AI_USAGE_MONITOR_V18_CODEX_PLAN.md`](docs/plans/PLASMA_AI_USAGE_MONITOR_V18_CODEX_PLAN.md).
Phases 0 through 6 and local Phase 7 qualification are complete. Public
publication requires exact-tag artifacts and readback from every named surface.

## Current priorities

### Protect daily truth

- Keep unavailable values distinct from real numeric zeroes.
- Keep lowest-quota and next-reset modes limited to synchronized or provider-reported windows.
- Keep actual spend, estimates, balances, and fixed fees separate by currency.
- Keep deterministic recovery actions ahead of secondary dashboard detail.

### Useful retained history

- Preserve disabled-source history and explicit chart gaps.
- Compare only compatible units, semantics, scopes, windows, and currencies.
- Keep Analyst methodology, coverage, and sample requirements visible.
- Keep History and Analyst queries asynchronous and bounded at 100k observations.

### Reliable Fedora delivery

- Keep COPR as the supported Fedora package.
- Test clean install, update, rollback, removal, and user-data preservation.
- Detect mixed user-local and system versions before they cause QML plugin failures.
- Keep the native-plugin requirement before every KDE Store install action.

### Useful local integrations

- Improve scheduled exports and low-cardinality Prometheus labels while keeping
  loopback as the secure default and all-IPv4 listening as an explicit,
  unauthenticated opt-in.
- Keep webhook payloads small and explicit.
- Improve Browser Sync diagnostics while treating the feature as Labs.

## Released foundations

| Release | Result |
| --- | --- |
| 18.0.0 Budget Control | Typed local policies, billing-cycle-aware pacing, scoped cost reconciliation, staged editing, persistent transitions, schema v6 and catalog v6 |
| 17.0.0 Runway Guardrails | Complete OpenAI pagination, deterministic quota and budget forecasts, provider-reported scope detail, schema v5, and private restart-safe integrations |
| 16.0.1 Control Loop | Restore localized Settings and widget text and add a release-blocking QML localization contract |
| 16.0.0 Control Loop | Daily Focus, Source Detail, responsive History and Analyst, exact UTC periods, optional Anthropic Admin reporting, and reproducible release evidence |
| 15.0.0 Daily Control | Shared daily source state, action-first Overview, truthful compact modes, retained History, evidence-bound Analyst, unified notifications, and performance/accessibility gates |
| 14.0.0 First Successful Use | Dependency-safe startup, Guided first success, source-focused Settings, outcome-first Overview, and native Diagnostics |
| 13.0.0 Provider Intelligence | Read-only scheduled monitoring, nullable metric contract, SQLite schema v4, Catalog v5 adapters, LiteLLM, Cerebras, Fireworks, and Perplexity |
| 12.0.x Reliability Core | Typed refresh lifecycle, source-aware history, catalog-driven models, KWallet caching, correct Fedora plugin packaging |
| 11.0.0 Distribution and Catalog Truth | COPR-first installation, reviewed catalogs, release validation |
| 10.0.x Accuracy | Billing parsing, source labels, unknown-value handling, onboarding and diagnostics |
| 9.0.0 Confidence | Config portability, catalog trust, plugin-path diagnostics |
| 8.0.x Source of Truth | Local provider and subscription catalogs |
| 7.0.0 and earlier | Fedora/Plasma reliability, Analyst view, local tool tracking, alerts, history, and budget features |

Detailed release history lives in [CHANGELOG.md](CHANGELOG.md).

## Non-goals

- hosted account or team service
- cloud database or cross-machine aggregation
- server-side telemetry
- runtime scraping of provider pricing pages
- automatic currency conversion
- a frontend-only package presented as a complete native installation
- non-Linux Browser Sync without a tested platform target
