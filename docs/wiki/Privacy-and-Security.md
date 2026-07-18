# Privacy and security

AI Usage Monitor has no telemetry, hosted account, cloud database, or cloud sync.

- API keys, tokens, and webhook URLs stay in KWallet.
- History stays in a local SQLite database.
- Configuration exports exclude secrets.
- Scheduled provider requests are read-only.
- The Prometheus endpoint binds to 127.0.0.1.
- Browser Sync Labs is off by default and keeps cookies outside QML, logs, diagnostics, history, and exports.

Webhooks send alert content to Slack or Discord by design. Treat the configured webhook destination as part of your data boundary.

Native Diagnostics redacts endpoint and account details, credentials, and KWallet values from copied support reports. The dependency-recovery screen also omits error details and paths. Review either report before posting it publicly.

Read the [full privacy guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/privacy-and-security.md).

Report vulnerabilities through a private [GitHub Security Advisory](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/security/advisories/new), not a public issue.
