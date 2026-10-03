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

File selection parses and validates every setting and policy into a transient
window-owned draft. No settings or policies are written at that boundary. Apply
replaces policies for the current applet by ID in one database transaction,
then saves KConfig. A policy transaction failure leaves permanent KConfig
unchanged; Cancel changes neither store. Schema-v2 import filters retired and
unknown legacy settings, validates the retained types and leaves policies
untouched. V3 rejects unknown keys, invalid types, out-of-range settings, invalid fixed choices, reversed alert thresholds and duplicate policy UUIDs.

`ConfigApplyGuard` rejects the immediate native OK close after failed Apply,
because Plasma ignores a page's false `saveConfig()` return. Its guard expires
on the next event-loop turn so deliberate later Cancel remains possible. A
library-created draft survives a failed page-switch Apply until it is retried,
discarded or its settings window closes.

Config files exclude provider keys, tokens, cookies, personal access tokens and
webhook URLs. Schema-v3 policies can contain raw scope identities required for
exact restoration, so explicit backup files are sensitive. Diagnostics,
support reports and integrations use separate allowlists and exclude them.

## SQLite schema v8

Runtime schema v8 extends the real v7 provenance database with:

- `budget_policy_deliveries`: event/channel state, attempt count, next retry,
  acceptance time and stable failure/suppression reason
- `history_operations`: latest local manual/scheduled operation outcome

Existing policy definitions, observations, provenance, event and migration
records remain intact. Upgrade from v7 checkpoints WAL and creates a
`.v21-backup` before the transaction. Migration is idempotent; injected failure
rolls back schema and data. Existing events are not assigned invented
per-channel acceptance receipts.

Full-history JSON export is schema v7; selected-series JSON remains v6. These
format numbers are independent of SQLite schema v8 and config schema v3.
Per-channel CSV is a separate policy-events file. Explicit export projections
exclude raw scope identities, secrets and policy identifiers.

Forecasts are derived runtime results. They are never written into raw
observation history. Provider observations can remain in their existing major
unit form; conversion to checked minor units occurs at the query boundary.

## Rollback

Normal package removal retains user configuration, KWallet and local history.
V21 must never open the schema-v8 database. Preserve the current v8 database
and its WAL state before considering any downgrade; do not overwrite live
history or copy/remove KWallet as part of ordinary rollback.

Qualify rollback in an isolated data directory using a copy of the
pre-migration `usage_history.db.v21-backup` (schema v7) and the older binary.
The current v8 database stays separate for re-upgrade. If no verified v7 backup
exists, database rollback is unavailable; do not treat removing the package as
a schema downgrade.

Full-history exports omit the free-form `provenance_json` blob. Explicit catalog, pricing period, fingerprint and estimate-status columns remain available; arbitrary provenance keys never cross the export boundary.
