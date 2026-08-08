# v18 GitHub and KDE Store submission checklist

Use this checklist only after runtime, performance, accessibility,
documentation, and release media qualification is complete for the exact
v18.0.0 release commit.

## Preconditions

- every mandatory command in `docs/release/v18.0.0-checklist.md` passes against
  the exact candidate tree
- isolated Fedora 44 lifecycle, performance, accessibility, privacy, and
  teardown evidence is complete
- the v17/v18 popup comparison contains at least three warmups and 20 valid
  first-open and warm-open samples per version from one boot and one virtual
  Plasma session family
- roadmap, changelog, user guide, generated wiki, AppStream, and RPM metadata agree
- the ten canonical screenshots pass `scripts/check_release_media.py`
- the Store archive remains the frontend and explicitly requires the matching
  compiled plugin

## Listing copy

### Short description

Control AI spend with local policies, scoped costs, pacing, and private alerts.

### Long description

AI Usage Monitor 18 adds Budget Control: typed local policies for calendar
days, ISO weeks, calendar or anchored months, and verified provider reset
periods. The Plasma overview prioritizes active incidents, shows spent and
remaining budget, safe-today pacing, reset timing, and a direct action without
treating missing data as zero.

Policies can target aggregate spend or provider-declared scopes. Compatible
scoped costs reconcile against aggregate totals and show Unattributed cost when
the provider omits a dimension. Actual and estimated values, currencies, and
scopes remain separate; unsupported or incomplete evidence produces an
explicit unavailable reason instead of an invented forecast.

The searchable Budget Control editor stages create, duplicate, enable, snooze,
and delete operations until Apply. Warning, critical, exceeded, recovered, and
period-reset transitions are persisted before delivery and deduplicated across
restarts. Notifications and optional local integrations expose only allowlisted,
low-cardinality summaries.

Secrets stay in KWallet and observations, policies, and event state stay in a
local SQLite database. There is no telemetry, hosted backend, currency
conversion, provider-side budget mutation, billable inference, or automatic
model switching. Explicit configuration backups may contain sensitive local
scope identifiers and should be protected accordingly.

The KDE Store package contains the frontend. Install the matching native plugin
from the supported COPR repository or a source build first.

## Release media inventory

Keep the ten reviewed Breeze Dark captures from the same isolated headless
session in the release manifest:

- `assets/screenshots/overview-popup.png`
- `assets/screenshots/attention-state.png`
- `assets/screenshots/source-detail.png`
- `assets/screenshots/history-gap.png`
- `assets/screenshots/analyst-sufficient.png`
- `assets/screenshots/analyst-insufficient.png`
- `assets/screenshots/guided-first-success.png`
- `assets/screenshots/budget-control.png`
- `assets/screenshots/plugin-recovery.png`
- `assets/screenshots/panel-lowest-quota.png`

The manifest must identify the capture commit, date, Plasma version, scale,
fixture digest, scenario for every file, source-tree digest, accessibility
evidence, and file hashes.

KDE Store accepts at most five gallery pictures. Upload these representative
captures there:

- `assets/screenshots/overview-popup.png`
- `assets/screenshots/source-detail.png`
- `assets/screenshots/history-gap.png`
- `assets/screenshots/analyst-sufficient.png`
- `assets/screenshots/budget-control.png`

## GitHub release pack

- source tarball
- `com.github.loofi.aiusagemonitor.plasmoid`
- SHA-256 checksums
- SPDX source SBOM
- v18 changelog and release notes
- exact-tag provenance from the release workflow

Release notes must describe policy periods and time zones, actual versus
estimated value classes, scoped reconciliation and Unattributed, typed
unavailable forecasts, transition-only alerts, migration and rollback, explicit
backup sensitivity, and the read-only/frontend-native-plugin boundaries.

## Publication sequence

1. verify `docs/release/v18.0.0-checklist.md` is complete through qualification
2. bump every version surface to 18.0.0 and rerun the full exact-tree gate
3. create and verify reproducible plasmoid, source, RPM, SBOM, and checksum artifacts
4. fast-forward `main`, verify CI, and create annotated tag `v18.0.0` on the exact release commit
5. verify the tag workflow draft release, artifact lineage, hashes, and signatures
6. publish the GitHub release without rebuilding or replacing artifacts
7. submit the same tag to COPR for every supported Fedora target and verify a clean install
8. upload the exact plasmoid, listing copy, and five gallery images to KDE Store product `2353976`
9. publish the generated wiki mirror from the same release commit
10. perform unauthenticated readback of GitHub, COPR, KDE Store, and wiki
11. record publication evidence in a separate commit on `main` without moving the tag

If any mandatory surface, credential, lineage check, or unauthenticated readback
is unavailable, the release remains blocked and must not be described as
published.

## Final review

- unavailable values never look like actual zeroes
- policy periods use documented local-time boundaries converted to exact UTC intervals
- actual, estimated, currency, and scope classes are never mixed
- aggregate equals compatible scoped values plus Unattributed within defined rounding
- raw scope identifiers appear only in local storage and explicit configuration backups
- Budget Control saves only through Apply and Cancel leaves both persistence layers unchanged
- notifications and integrations contain no policy IDs or raw provider scope IDs
- GitHub, COPR, KDE Store, and wiki describe the same v18 product and exact artifacts
