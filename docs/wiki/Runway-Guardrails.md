<!-- Generated from docs/user-guide/runway-guardrails.md by scripts/generate_wiki_docs.py; do not edit. -->
# Runway guardrails

Runway guardrails are deterministic local projections. They use compatible
provider observations already stored by the widget; they do not call an LLM,
run inference, convert currencies, or change provider settings.

## Quota runway

Quota runway asks whether a synchronized request or token allowance is likely
to reach zero before its reported reset. It requires:

- at least four compatible remaining-quota observations
- at least 15 minutes between the first and last observation
- a latest observation no more than 15 minutes old
- one unchanged provider, metric, unit, limit, scope, window, and reset
- a declining series with enough observed time coverage

The calculation uses a bounded deterministic Theil–Sen slope. A flat series is
safe because no consumption was observed. An increase inside one reset window,
a changed reset, stale data, gaps below the coverage requirement, or a missing
value makes the forecast unavailable instead of guessing.

A forecast is a warning only when projected exhaustion precedes reset. It
becomes critical when exhaustion is within the greater of 60 minutes or
25 percent of the remaining reset window.

## Monthly budget pacing

Budget pacing asks whether compatible month-to-date spend is projected to
exceed the configured monthly budget. It uses the median of completed UTC
calendar days and excludes the current incomplete UTC day.

Pacing requires at least five represented completed days and at least 70 percent
coverage of elapsed completed days. Missing days are not filled with zero.
Actual billing rows and local estimates are calculated separately.

The widget never converts currencies. The budget currency and every included
observation must match. Mixed currencies, mixed actual/estimated values, or a
missing budget make pacing unavailable.

## Evidence and unavailable states

Each card shows its sample count, coverage, evidence grade, calculation method,
generation time, and period end. Evidence is derived mechanically:

- **Usable** meets every minimum needed to calculate the result.
- **Strong** meets the higher sample, coverage, freshness, and stability
  thresholds published by the calculation method.
- **Unavailable** means the result failed a named contract requirement.

Common unavailable reasons include insufficient samples or time span, stale
data, changed reset, non-monotonic quota, insufficient completed-day coverage,
missing budget, incompatible currency, and mixed actual/estimated values.
Unavailable is different from numeric zero.

All period boundaries and stored timestamps use UTC half-open intervals. Local
time is used only to display timestamps.

## Scope attribution

Source Detail may show provider-reported model, project, workspace,
service-tier, or cost-line-item rows. The widget never invents an absent
dimension:

- OpenAI usage can report model and project groupings.
- OpenAI cost can report project and line-item groupings, not model cost.
- Anthropic usage can report model, workspace, and service tier.
- Anthropic cost line items remain provider-reported cost dimensions.

Aggregate rows drive Daily Focus and forecasts. Scoped detail rows are kept
separate so totals are not counted twice. Raw scope identifiers stay local and
the UI masks project identifiers.

## Settings

Open **Settings → Guardrails**.

- **Show deterministic runway guardrails** defaults to on.
- Monthly provider budgets are stored as integer USD cents. A non-USD
  observation cannot be paced against them.
- **Notify only when a guardrail state changes** defaults to off.
- Notification lead time can be 1, 6, 24, or 48 hours; the default is 6.

Forecast notifications also respect the global alert switch, Do Not Disturb,
cooldowns, and each provider's notification toggle. Enabling forecast
notifications enables the local history database needed for observations and
restart-safe transition evidence even if the general History switch is off.

## Notifications and integrations

A forecast notification is emitted only when a forecast enters warning, enters
critical, or leaves a prior warning/critical state. The schema-v5 database
persists that transition before delivery, so unchanged refreshes and restarts
do not repeat it.

KDE, Slack, and Discord use the same typed event meaning. External messages do
not contain raw project, workspace, model, API-key, or scope identifiers.

The loopback Prometheus endpoint exposes only aggregate series by provider, risk
kind, and actual/estimated value class:

~~~text
ai_usage_guardrail_risk_state
ai_usage_guardrail_seconds_until_event
~~~

Risk state values are `0` unavailable, `1` safe, `2` warning, and `3` critical.
Multiple compatible windows collapse to the worst state and earliest event for
that provider/risk/value-class series.

Guardrails are informational. They never switch a model, change a budget,
revoke a key, or write to a provider.
