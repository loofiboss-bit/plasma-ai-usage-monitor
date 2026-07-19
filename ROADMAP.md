# Roadmap

**Current release:** 14.1.1, First Successful Use

**Last updated:** 2026-07-18

AI Usage Monitor remains a desktop-native, local-first Plasma widget. v14 makes one verified source the first-run goal and keeps provider-reported data, local estimates, balances, and connection checks separate.

## Current priorities

### Protect the first-success path

- Keep Guided first success focused on one useful source.
- Keep the monitoring level visible before verification.
- Keep authentication, permission, endpoint, and unsupported-metric errors distinct.
- Test missing and mismatched native plugins without importing the plugin first.

### Trustworthy metrics

- Preserve unknown values throughout the UI, history, exports, alerts, and Prometheus.
- Keep actual billing, estimates, balances, published caps, and connectivity separate.
- Review provider and subscription catalogs on a documented schedule.
- Add provider billing endpoints only when deterministic fixtures prove their semantics.

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
