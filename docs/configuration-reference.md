# Configuration reference

AI Usage Monitor keeps preferences in the Plasma applet configuration, secrets
in KWallet and typed budget policies in the local SQLite database. These stores
have separate transaction boundaries.

## Budget policy fields

| Field | Contract |
| --- | --- |
| `policyId` | Stable UUID; import/update identity |
| owner | Stable applet instance; policies never leak between widgets |
| `sourceId`, `sourceKind` | Catalog source and provider/tool class |
| `scopeMode` | Exactly `aggregate` or `scoped` |
| `scopeKind`, `scopeIdentity`, `scopeLabel` | Catalog dimension, local raw identity, safe label snapshot |
| `valueClass` | Exactly `actual` or `estimated` |
| `limitMinor` | Positive integer minor units |
| `currency` | Known ISO 4217 code; no FX |
| `periodType` | `calendar_day`, `iso_week`, `calendar_month`, `anchored_month`, or verified `provider_reset` |
| `anchorDay` | Required 1–28 only for anchored month |
| `timeZoneId` | Valid saved IANA zone for calendar policies |
| `warningPercent`, `criticalPercent` | `0 < warning ≤ critical ≤ 100` |
| notification/enabled | Boolean delivery and calculation controls |
| created/updated | Repository-owned UTC timestamps |
| snooze | Local state ending at the next period start |

Repository create/update/duplicate/delete/enable/snooze operations validate the
whole policy. QML never writes policy SQL. Settings stages edits and calls the
atomic replacement boundary only from Apply.

## Legacy v17 keys

Every `*DailyBudget` and `*MonthlyBudget` KConfig key remains hidden and
unchanged for v17 rollback. v18 creates deterministic UTC actual-cost policies
from non-zero values once per applet/key and records a migration marker. Runtime
pacing and notifications never read those keys after migration. Deleting a
migrated policy does not recreate it.

## Portable configuration

Schema v2 import contains settings only. Schema v3 contains:

~~~json
{
  "schemaVersion": 3,
  "settings": {},
  "budgetPolicies": []
}
~~~

Import parses and validates every setting and policy before mutation. Apply
replaces policies for the current applet by ID in one database transaction,
then saves KConfig. A policy transaction failure leaves permanent KConfig
unchanged; Cancel changes neither store.

Config files exclude provider keys, tokens, cookies, personal access tokens and
webhook URLs. Schema-v3 policies can contain raw scope identities required for
exact restoration, so explicit backup files are sensitive. Diagnostics,
support reports and integrations use separate allowlists and exclude them.

## SQLite schema v6

- `budget_policies`: validated owner-scoped policy definitions
- `budget_policy_migrations`: one-shot legacy migration markers
- `budget_policy_state`: current policy period/risk and delivery timing
- `budget_policy_events`: deduplicated pending, delivered or suppressed
  transitions persisted before delivery

Indexes cover owner, source, scope, enabled state and transition identity.
Migration from v5 runs in one SQLite transaction after a `.v18-backup` and is
idempotent. An injected error rolls back schema and data.

Forecasts are derived runtime results. They are never written into raw
observation history. Provider observations can remain in their existing major
unit form; conversion to checked minor units occurs at the query boundary.

## Rollback

The v18 RPM does not delete configuration, secrets or history on removal. v17
continues to read its legacy KConfig budgets and ignores schema-v6 tables. Keep
the `.v18-backup` if the database itself must be restored for an older binary.
Do not copy or delete KWallet data as part of a normal rollback.
