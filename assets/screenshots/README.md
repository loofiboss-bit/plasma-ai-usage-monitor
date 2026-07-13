## Screenshot Playbook

This directory holds the canonical product screenshots used by the README and AppStream metadata.

## Stable filenames

Keep these filenames stable unless you also update every consumer:

- `main-window.png`
- `panel-view.png`
- `settings-view.png`
- `provider-intelligence.png`

The current README and AppStream metadata already reference those names.

## Required replacement shots

### `main-window.png`

Capture the expanded popup with:

- all v13 launch-provider cards visible
- actual spend and unavailable values labeled without fabricated zeroes
- clean summary area
- no errors, secrets, or placeholder-looking values

### `provider-intelligence.png`

Capture at least one expanded provider card with:

- typed usage and cost source chips
- an actual gateway spend value when the fixture provides one
- a connectivity-only provider showing `Unknown` for unavailable metrics
- last-success and last-attempt information

### `panel-view.png`

Capture the compact panel representation with:

- the widget seated in a clean Plasma panel
- readable badge or count state
- no unrelated noisy widgets stealing attention

### `settings-view.png`

Capture a polished configuration state with:

- provider setup or budget fields visible
- no exposed API keys
- realistic values that explain how the widget is configured

## Additional recommended shots

Prepare these for KDE Store even if they are not yet wired into README:

- history or compare analytics view
- subscriptions view with at least two useful cards
- optional budget or alert configuration view

## Capture rules

- use a real Fedora KDE session, not an image mockup
- keep wallpaper calm and non-distracting
- use consistent scale, theme, and panel placement across the set
- remove personal names, local hostnames, and private sessions
- prefer data-rich but readable states over dense walls of cards
- crop tightly enough to showcase the widget, but leave enough surrounding UI to feel native

## Replacement checklist

1. launch the widget in the Fedora KDE demo VM
2. start `python scripts/demo/mock_ai_usage_server.py`
3. run Plasma with the demo flag: `PLASMA_AI_MONITOR_DEMO=1 plasmashell --replace &`
4. capture the four canonical shots plus any optional store extras
5. review images at 100% scale before replacing files in this directory
6. confirm README and AppStream still render the updated assets correctly
