# Roadmap

**Current release:** 17.0.0, Runway Guardrails

**Last updated:** 2026-07-29

AI Usage Monitor remains a desktop-native, local-first Plasma widget. v17 adds
deterministic forward-looking guardrails without widening the read-only
provider or local-first boundary.

## Current release

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
[`docs/plans/PLASMA_AI_USAGE_MONITOR_V17_CODEX_PLAN.md`](docs/plans/PLASMA_AI_USAGE_MONITOR_V17_CODEX_PLAN.md).
Phases 0 through 6 are complete in the local release candidate. Public
publication remains a separately authorized operation.

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

- Improve scheduled exports and loopback Prometheus labels without widening the network boundary.
- Keep webhook payloads small and explicit.
- Improve Browser Sync diagnostics while treating the feature as Labs.

## Released foundations

| Release | Result |
| --- | --- |
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
