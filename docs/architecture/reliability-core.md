# Reliability Core architecture

This document defines the v12 runtime and distribution contracts. They are
release requirements, not implementation suggestions.

## Refresh lifecycle

`ProviderBackend::requestRefresh()` is the only public provider refresh entry
point. It owns single-flight/coalescing, refresh reasons, cancellation,
generation validation, timestamps, typed errors, retry metadata, freshness and
terminal state. Adapters implement only `refreshImpl()` and parsing. Scheduled
or popup refreshes coalesce; a manual request may supersede one older request.
Replies from an invalid generation cannot mutate state or history.

The scheduler consumes `ProviderErrorKind`, `retryable`, `retryAfter`, and
`Freshness`. It never parses localized error text. Backoff is capped and uses
deterministic jitter. Authentication, permission, configuration and schema
errors require user action. A successful primary endpoint with a failed
secondary endpoint produces `Degraded`, preserving the valid metrics.

## Observation schema v3

SQLite remains local and uses WAL mode. Schema v3 stores normalized
observations with UTC interval boundaries, metric kind/unit, aggregation
semantic, source, currency, quality, model/project scope and correlation ID.
Calendar totals include only compatible `interval_total` observations. Gauges,
cumulative counters and rolling windows are never relabeled as calendar-day
spend. Migration is transactional and preserves a pre-v12 backup and the legacy
table for rollback.

Queries and exports run on worker instances. Large exports iterate forward-only
SQL queries and write atomically, one row at a time, so memory use does not grow
with history size. Mixed ISO currencies remain separate throughout storage,
queries, UI, alerts and integrations; v12 performs no currency conversion.

## Catalog v4

`providers-v4.json` owns identity, capabilities, authentication shape,
endpoints, models, price units, metric/source expectations, lifecycle dates and
review provenance. The C++ catalog model exposes that truth to QML. Backend
object binding remains code, but QML must not duplicate capability facts.
Lifecycle aliases are date-driven and migration is visible to the user.

## Secret and browser boundaries

KWallet values are cached after wallet open and invalidated per entry on
write/remove. There is no periodic secret polling. QML receives only redacted
availability and operation status. `BrowserSyncService` owns cookie extraction,
authenticated requests, timeout and circuit breaking; cookie headers never
cross the QML boundary or enter diagnostics, logs, history or exports.

## Distribution contract

COPR is the supported Fedora 44 package and contains both the plasmoid and the
compiled QML plugin. A source build provides the same capability. The KDE Store
`.plasmoid` contains only the frontend and requires a matching compiled plugin.
Flatpak is unsupported because the removed scaffold did not package or exercise
the native plugin. Release artifacts must come from an exact signed/tagged
source commit, include checksums and an SPDX SBOM, and report the same version as
`VERSION`.

An out-of-process D-Bus service is deferred to v13; v12 keeps the plugin
in-process and local-first, with no telemetry, account or hosted backend.
