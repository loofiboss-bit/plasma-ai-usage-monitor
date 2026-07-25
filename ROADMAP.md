# Roadmap

**Current release:** 15.0.0, Daily Control

**Last updated:** 2026-07-26

AI Usage Monitor remains a desktop-native, local-first Plasma widget. v15 makes daily status actionable while preserving the source and quality boundaries established by v13 and v14. The planned v16 Control Loop release will make the path from status to explanation and action more direct without widening the local-first boundary.

## Planned release

### 16.0.0 Control Loop

- Consolidate repeated Overview facts into one daily focus and source-first drill-down.
- Correct Analyst UTC range semantics and improve responsive History and Analyst layouts.
- Add optional Anthropic Admin API usage and cost reporting without weakening secret handling.
- Make QML lint and release-media evidence trustworthy, reproducible release gates.
- Preserve v15 configuration, KWallet secrets, schema-v4 history, package identity, and metric truth boundaries.

The decision-complete implementation plan is
[`docs/plans/PLASMA_AI_USAGE_MONITOR_V16_PLAN.md`](docs/plans/PLASMA_AI_USAGE_MONITOR_V16_PLAN.md).

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
