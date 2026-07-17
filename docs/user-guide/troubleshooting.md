# Troubleshooting

Check the installed versions and Diagnostics before deleting configuration or reinstalling.

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

The compiled plugin and Plasma package must have the same version. On Fedora, reinstall the COPR package:

~~~bash
sudo dnf reinstall plasma-ai-usage-monitor
~~~

Open Diagnostics and inspect **Loaded plugin**. A missing library error in the journal usually means a partial source or Store-only install.

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

## Browser Sync Labs fails

- Sign in again in the selected browser.
- Choose the exact browser profile.
- Open the service once so its cookie database exists.
- Confirm KWallet or libsecret is available for Chromium-family browsers.
- Check Browser Sync readiness in Diagnostics.

The service may have changed its undocumented endpoint. Local subscription tracking continues without Browser Sync.

## Collect logs

Follow Plasma logs while reproducing the problem:

~~~bash
journalctl --user -f | grep -i -E 'plasma|aiusage|qml'
~~~

Also collect:

~~~bash
plasmashell --version
rpm -q plasma-ai-usage-monitor
~~~

Attach the redacted support report from Diagnostics and the smallest relevant log excerpt to the GitHub issue.
