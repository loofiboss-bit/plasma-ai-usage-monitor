# Troubleshooting

Check the recovery screen or native Diagnostics before deleting configuration or reinstalling.

## Widget is missing after installation

Log out and back in. If you need to reload the current session:

~~~bash
./scripts/reload_plasma.sh
~~~

Confirm that Plasma sees the package:

~~~bash
kpackagetool6 --type Plasma/Applet --show com.github.loofi.aiusagemonitor
~~~

## Widget shows an old version

A user-local package can override the system package:

~~~bash
./scripts/show_installed_versions.sh
~~~

Run the smoke check from a repository checkout:

~~~bash
./scripts/smoke_test_plasmoid.sh
~~~

Remove only the stale user-local package if Diagnostics confirms that it shadows the current RPM:

~~~bash
kpackagetool6 --type Plasma/Applet --remove com.github.loofi.aiusagemonitor
~~~

Log out and back in after changing install layers.

## QML plugin cannot be loaded

The compiled plugin and Plasma package must have the same version. A missing or mismatched plugin opens an in-widget recovery screen with the detected frontend and plugin versions.

On Fedora, copy the command from that screen or reinstall the COPR package:

~~~bash
sudo dnf reinstall plasma-ai-usage-monitor
~~~

Restart Plasma or log out and back in after repair. The QML engine can retain a failed import for the current session.

Open Diagnostics and inspect **Version**, **Loaded plugin**, and **Install layers**. A missing library error in the journal usually means a partial source install or a Store frontend installed without the native plugin.

## Frontend and native plugin versions differ

Update the KDE Store frontend and the Fedora or source-installed plugin together. Diagnostics warns when a user-local frontend shadows the system package and offers a copyable repair command when it can identify one.

## KWallet does not open

1. Open **System Settings → KDE Wallet** and confirm that the wallet subsystem is enabled.
2. Unlock the wallet.
3. Reopen widget settings.
4. Check **Diagnostics → Wallet & Secrets**.

There is no plaintext fallback for provider keys.

## Provider returns 401 or 403

- Re-enter the key and apply settings.
- Remove a custom base URL and retry.
- Confirm the key belongs to the selected provider.
- For OpenAI account usage, use an Admin API key.
- For Azure or AWS, verify the resource, region, deployment, API version, and account permission.

Do not post keys or unredacted request URLs in an issue.

## Provider is connected but usage is unknown

This can be correct. Many provider APIs expose model discovery but no account usage or spend. Check the monitoring level and source labels on the card or in the [capability matrix](../provider-capabilities.md).

## History is empty

- Enable history in Settings.
- Wait for an enabled source to complete a successful refresh.
- Confirm the source returned a compatible metric.
- Check the configured retention period.
- Inspect database size in History settings.

Connectivity-only responses do not create fake usage rows.

## Budget policy or runway forecast is unavailable

Open Source Detail or Analyst and read the unavailable reason, samples,
coverage, period end, and method.

- Wait for at least four compatible quota observations spanning 15 minutes.
- Confirm the newest quota observation is no more than 15 minutes old and the
  provider still reports the same reset, limit, unit, and window.
- For policy pacing, wait for the minimum completed compatible UTC-day samples
  and coverage shown by the result.
- Open **Settings → Budget Control**, validate the limit, ISO currency, period,
  saved time zone, value class and catalog-supported scope, then Apply.
- Keep actual and estimated values separate and use the same ISO currency as
  the budget; the widget never converts currencies.
- A provider reset must be authenticated, stable and catalog-declared. A
  removed scope or scoped-over-aggregate mismatch remains unavailable.

An unavailable result is not zero and does not mean a budget or quota is safe.

## Guardrail notification did not arrive

- Enable global alerts and notifications on the policy under **Settings →
  Budget Control**.
- Confirm notifications are enabled for that provider.
- Check Do Not Disturb and cooldown settings.
- Check whether the policy is snoozed until its next period.

An unchanged warning, critical or exceeded state is intentionally suppressed
after refresh and restart. Risk becoming unavailable is not a recovery, and
unavailable becoming safe is not a recovery. DND/cooldown/delivery failures
retain pending or suppressed evidence for retry.

## Browser Sync Labs fails

- Sign in again in the selected browser.
- Choose the exact browser profile.
- Open the service once so its cookie database exists.
- Confirm KWallet or libsecret is available for Chromium-family browsers.
- Check Browser Sync readiness in Diagnostics.

The service may have changed its undocumented endpoint. Local subscription tracking continues without Browser Sync.

## Antigravity quota is unavailable

- Update to a build that supports the Antigravity 2.x `resources/bin/language_server` layout.
- Keep the Antigravity desktop app open and signed in, then use **Refresh / Test connection**.
- Restart Antigravity after an app update so its localhost daemon and bundled certificate are current.
- Check that Diagnostics reports the tool as installed and the source as **Antigravity local**.

The Antigravity card reports Antigravity quota only. It does not include the separate usage limits shown by `gemini.google.com`.

## Collect logs

Follow Plasma logs while reproducing the problem:

~~~bash
journalctl --user -f | grep -i -E 'plasma|aiusage|qml'
~~~

Also collect or copy **Diagnostics → Copy version check**:

~~~bash
plasmashell --version
rpm -q plasma-ai-usage-monitor
~~~

Attach the native **Copy support report** output and the smallest relevant log excerpt to the GitHub issue. The report includes install, database, and source-readiness state without credentials or identifying endpoint details.
