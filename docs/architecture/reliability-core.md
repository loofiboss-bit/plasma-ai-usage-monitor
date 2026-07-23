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

## Daily state contract

`DailyStateModel` is the typed Phase 1 projection for daily surfaces. It observes
`SourceReadinessModel`, provider Metric Contract v2 rows, subscription-tool quota
windows, budgets, thresholds, and freshness. Only enabled sources are rows, and
each catalog stable ID can occur once. `NativeMonitor.dailyState` exposes the
same model instance to the panel, Overview, notifications, and later analytical
surfaces; those consumers must not build independent provider or tool loops.

Every row uses non-localized keys. Identity and state are `stableId`,
`displayName`, `sourceKind`, `monitoringLevel`, `readinessState`, `qualityClass`,
`freshnessState`, `lastSuccess`, `lastAttempt`, `lastErrorKind`, and
`nextActionKey`. Quality classes are `actual`, `estimated`, `balance`,
`connectivity_only`, and `unavailable`. Freshness is `fresh`, `aging`, `stale`,
or `never`.

Metric state is exposed through `hasUsefulData`, `hasActualData`,
`hasEstimatedData`, `hasBalance`, `connectivityOnly`, `primaryMetricKind`,
`primaryMetricAvailable`, `primaryMetricValue`, and `primaryMetricUnit`.
Optional quota, reset, cost, and budget scalars always have a matching
availability role. An available numeric zero remains zero. An unavailable value
is an invalid `QVariant`; callers must check its availability role and must not
coerce it to zero.

`quotaWindows` contains every compatible normalized quota window for the source.
Each row carries `kind`, `window`, `percentUsed`, `percentRemaining`,
`sourceClass`, `sourceKey`, and `resetAt` only when the source contract provides
one. `sourceClass` is `actual`, `local_estimate`, `configured_limit`, or
`unknown`. Notifications may group threshold-crossing windows, but must keep
these classes visible and must never treat an `unknown` window as exhausted.

The aggregate `summary` contains enabled, useful, actual, estimated, balance,
connectivity-only, attention, and stale counts; highest severity and the most
urgent source; lowest remaining quota; nearest reset; actual spend, estimated
spend, and fixed subscription fees as separate ISO-currency maps; and the most
recent successful aggregate completion time. Mixed currencies are never summed
into a scalar. A row with multiple cost currencies reports `currency` as
`MIXED`, leaves `costAvailable` false, and remains represented in the aggregate
currency maps.

Attention order is deterministic: actionable failure, exhausted quota,
critical quota, critical budget, stale useful data, warning threshold, ready to
verify, healthy connectivity-only, then normal reporting. Ties use lower
remaining quota, earlier reset, and finally immutable catalog order. Severity
keys are `critical`, `warning`, `info`, and `none`; reason keys remain stable and
non-localized.

Phase 2 daily surfaces consume this model through `DailyOverviewState`. The
normal Overview and compact representation do not maintain independent provider
or tool summary loops. Overview renders one top recovery action, then actual
quota/reset facts, separated spend categories, and compact source groups.
Connectivity-only sources stay collapsed by default.

The live-quota aggregates accept only provider-reported or synchronized quota
windows. A published documentation limit and a locally configured activity
limit remain available on their source card as estimates, but they cannot drive
the Overview live-quota section or the `lowest-quota` and `next-reset` panel
modes. Compact-mode compatibility maps `count` to `active-sources`, `critical`
to `attention`, and `cost` to `actual-spend`; legacy `dailycost` and `requests`
remain readable only when the Daily State Model exposes a compatible available
metric.

Phase 5 notifications observe Daily State source changes instead of raw provider
or tool warning signals. They use the row severity, reason, action, freshness,
and normalized quota windows; one source produces at most one grouped quota
notification per cooldown. Stale snapshots suppress cached quota changes.
Recovery is emitted only after a real failed readiness state, and webhook text
is built from the same compact, source-explicit, redacted payload as the desktop
notification.

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

Phase 3 history discovery uses the union of configured provider descriptors,
configured subscription tools, retained provider observations, and retained tool
snapshots. The UI merges these by `sourceKind` plus database identity, so
disabling or removing a source does not hide its retained rows. A source is
shown as enabled, disabled, or history-only. Metric selectors are derived from
compatible stored values; a capability claim alone cannot create a metric tab.

`UsageDatabase::requestHistoryCatalog()` and
`UsageDatabase::requestHistorySeries()` are the asynchronous QML boundary.
Series results include source identity, kind, metric, unit, currency,
observation semantics, quality classes, sample and plotted-point counts,
observation bounds, bucket size, gaps, freshness, and history-only state.
Requests are superseding: a completed older generation cannot replace a newer
selection.

Unavailable observations are omitted while an available numeric zero remains a
point. Missing buckets become explicit chart gaps. Source, semantic, scope,
window, or currency changes start a separate series. Multi-source comparisons
fail closed when units, semantics, or currencies differ. Rolling tool quota is
a gauge and is never summed or relabeled as calendar-day usage.

The Analyst output/input query retains the schema-v4 compatibility projection
and returns only days with positive total input. It does not synthesize ratios
for missing input and does not interpret the ratio as prompt quality.

## Analyst snapshot contract

`UsageDatabase::requestAnalyst()` is the only QML-facing Analyst query. It runs
on a worker-owned database connection and returns one internally consistent
snapshot for the exact requested UTC range. Requests are superseding, so an
older completion cannot replace the current view or report request. Opening the
Analyst view does not run synchronous SQL on the UI thread.

The snapshot carries its request range, generation time, currency status,
coverage, actual and estimated sample counts, daily spend, activity,
output/input ratio, compatible drivers, anomaly candidates, method metadata,
and an explicit availability result for every derived KPI. Unavailable KPI
values remain invalid variants and have a stable reason key plus the observed
and required sample counts.

Only `interval_total` cost and explicitly identified local estimates contribute
to spend analysis. A typed daily cost window is persisted as an interval total;
cumulative, all-time, current, probe-only, unknown, and connectivity values do
not become daily spend. Actual and estimated costs remain separate. Mixed
currencies pause cost-derived results unless one currency is explicitly
selected, while compatible token, request, and local-tool activity remains
available.

Average daily spend requires three recorded days. Volatility and anomaly
candidates require seven. Week-over-week change requires two complete
seven-day windows and a non-zero previous window. Anomaly candidates use the
period mean and population standard deviation, require two standard deviations
above the mean, and also require an increase of at least one currency unit or
50 percent of the baseline. Output/input ratio requires three compatible days
with positive input and remains descriptive only.

Seven-day and 30-day reports each request their own snapshot. Reports include
coverage, quality counts, currency status, unavailable explanations, and method
notes. They never reuse a different period's UI state or include endpoints,
credentials, cookies, account identifiers, or unrestricted backend errors.
Endpoint and installation diagnostics remain exclusively in Diagnostics.

## Daily UI test boundaries

Phase 6 keeps daily behavior independently testable without changing its public
QML imports or backend contracts. `AnalystState` owns Analyst requests, result
supersession, availability formatting, chart-series projection, and report
generation; `AnalystTab` remains the visual surface. `HistoryController` owns
catalog and series requests, selection normalization, result supersession,
series decoration, coverage text, and export projection; `HistoryView` remains
the navigation and chart surface.

`CompactMetricState` selects the configured panel metric from a supplied Daily
State summary, so compact behavior can be fixture-tested without loading the
plasmoid. `MetricAvailabilityFormatter` is the shared boundary for unavailable
placeholders, reason text, dates, percentages, currencies, and KPI fallback
objects. `DailyOverviewState` delegates its compatibility compact API to the
same selector. Available numeric zero remains distinct from unavailable state
through every extracted object.

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
