# Security policy

## Report a vulnerability

Use a private [GitHub Security Advisory](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/security/advisories/new). If GitHub advisories are unavailable, email **loofi@github.com**.

Do not open a public issue for a suspected vulnerability.

Include:

- affected version
- clear reproduction steps
- expected impact
- logs or proof of concept with secrets removed
- a suggested fix, if you have one

The project aims to acknowledge reports within 48 hours. Fix timing depends on severity, reproducibility, and upstream dependencies.

## Supported releases

| Release line | Security updates |
| --- | --- |
| 13.x | Supported |
| 12.x and older | Upgrade to the current 13.x release |

## In scope

- KWallet storage and secret lifetime
- provider authentication and network requests
- custom base URL validation
- Browser Sync cookie handling
- local SQLite history and exports
- configuration import and export
- loopback Prometheus server
- Slack and Discord webhook handling
- native plugin packaging and load paths

Reports about provider-side services, browser vulnerabilities, or KDE components should go to the affected upstream project unless this widget creates the exposure.

## Security boundaries

### Secrets

Provider keys, personal access tokens, and webhook URLs are stored in KWallet. The widget has no plaintext fallback. Non-secret Plasma configuration and configuration exports contain only redacted availability or ordinary settings.

### Provider traffic

Default provider endpoints use HTTPS. Remote custom endpoints must use HTTPS; plain HTTP is limited to loopback development addresses. Scheduled provider refreshes are read-only. Explicit manual inference tests may consume quota or money.

Requests use timeouts and the refresh scheduler respects retry guidance. Authentication, permission, configuration, and schema failures require user action instead of unlimited retries.

### Browser Sync Labs

Browser Sync is disabled by default. Cookie databases are read through a native boundary, and temporary copies use owner-only permissions. Cookie values do not enter QML, logs, diagnostics, history, or exports.

### Local data

Usage history stays in a local SQLite database. The widget has no telemetry, hosted backend, or cloud sync. The optional Prometheus endpoint binds to 127.0.0.1.

Webhook alerts leave the computer by design and may contain provider status or budget context. The user chooses and controls the destination.

### Diagnostics

Copied support reports redact endpoint hosts, query strings, account IDs, project IDs, credentials, and KWallet values. Users should still review reports before posting them publicly.

## Build and release controls

Release checks cover version consistency, provider and subscription catalogs, QML imports, package payload, non-invasive monitoring, deterministic provider contracts, AppStream metadata, RPM policy, checksums, and an SPDX source SBOM. Public packages are verified by readback after release.
