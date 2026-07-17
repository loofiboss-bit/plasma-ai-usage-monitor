# Runtime and data architecture

This document describes the current v13 contracts. It is the technical boundary for provider adapters, history, secrets, and distribution.

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

## SQLite schema v4

History stays local and uses WAL mode. Schema v4 stores normalized observations and permits null values. Migration from v3 is transactional, idempotent, and backed up before changes.

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
