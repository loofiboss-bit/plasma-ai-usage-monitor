# User guide

AI Usage Monitor is a KDE Plasma 6 widget for AI provider usage, tool quotas,
and optional local coding-session history. Start with one source; you can add
budgets, alerts and integrations later.

**New here?** [Install both required parts](installation.md), add **AI Usage
Monitor** through Plasma's **Add Widgets** menu, then follow
[first setup](getting-started.md). Fedora users only need the complete COPR
package. The KDE Store download requires the matching native plugin.

## Find the right task

| I want to… | Read |
| --- | --- |
| Install, upgrade, or fix a version mismatch | [Installation](installation.md) |
| Monitor a tool without setting up an API provider | [First setup](getting-started.md) and [tool sources](subscriptions.md) |
| Search Codex or Claude Code tokens by project, model, or date | [Coding sessions](coding-sessions.md#try-it-with-one-tool) |
| Connect an API provider | [Providers and permissions](providers.md) |
| Understand missing values or estimates | [Understanding the data](understanding-data.md) |
| Repair a source or collect a safe support report | [Troubleshooting](troubleshooting.md) |

## Set up the widget

1. [Install or update](installation.md)
2. [Reach your first successful source](getting-started.md)
3. [Understand the data](understanding-data.md)

## Add data sources

- [API providers](providers.md)
- [Subscription tools and Browser Sync Labs](subscriptions.md)
- [Local coding sessions and projects](coding-sessions.md)

## Use the data

- [Budget Control and runway guardrails](runway-guardrails.md)
- [History, exports, Prometheus, and webhooks](history-and-integrations.md)
- [Troubleshooting](troubleshooting.md)
- [Privacy and security](privacy-and-security.md)

## Before asking for help

Open **Settings → Diagnostics** and check the frontend and native-plugin versions, install layers, database, source readiness, KWallet, and catalogs. The native **Copy support report** action omits endpoint hosts, query strings, account IDs, project IDs, credentials, and KWallet values.
