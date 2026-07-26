<!-- Generated from docs/user-guide/privacy-and-security.md by scripts/generate_wiki_docs.py; do not edit. -->
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

Anthropic's standard and Admin API keys are separate KWallet entries. The
widget never copies one into the other. Admin report rows may include model and
workspace identifiers; those scopes stay in local schema-v4 history and are
excluded from diagnostics, support reports, configuration exports, and alert
payloads.

## Network traffic

Enabled providers connect directly to their configured API endpoints. Scheduled calls are read-only and listed in the generated capability matrix. Manual inference tests are explicit and may consume quota or money.

Anthropic organization usage and cost requests require the optional Admin key,
use read-only report endpoints, follow bounded pagination, and never send a
message or inference request.

Remote custom base URLs must use HTTPS. Plain HTTP is accepted only for loopback development endpoints.

The optional Prometheus server binds to 127.0.0.1. Slack and Discord webhooks send alert content to the configured webhook service. Daily alerts contain the stable source name, severity, reason, recommended action, freshness, and compatible quota context. They do not include endpoint URLs, account or project identifiers, credentials, cookies, unrestricted backend errors, or wallet contents.

## KWallet

The widget does not write provider keys to Plasma config files. If KWallet is unavailable, secret-backed providers cannot load their credentials. There is no insecure fallback.

## Browser Sync Labs

Browser Sync reads the selected local profile and uses the existing authenticated session for a direct request to the service. Cookie data does not enter QML, diagnostics, logs, history, or exports.

Temporary browser database copies use owner-only permissions. Browser Sync is disabled by default because it depends on undocumented service behavior.

## Antigravity local monitoring

Antigravity monitoring connects only to a language-server process owned by the current user and listening on loopback. The widget validates the executable layout and pins the localhost certificate shipped with the detected Antigravity installation. Its CSRF value remains in memory and is never logged, persisted, diagnosed, or exported.

The monitor reads only account status and quota-summary RPCs. It does not read Antigravity prompts, conversations, OAuth tokens, logs, or the separate `gemini.google.com` session.

## Support reports

Native Diagnostics builds the support report in the compiled plugin. It includes only allowlisted source IDs, readiness states, typed error kinds, next-action keys, and whether verification has succeeded. It excludes endpoint hosts, query strings, account IDs, project IDs, credentials, cookies, webhook URLs, free-form backend errors, and KWallet values. The dependency-recovery screen has a smaller redacted bootstrap report that omits error details and paths. Review either report before posting it publicly.

## Removing local data

Removing the RPM leaves user data in place. Delete history from the widget's History settings when possible. Remove stored keys through provider settings or KWallet Manager.

Do not delete the whole wallet to remove this widget's entries.

## Report a vulnerability

Use a private [GitHub Security Advisory](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/security/advisories/new). Do not open a public issue for a suspected vulnerability.
