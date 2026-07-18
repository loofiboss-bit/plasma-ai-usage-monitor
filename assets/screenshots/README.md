# v14 screenshot playbook

Capture the v14 set in one Fedora Plasma session after UI strings, demo data, and release metadata are final. The README and AppStream metadata use images from this directory.

## Stable filenames

Keep these filenames stable unless you also update every consumer:

- `main-window.png`
- `panel-view.png`
- `settings-view.png`
- `provider-intelligence.png`
- `analyst-view.png`

Add these v14 filenames and keep them stable after publication:

- `guided-first-success.png`
- `verified-success.png`
- `history-view.png`

The README and AppStream metadata reference the existing stable names. Add a new filename to a public surface only after its image is captured and reviewed.

## Required v14 set

Use the same deterministic demo fixture, Breeze Dark theme, scale, wallpaper, and panel placement for every shot. Capture the entire set from an isolated demo-user environment with separate configuration, cache, and data paths; never reuse the regular user's provider credentials or history.

### `guided-first-success.png`

Capture source choice with:

- the **Guided first success** title
- one recommended source
- monitoring levels visible before configuration
- no credentials or personal paths

### `verified-success.png`

Capture the result step with one explicit outcome: provider usage or spend, gateway data, balance, local estimate, or connectivity only. The result must match the chosen source and demo response.

### `main-window.png`

Capture the expanded popup with:

- the v14 **Overview** destination selected
- reporting providers separate from collapsed connection checks
- actual data, estimates, balances, and attention counts matching the fixture
- no errors, secrets, or placeholder-looking values

### `provider-intelligence.png`

Capture at least one expanded provider card with:

- a useful provider-reported or gateway value from the fixture
- typed source and quality labels
- no duplicate quality badge
- last-success and last-attempt information

### `panel-view.png`

Capture the compact panel representation with:

- the widget seated in a clean Plasma panel
- a readable badge or selected signal that agrees with the expanded Overview
- no unrelated noisy widgets stealing attention

### `settings-view.png`

Capture a polished configuration state with:

- the source list and one selected detail pane
- a monitoring-level filter
- required permission and scheduled check visible
- the **Advanced** switch in the intended state
- no exposed API keys
- a verification result only if it came from the fixture

### `history-view.png`

Capture History with one compatible unit and currency. Do not imply that missing observations are zero.

### `analyst-view.png`

Replace the stale Analyst image. Use current navigation, current model names, and values from the same demo run as Overview and History.

## Capture rules

- use a real Fedora KDE session, not an image mockup
- use Breeze Dark for both the Plasma theme and application color scheme, even if the regular desktop is in light mode
- use an isolated demo-user environment with dedicated configuration, cache, and data paths
- keep wallpaper calm and non-distracting
- use consistent scale, theme, and panel placement across the set
- remove personal names, local hostnames, and private sessions
- prefer one useful result over a dense wall of cards
- crop tightly enough to showcase the widget, but leave enough surrounding UI to feel native

## Replacement checklist

1. Build and install the exact candidate commit in the Fedora KDE demo-user environment.
2. Start `python scripts/demo/mock_ai_usage_server.py`.
3. Run Plasma with `PLASMA_AI_MONITOR_DEMO=1 plasmashell --replace &`.
4. Capture Guided first success, verified success, Overview, provider detail, Settings, History, Analyst, and panel state.
5. Check every model, amount, unit, source label, and timestamp against the deterministic fixture.
6. Review every image at 100% and 200% scale before replacing files.
7. Confirm README and AppStream render the final assets.
8. Record the exact commit, fixture, session date, scale, and filenames in `docs/release/v14.0.0-checklist.md`.
