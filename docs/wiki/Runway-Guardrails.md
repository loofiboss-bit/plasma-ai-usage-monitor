<!-- Generated from docs/user-guide/runway-guardrails.md by scripts/generate_wiki_docs.py; do not edit. -->
# Budget Control and runway guardrails

Budget Control turns compatible local cost observations into deterministic
policy results. Quota runway remains available for synchronized request/token
allowances. Neither path calls an LLM, runs inference, converts currencies or
changes a provider account.

## Create a policy

Open **Settings → Budget Control**, select **Create policy**, choose a template
or configure the fields, then select **Apply**. The editor stages every change;
moving between fields, closing Settings or selecting Cancel does not save.

A policy has one source, one value class and exactly one aggregation mode:

- **Aggregate** uses the provider's compatible account/gateway total.
- **Scoped** uses one catalog-declared provider dimension and one local scope
  identity. The identity is needed for exact local filtering and backup restore.
- **Actual** accepts only provider-reported billing/usage cost.
- **Estimated** accepts only local price-derived observations.

Limits are stored as integer minor units in the selected ISO currency. The
widget uses its checked-in ISO precision table. An unknown currency is
unavailable; precision is never guessed and there is no FX conversion.

Warning and critical thresholds must satisfy
`0 < warning ≤ critical ≤ 100`. New policies default to 80/90. A policy can be
duplicated, disabled without deletion, deleted with confirmation, or snoozed
until the next period starts.

## Billing periods and time zones

Choose one period:

- calendar day
- ISO week beginning Monday
- calendar month
- anchored month with reset day 1–28
- verified provider reset, only when an authenticated stable reset and a
  reviewed catalog contract are both present

Calendar policies save the selected IANA time zone. Local boundaries are
converted to exact UTC half-open intervals: the start is included and the end
is excluded. This keeps 23- and 25-hour DST days correct. Migrated v17 daily and
monthly budgets use UTC to preserve their old meaning.

A provider reset that changes, is unauthenticated or is not catalog-declared is
`unstable-reset`; it is never treated as a calendar approximation.

## Pacing result

`budget-pacing-v2` reports:

- current period start and end
- spent and remaining minor units
- percentage consumed
- projected spend at period end
- predicted overrun time when supported by evidence
- **Safe today**
- remaining daily allowance
- sample count, coverage and evidence grade
- compatible previous-period comparison
- a typed unavailable reason when calculation is unsafe

**Safe today** never becomes negative. It is the smaller of remaining budget
and today's remaining pro-rata room, clamped at zero. **Remaining daily
allowance** divides remaining budget by remaining local calendar days including
today.

The forecast baseline uses complete compatible UTC days and excludes the
current incomplete UTC day. Missing days are not filled with zero; an explicit
recorded zero is valid. The v17 minimum sample and coverage requirements remain
blocking, so sparse evidence returns unavailable instead of a weak projection.

Previous-period comparison uses only the same policy, scope, currency and value
class. Forecasts are runtime results and are never written into observation
history as provider facts.

## Risk order

Risk is deterministic:

1. **Unavailable** if a required contract is missing.
2. **Exceeded** when spend is at least the policy limit.
3. **Critical** at the configured critical threshold.
4. **Warning** at the warning threshold or when compatible evidence predicts a
   period overrun.
5. **Safe** otherwise.

Current incidents remain ahead of future risk on Overview. A risky result that
becomes unavailable is not described as healthy or recovered.

Common unavailable reasons include `no-data`, `insufficient-samples`,
`mixed-value-class`, `mixed-currency`, `scope-unavailable`, `unstable-reset`,
`invalid-policy`, `unknown-currency` and `query-failed`. Unavailable does not
mean zero or safe.

## Provider scope and `Unattributed`

Catalog v6 controls which scopes can be selected:

- OpenAI: aggregate, project cost and cost line item. OpenAI model cost is
  never inferred.
- Anthropic: aggregate plus workspace, model, service tier and cost dimensions
  actually reported by the Admin API.
- OpenRouter: aggregate.
- LiteLLM: aggregate unless the validated gateway response and catalog contract
  explicitly expose another supported scope.

Source Detail initially renders at most eight scope rows; **Show all** creates
the remainder only when activated. Removed or renamed provider objects keep a
safe local label snapshot.

When compatible scoped values are lower than aggregate, the difference appears
as **Unattributed** within minor-unit rounding. Scoped-over-aggregate is a
mismatch and becomes unavailable rather than displaying negative attribution.
Scoped values are never added back into aggregate totals.

## Quota runway

Quota runway asks whether synchronized remaining requests or tokens are likely
to reach zero before the reported reset. It requires four compatible samples,
15 minutes of span, a latest sample no more than 15 minutes old, one stable
source/unit/limit/scope/window/reset and adequate coverage.

A bounded deterministic Theil–Sen slope projects exhaustion. Flat compatible
usage is safe. A changed reset, non-monotonic remaining value, stale sample or
missing contract is unavailable. Warning means exhaustion precedes reset;
critical means it is within the greater of 60 minutes or 25 percent of the
remaining window.

## Notifications and snooze

Enable policy notifications in the policy editor and global alerts under
**Settings → Alerts**. Events exist only for warning, critical, exceeded,
recovered and period reset:

- recovery is created only when a real risky state becomes safe
- risky-to-unavailable is never recovery
- unavailable-to-safe is never recovery
- reset is recorded at most once per policy and period

The event and new state are committed atomically before KDE delivery. DND,
cooldown or delivery failure leaves a pending/suppressed event instead of
losing it. Deduplication survives process restart. Snooze lasts until the next
period start and expires automatically at reset.

The KDE action opens Budget Control with the local policy selected. Policy and
scope IDs are not placed in external URLs or notification payloads.

## Integrations and privacy

Slack and Discord policy messages contain only provider display name, risk,
coarse percentage class, period and locally generated link text. Prometheus
uses fixed provider/risk/value-class labels; policy IDs and scope fields are
forbidden labels. The risk gauge uses `0` unavailable, `1` safe, `2` warning,
`3` critical and `4` exceeded.

Raw project, workspace, model, line-item and API-key identities stay local.
Explicit config backups can contain policy scope identities for restoration,
so review and protect those files. Diagnostics, standard reports, webhooks and
metrics exclude them.

Budget Control is informational and read-only. It never switches a model,
writes a provider budget or credential, runs billable inference or performs
another provider-side mutation.
