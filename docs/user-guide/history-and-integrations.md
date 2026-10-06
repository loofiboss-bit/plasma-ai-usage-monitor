# History and integrations

History is local and optional. Enable only the outputs you plan to use.

## History

Open **Settings → History** to enable recording and choose a retention period from 7 to 365 days. The default is 90 days.

Runway calculations use the same local observations. Enabling forecast
notifications also enables the local database for the minimum observation and
transition evidence even when the general History switch is off.

The popup provides:

- a detail view for one provider or subscription tool
- comparison charts across compatible sources
- cost, token, request, and rate-limit metrics
- 24-hour, 7-day, 30-day, and 90-day ranges
- an Analyst view with activity, change, volatility, anomalies, and top drivers

Sources with retained data remain selectable after they are disabled. A source
that is no longer part of the current catalog is labeled **History only**.
Metric choices appear only when compatible stored observations exist.

Only compatible units, measurement semantics, and currencies can be compared.
The comparison explains why it was rejected instead of combining incompatible
values. Unknown values remain absent rather than becoming zero, and missing
time buckets appear as chart gaps. Coverage text below the chart reports stored
samples, plotted points, gaps, stale data, and history-only state.

Analyst loads one background snapshot for its exact 30-day period. It starts
with coverage and then shows only sections supported by compatible history:
spend, activity, top drivers, anomaly candidates, and a written summary.
Insufficient history is explained as unavailable instead of appearing as
`0.0`. Actual and estimated spend stay labeled separately. Mixed currencies
pause spend analysis without hiding compatible token, request, or local-tool
activity.

**Output / Input Ratio** is available only after at least three days with
compatible snapshots and positive input tokens. It is a neutral relationship
between two reported quantities, not a quality, productivity, prompt-clarity,
or efficiency score.

The 7-day and 30-day copy actions each build a report from their own period.
Reports include coverage, currency status, actual and estimated sample counts,
unavailable explanations, and the analysis method. Provider and installation
diagnostics remain under **Settings → Diagnostics**.

History and Analyst requests are asynchronous. Changing source, metric, range,
or report period supersedes the older request; a late result cannot replace the
newer selection.

The database is stored at:

~~~text
~/.local/share/plasma-ai-usage-monitor/usage_history.db
~~~

Use **Settings → History** to inspect database and WAL-file size separately
or prune rows older than the retention period. These operations run in the
background. The page reports how many rows were removed and refreshes storage
statistics when pruning finishes. A database-open or migration failure appears
as an error rather than an empty history or `0 B`.

## JSON and CSV export

The popup's **Export file** action writes the selected series as JSON or CSV.
Copying CSV to the clipboard is available as a separate secondary action.
History settings can also write JSON or CSV on a schedule.

Selected-series JSON follows schema v6 and contains the metric, selected time
bounds, bucket size, series metadata, quality classes, coverage flags, and
timestamped points. CSV uses the same selection and explicit columns. Both
formats preserve unit, currency, period, quality, gaps, and unavailable `null`
separately from available numeric zero. They use an explicit field allowlist;
raw scope identities, internal database names, and other internal fields are
omitted.

Choose a directory you own. **Settings → History → Write Export Files** reports
progress, the written paths, complete or partial success, and failures. Repeated
clicks cannot overlap an active export. **Last scheduled export** shows the last
completed scheduled result. Each file is written atomically; a partially
successful batch reports the files actually written.

Full-history JSON uses export schema v7, independently of SQLite schema v8.
It includes observations, metadata, legacy guardrail transitions, policy
transitions, and per-channel delivery status. CSV writes policy/channel evidence
in a separate `ai-usage-policy-events-*.csv` alongside the other history files.
The popup's selected-series JSON retains schema v6 and does not represent a
full-history backup.

Full-history exports use explicit field lists and omit API keys, cookies,
personal access tokens, webhook URLs, raw scope identities, policy identifiers. Values and source names can still reveal
usage patterns, so review any export before sharing.

## Configuration backup

Open **Settings → Diagnostics** to export or import non-secret settings. Schema
v3 includes settings and the current applet's policies; schema v2 remains a
settings-only import. File selection validates the objects and prepares a
transient draft; it does not write settings or policies. Choose **Apply** to
save, or **Discard pending import** to retain your current saved values.

Changing settings pages prompts for Apply, Discard or Cancel. Applying replaces
policies in one transaction before changing applet settings. A policy-save
failure changes neither saved store, keeps the draft available for retry, and
prevents native OK from immediately closing the failed operation. Returning to
Diagnostics after a failed page-switch Apply restores the pending draft within
the same settings window. Closing or discarding the window releases its
transient payload.

Policy scope identities can appear in an explicit schema-v3 backup because they
are needed for exact restore. Treat the file as sensitive and do not attach it
to public diagnostics or bug reports.

Secrets remain in KWallet and must be configured separately on a new computer.

## Prometheus

Enable the metrics endpoint under History and choose an unused port. The server
binds to `127.0.0.1` by default.

Example check for the default port:

~~~bash
curl http://127.0.0.1:9464/metrics
~~~

Use a local Prometheus instance or an explicitly configured local forwarder
when possible. **Listen on all IPv4 interfaces** is an explicit opt-in for a
Prometheus server on another host. It binds to `0.0.0.0`; the endpoint has no
authentication or TLS, so restrict the selected port with a host or network
firewall. Anyone who can connect can read the exported metrics.

**Settings → History** shows the actual runtime state, bound address, and port.
If startup fails, it reports a typed reason such as a port already in use. A
missing or older-than-one-minute runtime snapshot is shown as **unknown**.
Opening Settings reads the existing endpoint state and does not start another
server.

Guardrail metrics use fixed source/risk/value-class labels.
Authenticated local-tool quota windows are exported as
`ai_usage_tool_quota_percent_remaining`, labeled by tool, window kind, source,
a Unix timestamp when the source provides one. Self-tracked
`ai_usage_tool_usage_count` and `ai_usage_tool_percent_used` require a local
activity observation from the last 15 minutes. Older activity is available
through `ai_usage_tool_last_activity_seconds`, while its count and percentage
are omitted until a fresh observation arrives. These metrics are local
estimates, not live provider quota.

### Cost metrics and v21 interface change

Cost classes remain separate and are emitted only when a fresh numeric value
and a recognized ISO currency are available:

- `ai_usage_api_spend{period="current|today|month",currency="…"}` for actual
  provider-reported spend
- `ai_usage_estimated_burn{period="month",cost_source="estimated_from_usage",currency="…"}`
  for local pricing estimates
- `ai_usage_subscription_fees{period="month",cost_source="self_tracked",currency="…"}`
  for known fixed subscription fees
- `ai_usage_tool_subscription_fee{tool="…",cost_source="self_tracked",currency="…"}`
  for each known tool plan fee

Unknown currencies are not relabeled as USD. Missing usage, activity, or price
values are omitted instead of exported as zero. An explicitly observed numeric
zero remains a valid sample.

Version 21 removes `ai_usage_total_monthly_exposure`. It mixed actual provider
billing, local estimates, and subscription fees into one value. Update any
consumer of that series to query the separate class-specific metrics above;
the project does not convert or add unlike currencies.

`ai_usage_guardrail_risk_state` uses `0` unavailable, `1` safe, `2` warning,
`3` critical and `4` exceeded.
`ai_usage_guardrail_seconds_until_event` appears only when a predicted event
exists. Their labels are limited to source, risk kind, and actual/estimated
value class.

Import `docs/grafana-dashboard.json` into Grafana for a starter dashboard. Pick
the Prometheus data source and, if necessary, select a subscription tool from
the dashboard variable.

## Slack and Discord webhooks

Webhooks use the same alert pipeline as KDE notifications.

1. Open **Settings → Alerts**.
2. Enable alerts and the required event types.
3. Enable Slack or Discord.
4. Paste the incoming webhook URL.
5. Set a webhook cooldown.

Webhook URLs are stored in KWallet. Alerts can contain provider names, status, and usage or budget context, so treat the destination as part of your data boundary.

Policy alerts are configured per policy under **Settings → Budget Control**.
They fire only for warning, critical, exceeded, real recovery and period reset.
Policy state/events persist before delivery and suppress the same transition
after refresh or restart. SQLite schema v8 records KDE, Slack and Discord
separately. **Settings → Alerts → Recent budget delivery** shows accepted,
sending, retry-pending, suppressed and action-required states.

KDE acceptance means submitted to the desktop notification system. Webhook
acceptance means HTTP 2xx from Slack or Discord. Neither proves that a person
read the message. A failed channel does not resend a channel already accepted.
Network failures, timeouts, HTTP 429 and server errors allow at most three
attempts per event/channel, with cooldown and Retry-After. Invalid URLs or
rejected credentials require configuration repair. Disabled channels and
outdated events are suppressed with a reason. Raw model, project, workspace,
line-item, policy and API-key identifiers are not sent.

**Settings → Alerts → Recent direct delivery** shows the latest safe result for
each ordinary Slack or Discord webhook: accepted HTTP status or a short reason
such as timeout, rate limiting, or permission failure. It never displays a
webhook URL or free-form network error. Budget-policy deliveries continue to
use their per-event receipts above.

## Alert tuning

Start with provider disconnect, reconnect, and API errors. Add budget warnings only for providers with compatible spend data. Set Do Not Disturb hours and a cooldown to avoid repeated notifications during a provider outage.

Existing global alert, Do Not Disturb, cooldown and per-provider switches still
apply. A policy snooze ends automatically when its next period begins.
