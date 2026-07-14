# Roadmap — Plasma AI Usage Monitor

> **Current version:** v13.0.0 (Provider Intelligence, prerelease development)
> **Last updated:** 2026-07-13
> **Direction:** Keep the widget desktop-native, local-first, and honest about data quality. Prefer setup clarity, diagnostics, export, notifications, and loopback integrations over backend/server expansion.

## Product Direction

The app is a KDE Plasma widget, not a cloud control plane. The strongest path is to make local monitoring reliable, explain what each data source can and cannot prove, and keep validation strict enough that stale metadata cannot silently ship.

The project stays focused on:

- local provider and subscription-tool monitoring
- optional Browser Sync Labs where technically realistic on Linux
- catalog-backed source and precision labels
- local integrations such as Prometheus, webhooks, and scheduled exports
- Fedora KDE 44 validation and package trust

## Version Summary

| Version | Codename | Theme | Status |
| ------- | -------- | ----- | ------ |
| v13.0.0 | **Provider Intelligence** | Non-invasive monitoring, metric truth, provider adapters | Prerelease development |
| v12.0.3 | **Reliability Core** | Catalog-driven provider model corrections | Released |
| v12.0.0 | **Reliability Core** | Deterministic refresh, typed state, correct history, and release confidence | Released |
| v11.0.0 | **Distribution & Catalog Truth** | RPM/COPR visibility, fresh catalogs, and release gates | Released |
| v10.0.1 | **Accuracy Patch** | Provider card catalog singleton import fix for installed v10 packages | Released |
| v10.0.0 | **Accuracy** | Cost correctness, probe honesty, source-aware history, and onboarding | Released |
| v9.0.0 | **Confidence** | Setup, diagnostics, catalog trust, and release hardening | Released |
| v8.0.0 | **Source Of Truth** | Local catalogs for pricing, plans, and quota windows | Released |
| v7.0.0 | **Beacon** | Fedora KDE 44 reliability, trust, and UX | Released |
| v6.0.1 | **Ground Truth** | Stabilization, trust, and metadata | Released |
| v6.0.0 | **Nexus (Light)** | UI redesign and local integrations | Released |
| v5.3.0 | **Vanguard** | Distribution and local tools | Released |

## v11.0.0 — "Distribution & Catalog Truth"

**Goal:** make Fedora RPM delivery and shipped pricing/plan catalogs trustworthy before expanding the product surface.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **COPR-first RPM path** | Document COPR as the supported Fedora install path and verify Fedora 44 builds before release. | Low |
| **Provider catalog refresh** | Refresh Provider Catalog v3 to 2026-06-30 with official source refs, DeepSeek v4 models, and visible manual-review reasons. | Medium |
| **Subscription catalog refresh** | Refresh Subscription Catalog v1 to 2026-06-30, including Copilot AI credits and Windsurf/Devin/Cognition source-conflict notes. | Medium |
| **Copilot billing label fix** | Keep Auto mode visible after the 2026-06-01 AI credits transition so tests and UI stay aligned. | Low |
| **Release checklist hardening** | Add v11 checklist items for COPR install/update/remove, artifacts, and issue #12 closure. | Low |

## v10.0.0 — "Accuracy"

**Goal:** make user-visible usage and cost numbers honest about source and confidence before adding provider breadth.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **OpenAI billing correctness** | Parse Costs API object amounts with currency warnings and legacy mock fallback only. | Low |
| **Probe isolation** | Keep health-check tokens and requests separate from displayed usage, spend, history spend, Prometheus spend, and budgets. | Medium |
| **Source-aware history** | Persist cost source, usage source, currency, and data quality through SQLite, exports, metrics, and Analyst summaries. | Medium |
| **Catalog/default gate** | Validate KConfig model defaults against Provider Catalog v3 during local, release, and CTest gates. | Low |
| **Goal-driven onboarding** | First-run flow starts with monitoring goal, data level, quick preset, and data-quality review. | Medium |
| **Trust Center actions** | Diagnostics includes wallet, catalog review, Browser Sync, insecure URL, install-shadowing, and copyable support-report actions. | Low |

## v9.0.0 — "Confidence"

**Goal:** finish the product loop around first-run setup, Diagnostics, catalog review visibility, config portability, and release validation without adding provider sprawl.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Single setup path** | Remove the stale standalone setup wizard and keep the maintained inline onboarding path in the popup. | Low |
| **Config portability v2** | Export/import every non-secret KConfig key while keeping API keys, browser cookies, PATs, and webhook URLs in KWallet. | Medium |
| **Actionable Trust Center** | Diagnostics show catalog review reasons, source conflicts, wallet state, Browser Sync readiness, local tool status, version, and loaded plugin path. | Medium |
| **Catalog review reasons** | Provider and subscription catalogs include user-visible reasons for manual-review and source-conflict flags. | Low |
| **Validation hardening** | Gates catch stale QML version strings, retired setup files, config export drift, catalog review gaps, and missing payload assets. | Low |

## v8.0.0 — "Source Of Truth"

**Goal:** make shipped JSON catalogs the local source of truth for provider pricing, subscription plans, quota windows, source metadata, and precision labels while keeping Browser Sync Labs optional and visibly sourced.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Provider Catalog v3** | Static provider/model pricing with source refs, reviewed dates, cached input pricing, context metadata, and non-token pricing status. | Medium |
| **Subscription Catalog v1** | Stable plan IDs, subscription prices, quota windows, Copilot billing modes, and manual-review/source-conflict markers. | Medium |
| **Quota source UI** | Subscription cards render normalized quota rows with readable source and precision badges. | Medium |
| **Trust Center catalog status** | Diagnostics expose provider/subscription catalog versions, review dates, stale state, runtime scraping disabled, and review/conflict counts. | Low |
| **Release gates** | Validation checks prevent missing source refs, stale catalogs, fake non-token pricing, and hardcoded subscription pricing. | Low |

## v7.0.0 — "Beacon"

**Goal:** make Fedora KDE 44 / Plasma 6.6 the native release target while improving trust, validation, Browser Sync Labs readiness, provider metadata maintenance, Copilot billing assumptions, and popup actionability.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Fedora 44 release gate** | CI, demo docs, release scripts, and `just fedora44-check` target Fedora KDE 44 as required. | Low |
| **Trust Center** | Diagnostics explain actual API usage, estimated cost, rate-limit-only data, local tool data, Browser Sync Labs, catalog freshness, KWallet, and provider health. | Low |
| **Provider Catalog v2** | Static local provider/model/pricing metadata plus validation; no runtime website scraping. | Medium |
| **Copilot 2026 billing scaffolding** | Premium request tracking remains, with usage-based/credits mode labels and configurable reset assumptions. | Medium |
| **Adaptive polling** | Slower closed-popup refresh, open-popup/manual refresh, jitter, and error backoff diagnostics. | Medium |

## Explicit Non-Goals

- No PostgreSQL migration inside the plasmoid/plugin
- No multi-user or cross-machine aggregation service
- No standalone web dashboard owned by this repo
- No team login/account system
- No non-Linux browser sync abstraction until there is a platform target that can exercise it
