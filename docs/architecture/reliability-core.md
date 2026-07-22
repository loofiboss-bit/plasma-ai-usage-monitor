# Runtime and data architecture

This document describes the current runtime contracts. It is the technical boundary for provider adapters, source readiness, history, secrets, and distribution.

## Refresh lifecycle

ProviderBackend::requestRefresh() is the public refresh entry point. It owns single-flight coalescing, refresh reasons, cancellation, generation checks, timestamps, typed errors, retry metadata, freshness, and terminal state. Adapters implement refreshImpl() and response parsing.

Scheduled and popup refreshes coalesce. A manual request may supersede one older request. A reply from an invalid generation cannot change state or write history.

The scheduler consumes typed error kind, retryability, Retry-After, and freshness. It does not parse localized UI text. Authentication, permission, configuration, and schema errors wait for user action. A failed secondary endpoint can produce a degraded state while preserving valid primary metrics.

Scheduled provider traffic is read-only. Explicit inference tests are separate actions and never run on the background schedule.

## Provider Metric Contract v2

Each metric carries its kind, nullable value, unit, source, quality, scope, time window, observation semantics, currency, reset metadata, and optional model or project scope.

Unavailable values stay null. Connectivity cannot create zero usage. Published limits cannot become live remaining quota. Mixed currencies stay separate.

ProviderManager and Catalog v5 descriptors own stable identity, adapter type, authentication slots, endpoints, models, capability claims, source expectations, and safe refresh policy. QML reads this catalog instead of maintaining a second provider truth table.

The generated [provider capability matrix](../provider-capabilities.md) is checked against the shipped catalog during validation.

## Source readiness contract

`SourceReadinessModel` is the single QML-facing readiness authority for all 18
providers and 7 local tools. Each source appears once with a stable ID, source
kind, monitoring level, required credential slots, local installation state,
enabled state, last verified time, safe verification capability, and a redacted
next action.

The stable states are `disabled`, `unavailable_locally`,
`needs_configuration`, `ready_to_verify`, `verifying`,
`connected_connectivity_only`, `reporting_estimate`, `reporting_actual`,
`degraded`, and `failed`. Connectivity never implies useful usage or spend.
Actual and estimated metrics are classified from Metric Contract v2 sources.
Provider failures are classified from `ProviderErrorKind`; localized error text
is not parsed. Local sync diagnostics use stable diagnostic codes and never
expose cookies or credential values.

Setup ranking is deterministic: detected local tools come first, then actual
usage/spend and gateway sources, then balance/connectivity sources, and finally
local tools that are not detected. Onboarding, Settings, Overview, and
Diagnostics must consume this model instead of independently deriving status.

`OverviewState` is the shared QML projection for Overview, the panel, and the
popup footer. Its summary keeps useful data, connectivity-only results, and
attention states separate. Compact quota and cost modes consume only available
typed metrics; unavailable scalar defaults cannot create healthy status or zero
usage.

## Diagnostics and recovery

`AppInfo` owns native installation, version, Plasma, distribution, and read-only
SQLite health inspection. It resolves the loaded plugin library, compares
user-local and system plasmoid metadata, and reports one copyable repair command;
it never removes an installation layer. The runtime persists only stable source
IDs, readiness states, error kinds, actions, and a verified/not-verified flag for
the Diagnostics KCM.

Support reports are built from allowlisted fields. Endpoint details, account or
project identifiers, credentials, cookies, webhook URLs, wallet contents, and
free-form backend errors are excluded. The frontend-only bootstrap produces a
separate minimal report for missing or mismatched native plugins.

## SQLite schema v4

History stays local and uses WAL mode. Schema v4 stores normalized observations and permits null values. Migration from v3 is transactional, idempotent, and backed up before changes.

The Analyst output/input query retains the schema-v4 compatibility projection
and returns only days with positive total input. It does not synthesize ratios
for missing input and does not interpret the ratio as prompt quality.

Calendar totals include only compatible interval-total observations. Gauges, cumulative counters, and rolling windows are not relabeled as calendar totals. Queries preserve ISO currency and source quality.

Large exports use worker instances, forward-only queries, and atomic output files so memory use does not grow with history size and partial files do not replace complete exports.

## Secret and browser boundaries

SecretsManager opens KWallet, caches secret availability, and invalidates individual entries after writes or removals. QML receives redacted availability and operation status, not raw secret lists.

BrowserSyncService owns profile discovery, cookie extraction, authenticated requests, timeouts, and circuit breaking. Cookie headers do not cross into QML or enter diagnostics, logs, history, or exports. Browser Sync remains off by default.

## Local integrations

The Prometheus server binds to loopback. Scheduled JSON and CSV export writes to a user-selected local directory. Slack and Discord webhooks are explicit outbound integrations and use KWallet-stored URLs plus alert cooldowns.

Configuration export uses schema v2 and excludes every secret-bearing field.

## Distribution contract

COPR is the supported Fedora package and contains both the Plasma package and compiled QML plugin. Source installation builds the same parts.

The KDE Store plasmoid contains the frontend, catalogs, icons, and metadata. It requires a matching compiled plugin. Flatpak remains unsupported because the removed scaffold did not package or exercise the native plugin.

VERSION is canonical. Release validation checks package metadata, catalogs, RPM metadata, AppStream, the QML import, package payload, reproducible artifacts, checksums, and an SPDX source SBOM.

## Deferred architecture

The plugin remains in-process and local-first. A hosted backend, account system, cross-machine database, or standalone web dashboard is outside the current product boundary.
