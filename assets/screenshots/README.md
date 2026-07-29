# v17 screenshot playbook

Capture the v16 set only after runtime, strings, documentation, and version
metadata are final:

```bash
scripts/demo/capture_v17_media.sh build/debug assets/screenshots
python3 scripts/check_release_media.py
```

The capture command installs into a temporary prefix, uses isolated config,
cache, data, KWallet-free demo state, and a nested Plasma session for the panel.
It does not modify the active widget installation, user history, or panel.

## Required set

| File | Required truth |
| --- | --- |
| `overview-popup.png` | Real narrow popup; actual, estimated, balance, and fixed-fee categories stay separate |
| `attention-state.png` | Critical synchronized quota is the first recovery action |
| `source-detail.png` | One source exposes provenance, freshness, quota, typed metrics, and source actions |
| `history-gap.png` | Missing interval is an explicit chart gap, not zero |
| `analyst-sufficient.png` | Compatible history satisfies documented sample requirements |
| `analyst-insufficient.png` | Unavailable KPIs explain observed and required samples |
| `guided-first-success.png` | Onboarding keeps the first successful source path explicit |
| `provider-settings.png` | Provider settings show the source-first credential workflow |
| `plugin-recovery.png` | Frontend-only install gives deterministic native-plugin recovery |
| `panel-lowest-quota.png` | Real isolated Plasma panel in lowest-quota mode |

Do not use unavailable values as actual data, combine currencies, treat a
published plan cap as live quota, or hide the disabled/history-only label.

## Session contract

All files use Breeze Dark at 100% scale in one Fedora KDE Plasma session. The
generated `v17-media-manifest.json` records:

- release version
- session ID
- combined fixture digest
- source-tree digest for the exact package and capture tooling
- source-tree mode (`git-commit` or an explicit local release candidate)
- capture commit and UTC time
- Plasma version and scale
- exact scenario for each image
- SHA-256 hash for every image

The release gate checks the manifest, fixture digest, dimensions, distinct image
hashes, README references, and AppStream references.
