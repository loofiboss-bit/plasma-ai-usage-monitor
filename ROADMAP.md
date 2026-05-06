# Roadmap — Plasma AI Usage Monitor

> **Current version:** v8.0.0 (Source Of Truth)
> **Last updated:** 2026-05-06
> **Direction:** Keep the widget desktop-native and local-first. Prefer export, notifications, and loopback integrations over backend/server expansion.

## Analyst's Note on Feasibility

The earlier Horizon/Nexus direction pushed toward web dashboards, PostgreSQL, and multi-user infrastructure. That is not a good fit for a KDE Plasma widget. The project now stays focused on:

- local monitoring and distribution
- browser sync where technically realistic on Linux
- local integrations such as Prometheus, webhooks, and scheduled exports

Chrome/Chromium cookie decryption and AWS Bedrock support remain in scope, but both are explicitly higher-risk Linux desktop integrations.

## Version Summary

| Version | Codename | Theme | Status |
| ------- | -------- | ----- | ------ |
| v5.3.0 | **Vanguard** | Distribution and local tools | Released |
| v5.4.1 | **Link** | Advanced sync and enterprise API | Released |
| v6.0.0 | **Nexus (Light)** | UI Redesign and integration | Released |
| v6.0.1 | **Ground Truth** | Stabilization, trust, and metadata | Released |
| v7.0.0 | **Beacon** | Fedora KDE 44 reliability, trust, and UX | Released |
| v8.0.0 | **Source Of Truth** | Local catalogs for pricing, plans, and quota windows | Current |

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
| **Browser Sync Labs readiness** | Clear profile, cookie DB, safe-storage, and service-probe states for Firefox, Chrome, Chromium, and Brave. | Medium |
| **Adaptive polling** | Slower closed-popup refresh, open-popup/manual refresh, jitter, and error backoff diagnostics. | Medium |

## v6.0.1 — "Ground Truth"

**Goal:** A focused stabilization update to fix version drift, build/runtime consistency, plugin wiring, docs drift, and packaging trust.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Consistency** | Single source of truth for versions across the repository. | Low |
| **Plugin Wiring** | Ensure all QML registered C++ types are properly compiled and linked. | Low |
| **Metadata** | Update RPM spec, AppStream/metainfo, and package metadata. | Low |

## v6.0.0 — "Nexus (Light)"

**Goal:** extend browser sync beyond Firefox on Linux and add AWS Bedrock without turning the widget into a cloud control plane. Includes 5.4.1 hotfix for COPR test environments.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Chrome/Chromium sync** | Linux-only profile discovery plus cookie reads/decryption when safe-storage secrets are available via Secret Service or KWallet-compatible paths. | Medium |
| **AWS Bedrock** | Add a Bedrock provider using AWS Signature Version 4 and region/model-aware connectivity checks, with estimated cost where direct spend data is unavailable. | Medium/High |
| **JetBrains AI** | Track local JetBrains AI activity from `~/.config/JetBrains/`. | Low |

## v5.3.0 — "Vanguard"

**Goal:** complete distribution work and cover the fast-growing AI coding tool surface using local filesystem-backed monitoring.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Flatpak package** | Finalize the existing Flatpak scaffold and add CI validation for manifest consistency, build, and test coverage. | Low |
| **Cursor** | Track local activity and self-managed limits from `~/.cursor/`. | Low |
| **Windsurf (Codeium)** | Track local activity and self-managed limits from `~/.codeium/`. | Low |
| **Shared local monitor base** | Consolidate install detection, recursive watching, debounce, and self-tracked usage increments for local subscription tools. | Low |

## v6.0.0 — "Nexus (Light)"

**Goal:** add practical team-adjacent integrations without building a custom backend.

| Feature | Description | Technical Risk |
| ------- | ----------- | -------------- |
| **Prometheus exporter** | Expose a local loopback-only `/metrics` endpoint for Grafana/Prometheus pipelines. | Medium |
| **Slack & Discord webhooks** | Send budget, quota, and connectivity alerts to incoming webhooks using the existing notification pipeline as the event source. | Low |
| **JSON/CSV auto-export** | Write timestamped JSON and CSV exports from local SQLite history to user-selected directories, including network-mounted paths. | Low |
| **Removal of PostgreSQL/team backend work** | Explicitly drop PostgreSQL, multi-user database, login, and web dashboard plans from the widget itself. | N/A |

## Explicit Non-Goals

- No PostgreSQL migration inside the plasmoid/plugin
- No multi-user or cross-machine aggregation service
- No standalone web dashboard owned by this repo
- No team login/account system
- No non-Linux browser sync abstraction until there is a platform target that can exercise it
