# Plasma AI Usage Monitor v16.0.0 — Control Loop

- **Document type:** Implementation plan
- **Status:** Phase 0 implemented and verified
- **Planning baseline:** `main` at `d51f597ebb0d23f7572c8d9d65f4ecf0beb85396`
- **Current release:** `15.0.0`
- **Target release:** `16.0.0`
- **Primary platform:** Fedora 44 KDE Plasma 6
- **Primary audience:** Individual developers monitoring AI API usage, spend,
subscription activity, quota, and reset windows from the Plasma panel

---

## 1. Mission

v15 established a shared, truthful daily state. v16 must turn that state into a
short control loop:

1. See the most important current condition.
2. Identify the affected source.
3. Understand the evidence, freshness, and data quality.
4. Take the correct source-specific action or inspect compatible history.

The release must reduce repeated information, fix known date and release-media
correctness defects, add a source-detail workflow, and deepen Anthropic support
through its read-only organization Usage and Cost APIs.

v16 is not a provider-count release. It must not add a hosted service, telemetry,
cloud synchronization, team dashboard, or automatic currency conversion.

## 2. Non-negotiable contracts

- Preserve package ID `com.github.loofi.aiusagemonitor`, the native QML URI,
  notifyrc, AppStream installation, and COPR/source distribution.
- Preserve the KWallet folder and all existing secret identifiers. New secrets
  must use KWallet and must never enter configuration exports, history, logs, or
  diagnostics.
- Preserve SQLite schema v4 history. Existing `model_scope` and `project_scope`
  columns are sufficient for v16 source dimensions.
- Preserve unavailable values as unavailable. A real, available numeric zero
  must remain distinguishable from missing data.
- Keep actual usage and spend, local estimates, balances, fixed fees, and
  connectivity checks separate. Never combine incompatible currencies.
- Scheduled provider requests remain read-only and non-billable.
- History and Analyst work remains asynchronous, bounded, and superseding.
- Keep connectivity-only sources useful but subordinate to sources reporting
  actionable usage, spend, or quota.

## 3. Delivery phases

### Phase 0 — Baseline and contract cleanup

- Record a clean v15 baseline, current gates, lint debt, performance budgets,
  release-media coverage, and known defects.
- Mark the v15 plan as historical and add this canonical v16 plan.
- Add v16 to the roadmap without changing the stable `VERSION`.
- Correct the supported-release table to the current stable major.
- Retire the unused `dashboardMode` and `showOnlyProblems` KConfig entries.
  Schema-v2 imports accept and ignore these legacy keys; new exports omit them.
- Preserve every other setting, all KWallet data, schema-v4 history, catalogs,
  and release surfaces.

Phase 0 is complete when its focused config regression and all repository gates
pass and the working diff contains no v16 runtime feature.

### Phase 1 — Correctness and trustworthy gates

- Define Analyst periods as the UTC half-open interval
  `[fromInclusive, toExclusive)`. A 7, 30, or 90 day request must contain exactly
  that number of UTC calendar days across local time zones and DST boundaries.
- Apply the same range contract to UI coverage, reports, and SQL queries.
- Configure qmllint with correct imports and type information. Declare legitimate
  context properties explicitly, remove shipped warnings, and enforce a
  machine-readable zero-warning release gate.
- Isolate release-media capture with temporary HOME/XDG directories, an empty
  desktop, and controlled wallpaper.
- Bind every capture to the expected PID/window identity and an AT-SPI
  view/state marker before and after capture. The manifest records this evidence
  and rejects the wrong window.
- Add a version-policy check covering `VERSION`, roadmap, security policy,
  AppStream, package metadata, RPM spec, and catalog release metadata.

### Phase 2 — Daily Focus and Source Detail

- Replace the custom three-button destination row with
  `Kirigami.NavigationTabBar`; move refresh, setup, and settings to an accessible
  action toolbar.
- Replace duplicated Overview status, quota-reset, and spend cards with one
  Daily Focus showing at most one primary action plus compact, non-overlapping
  facts for quota, reset, and spend.
- Sort source rows by attention, actual reporting, estimate or balance,
  connectivity-only, then unavailable.
- Open each source in a linear Source Detail workflow with restored keyboard
  focus on return.
- Add a typed `SourceDetailModel` exposing source status, source-specific action,
  freshness, quota windows, typed metrics, provenance, coverage, and compatible
  recent history.
- Source Detail provides links to source settings and pre-filtered History.
- Replace generic “Fix” actions with concrete labels such as “Add credential”,
  “Refresh”, “Review quota”, or “Open source settings”.

### Phase 3 — Responsive History and Analyst

- Wrap History controls at narrow widths and label the modes “Single source” and
  “Compare”.
- Reserve chart margins from measured labels so first and last axis labels are
  not clipped; preserve explicit gaps and visible multi-series legends.
- Support deep links from Source Detail with source, metric, and range selected.
- Use a responsive CardsLayout for Analyst KPIs: three columns wide, two medium,
  and one narrow.
- Show period and coverage before KPIs and format currency consistently.
- Preserve insufficient-data explanations and mixed-currency failure behavior.
- Extract Analyst SQL and result projection from the database implementation
  into focused worker/query units without changing the superseding public
  request contract.

### Phase 4 — Anthropic actual usage and cost

- Extend Catalog v5 authentication metadata with credential alternatives and
  capability-specific requirements:
  - standard `anthropic` key for connectivity;
  - optional `anthropic_admin` key for organization usage and cost.
- Store the Admin key in the existing KWallet folder. Never overwrite or migrate
  the standard key.
- Fetch the read-only message usage and cost reports. Scheduled refresh covers
  the current and previous UTC day at no less than five-minute intervals;
  initial/manual backfill is capped at 31 daily buckets.
- Follow pagination to completion. A safety limit produces typed partial or
  unavailable state rather than a false complete total.
- Add cache-read and cache-creation input-token metric kinds.
- Add optional `modelScope` and `projectScope` fields to provider metric maps and
  persist them in existing schema-v4 columns.
- Parse cost decimal strings through integer micro-USD before presentation or
  storage.
- Track usage and cost capability status independently. Retain a previous value
  as stale on partial failure; never replace unavailable data with zero.
- Mark Priority Tier usage as covered while keeping unavailable Priority Tier
  cost explicit.
- Cover authentication, permission, pagination, rate-limit, cancellation,
  schema, empty-bucket, cache-token, and partial-endpoint cases with mocked HTTP
  tests.

### Phase 5 — Maintainability, documentation, and release readiness

- Extract only verified hotspots: Anthropic parsing/pagination, Analyst query
  workers, and provider runtime registration.
- Keep the provider catalog authoritative and avoid broad provider rewrites.
- Update the user guide, generated wiki, architecture, capability matrix,
  privacy/security material, and screenshots from one isolated capture session.
- Capture Overview, attention, Source Detail, History gap, Analyst sufficient
  and insufficient, onboarding, provider settings, plugin recovery, and compact
  panel states.
- Verify a real v15-to-v16 Fedora package upgrade preserves KConfig, KWallet,
  schema-v4 history, package identity, and typed data semantics.
- Prepare a complete local release candidate. Commit, push, tag, COPR, GitHub,
  wiki, and KDE Store publication require explicit release authorization.

## 4. Public interfaces and data contracts

- `ProviderBackend::MetricKind` gains cache-read and cache-creation input-token
  kinds.
- Provider metric maps gain optional `modelScope` and `projectScope` fields.
- Catalog v5 gains credential-alternative and capability-credential metadata;
  existing provider credential declarations remain valid.
- `SourceDetailModel` becomes a registered QML type with stable availability and
  provenance roles.
- Analyst `from` and `to` use inclusive/exclusive UTC semantics.
- Configuration export stays at schema version 2. Phase 0 reduces its active
  settings set by two ignored legacy fields.
- No SQLite schema migration or new listening network service is introduced.

## 5. Verification and acceptance

Every phase runs focused tests before the broad gates:

```bash
just test
just check
just qml-lint
just phase7-check
just release-check
PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py \
  --strict --build-dir build/debug --expected-version "$(< VERSION)"
QT_QPA_PLATFORM=offscreen dbus-run-session -- \
  bash scripts/smoke_test_plasmoid.sh build/debug
git diff --check
```

Required scenarios include:

- healthy, attention, stale, unavailable, mixed-currency, and available-zero;
- exact UTC ranges across Stockholm, UTC, New York, and DST boundaries;
- Source Detail keyboard entry, return focus, settings action, and History link;
- retained history, explicit gaps, compatible and incompatible comparisons;
- Analyst sufficient and insufficient evidence at narrow and wide sizes;
- Anthropic standard-only, Admin success, pagination, partial cost failure,
  Priority Tier, 401/403, 429, schema failure, and cancellation;
- KWallet locked/cancelled, missing/older/newer plugin, and v15 upgrade;
- release-media rejection of a deliberately wrong active window.

Existing Phase 7 absolute budgets remain:

- History 100k observations under 2,000 ms;
- Analyst 100k observations under 1,000 ms;
- zero retained workers and one database connection after completion;
- startup median at or below 427 ms on the canonical comparison environment;
- first and warm panel popup at or below 100 ms.

A new same-machine baseline is recorded before UI work. No phase may regress it
by more than 10% without documented evidence and an explicit plan change.

## 6. Release completion

v16 is complete only after all five implementation phases pass, the exact
source archive and Fedora package upgrade are verified, the release-media
manifest proves the intended surfaces, and every requested public surface is
independently read back. Local implementation does not itself authorize public
release.
