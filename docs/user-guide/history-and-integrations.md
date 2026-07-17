# History and integrations

History is local and optional. Enable only the outputs you plan to use.

## History

Open **Settings → History** to enable recording and choose a retention period from 7 to 365 days. The default is 90 days.

The popup provides:

- a detail view for one provider or subscription tool
- comparison charts across compatible sources
- cost, token, request, and rate-limit metrics
- an Analyst view with activity, change, volatility, anomalies, and top drivers

Only compatible units and currencies should be compared. Unknown values remain absent rather than becoming zero.

The database is stored at:

~~~text
~/.local/share/plasma-ai-usage-monitor/usage_history.db
~~~

Use the History settings page to inspect its size or prune rows older than the retention period.

## JSON and CSV export

The popup can export the selected view. History settings can also write JSON or CSV on a schedule.

Choose a directory you own. Scheduled export writes atomically so a partial run does not replace the last complete file.

Exports contain usage observations and source metadata. They do not contain API keys, browser cookies, personal access tokens, or webhook URLs.

## Configuration backup

Open **Settings → Diagnostics** to export or import non-secret settings. The schema v2 file covers provider toggles, models, refresh settings, budgets, history, alerts, and subscription preferences.

Secrets remain in KWallet and must be configured separately on a new computer.

## Prometheus

Enable the metrics endpoint under History and choose an unused port. The server binds to 127.0.0.1 only.

Example check for the default port:

~~~bash
curl http://127.0.0.1:9464/metrics
~~~

Use a local Prometheus instance or an explicitly configured local forwarder. The widget does not expose the endpoint on other network interfaces.

## Slack and Discord webhooks

Webhooks use the same alert pipeline as KDE notifications.

1. Open **Settings → Alerts**.
2. Enable alerts and the required event types.
3. Enable Slack or Discord.
4. Paste the incoming webhook URL.
5. Set a webhook cooldown.

Webhook URLs are stored in KWallet. Alerts can contain provider names, status, and usage or budget context, so treat the destination as part of your data boundary.

## Alert tuning

Start with provider disconnect, reconnect, and API errors. Add budget warnings only for providers with compatible spend data. Set Do Not Disturb hours and a cooldown to avoid repeated notifications during a provider outage.
