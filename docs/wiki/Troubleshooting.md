# Troubleshooting

If the widget shows a native-plugin recovery screen, use its copyable COPR command or source-install link first. Restart Plasma after repairing the installation.

Otherwise open **Settings → Diagnostics**. Check the frontend and plugin versions, loaded plugin path, install layers, database, source readiness, KWallet, catalogs, and Browser Sync readiness.

Common fixes:

- log out and back in after the first install
- install the native plugin before using the frontend-only KDE Store package
- update the frontend and native plugin together when their versions differ
- check for a user-local package that overrides the system package
- unlock KWallet before re-entering provider keys
- remove a custom base URL before debugging authentication
- remember that connected providers can legitimately have unknown usage

For a bug report, include the Plasma version, distribution, installed package version, reproduction steps, native redacted support report, and a short relevant journal excerpt.

Do not post provider keys, cookies, tokens, or unredacted URLs.

Commands and symptom-specific steps are in the [troubleshooting guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/troubleshooting.md).
