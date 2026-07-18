# Privacy and security

AI Usage Monitor is local-first. It has no telemetry, account service, hosted database, or cloud sync.

## Data stored locally

| Data | Location |
| --- | --- |
| Provider keys and webhook URLs | KWallet folder used by AI Usage Monitor |
| Usage history | Local SQLite database under the user's data directory |
| Widget preferences | Plasma configuration |
| Export files | Directory selected by the user |

Configuration exports exclude keys, tokens, cookies, personal access tokens, and webhook URLs.

## Network traffic

Enabled providers connect directly to their configured API endpoints. Scheduled calls are read-only and listed in the generated capability matrix. Manual inference tests are explicit and may consume quota or money.

Remote custom base URLs must use HTTPS. Plain HTTP is accepted only for loopback development endpoints.

The optional Prometheus server binds to 127.0.0.1. Slack and Discord webhooks send alert content to the configured webhook service.

## KWallet

The widget does not write provider keys to Plasma config files. If KWallet is unavailable, secret-backed providers cannot load their credentials. There is no insecure fallback.

## Browser Sync Labs

Browser Sync reads the selected local profile and uses the existing authenticated session for a direct request to the service. Cookie data does not enter QML, diagnostics, logs, history, or exports.

Temporary browser database copies use owner-only permissions. Browser Sync is disabled by default because it depends on undocumented service behavior.

## Support reports

Native Diagnostics builds the support report in the compiled plugin. It redacts endpoint hosts, query strings, account IDs, project IDs, credentials, and KWallet values. The dependency-recovery screen has a smaller redacted bootstrap report that omits error details and paths. Review either report before posting it publicly.

## Removing local data

Removing the RPM leaves user data in place. Delete history from the widget's History settings when possible. Remove stored keys through provider settings or KWallet Manager.

Do not delete the whole wallet to remove this widget's entries.

## Report a vulnerability

Use a private [GitHub Security Advisory](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/security/advisories/new). Do not open a public issue for a suspected vulnerability.
