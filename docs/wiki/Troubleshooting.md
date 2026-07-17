# Troubleshooting

Open **Settings → Diagnostics** first. Check the widget version, loaded plugin path, KWallet, provider health, catalogs, and Browser Sync readiness.

Common fixes:

- log out and back in after the first install
- reinstall the COPR package when the compiled plugin is missing
- check for a user-local package that overrides the system package
- unlock KWallet before re-entering provider keys
- remove a custom base URL before debugging authentication
- remember that connected providers can legitimately have unknown usage

For a bug report, include the Plasma version, distribution, installed package version, reproduction steps, redacted support report, and a short relevant journal excerpt.

Do not post provider keys, cookies, tokens, or unredacted URLs.

Commands and symptom-specific steps are in the [troubleshooting guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/troubleshooting.md).
