# v16 GitHub and KDE Store submission checklist

Use this checklist after runtime, performance, accessibility, documentation, and
release media are complete.

## Preconditions

- `just release-check` passes against the exact candidate tree
- the Fedora 44 performance and accessibility evidence is complete
- roadmap, changelog, user guide, generated wiki, AppStream, and RPM metadata agree
- the ten canonical screenshots pass `scripts/check_release_media.py`
- the install story remains explicit: the Store archive is the frontend and the matching compiled plugin is required

## Listing copy

### Short description

See AI usage, quota, resets, and spend truthfully from the Plasma panel.

### Long description

AI Usage Monitor turns provider-reported usage and spend, synchronized
coding-tool quota, local activity, balances, and safe connection checks into one
daily Plasma view. It prioritizes the source that needs attention, the lowest
live quota, and the next live reset without converting missing data to zero.

Actual data, local estimates, fixed subscription fees, balances, connectivity,
and currencies remain separate. Disabled-source history stays selectable,
missing periods remain chart gaps, and Analyst explains coverage and sample
requirements before presenting a result.

Secrets stay in KWallet and history stays in a local SQLite database. There is
no telemetry or hosted backend. The KDE Store package contains the frontend;
install the matching native plugin from COPR or a source build first.

## Release media inventory

Keep the ten reviewed Breeze Dark captures from the same isolated session in
the release manifest:

- `assets/screenshots/overview-popup.png`
- `assets/screenshots/attention-state.png`
- `assets/screenshots/source-detail.png`
- `assets/screenshots/history-gap.png`
- `assets/screenshots/analyst-sufficient.png`
- `assets/screenshots/analyst-insufficient.png`
- `assets/screenshots/guided-first-success.png`
- `assets/screenshots/provider-settings.png`
- `assets/screenshots/plugin-recovery.png`
- `assets/screenshots/panel-lowest-quota.png`

The manifest must identify the capture commit, date, Plasma version, scale,
fixture digest, scenario for every file, and file hashes.

KDE Store accepts at most five gallery pictures. Upload these representative
captures there:

- `assets/screenshots/overview-popup.png`
- `assets/screenshots/source-detail.png`
- `assets/screenshots/history-gap.png`
- `assets/screenshots/analyst-sufficient.png`
- `assets/screenshots/panel-lowest-quota.png`

## GitHub release pack

- source tarball
- `com.github.loofi.aiusagemonitor.plasmoid`
- SHA-256 checksums
- SPDX source SBOM
- v16 changelog and release notes
- exact-tag provenance from the release workflow

Release notes must call out Daily Focus and Source Detail, exact UTC period
semantics, retained History gaps, Analyst methodology, optional Anthropic Admin
reporting, and the frontend/native-plugin packaging boundary.

## Manual publication sequence

1. verify `docs/release/v16.0.0-checklist.md` is complete
2. push the release commit and exact annotated tag
3. verify CI and the draft GitHub release assets against that tag
4. publish the GitHub release
5. submit the exact stable tag to COPR and verify Fedora 44 installation
6. upload the listing copy and five representative screenshots to KDE Store
7. confirm the Store page still states the compiled-plugin requirement before the install action
8. read back GitHub, COPR, AppStream-facing metadata, and the Store listing independently

## Final review

- unavailable values never look like actual zeroes
- the quota/reset displays use synchronized or provider-reported windows
- Source Detail preserves availability, freshness, and provenance
- History labels the disabled source and visible gap
- Analyst sufficient and insufficient states match the documented thresholds
- GitHub, COPR, and KDE Store describe the same v16 product
