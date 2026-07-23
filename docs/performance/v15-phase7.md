# v15 Phase 7 Performance and Accessibility Evidence

Date: 2026-07-23

This report records the v15 Phase 7 gates on the canonical Fedora 44 KDE
environment. Measurements use an isolated configuration and temporary install
prefix; they do not replace or modify the active widget installation or user
settings.

## Environment

- Fedora 44, Linux 7.1.4-204.fc44.x86_64
- KDE Plasma 6.7.3
- Qt 6.11.1
- Release build, GCC 16.1.1
- Installed comparison build: v14.1.1 RPM
- Candidate source version: v14.1.2

`protobuf-compiler` and `protobuf-devel` were installed from the Fedora
repositories to satisfy the checked-in Release build contract.

## Database and Async Work

The deterministic fixture contains cost observations at one-minute intervals.
The Release gate uses the measured candidate results as explicit regression
budgets.

| Observations | History | History budget | Analyst | Analyst budget |
| ---: | ---: | ---: | ---: | ---: |
| 1,000 | 9 ms | 150 ms | 31 ms | 250 ms |
| 10,000 | 114 ms | 500 ms | 81 ms | 700 ms |
| 100,000 | 899 ms | 2,000 ms | 303 ms | 1,000 ms |

History now performs bounded SQL pre-aggregation for cost and request series.
The existing semantic, currency, quality, sample-count, first-observation, and
last-observation contracts remain covered by the History tests.

After deliberately superseding History and Analyst requests:

- pending workers: 0;
- database connections before and after: 1;
- only the newest request IDs reached the UI.

## Runtime

All timings are wall-clock measurements from the same Fedora session. Startup
is the time until KWin exposes the `plasmawindowed` window. Ready is the time
until the requested view appears in the AT-SPI tree.

| Surface | Runs | Result |
| --- | ---: | ---: |
| v14.1.1 installed startup | 3 | 388 ms median |
| v15 candidate startup | 3 | 379 ms median |
| Candidate History ready | 3 | 760 ms median |
| Candidate Analyst ready | 3 | 648 ms median |
| v14.1.1 first panel popup | 1 | 77 ms |
| v15 candidate first panel popup | 1 | 69 ms |
| v14.1.1 warm panel popup | 1 | 74 ms |
| v15 candidate warm panel popup | 1 | 78 ms |

The candidate startup median is 9 ms faster than the installed v14.1.1
baseline on the same machine. The candidate first popup is 8 ms faster and its
warm popup is 4 ms slower. Both panel measurements are from an isolated real
`plasmashell` panel driven through AT-SPI, not from `plasmawindowed`.

Environment-dependent release thresholds are derived from the measured
v14.1.1 results:

- median `plasmawindowed` startup must remain at or below 427 ms (baseline plus
  10%);
- first and warm panel popup must remain at or below 100 ms (the measured
  74–77 ms baseline rounded up for compositor variance).

The 15-minute candidate overview run recorded:

- startup: 473 ms;
- idle CPU: 0.174%;
- RSS: 176,112 KiB initially and 225,276 KiB after 900 seconds;
- total RSS growth: 49,164 KiB.

A separate 15-second candidate run accounted for 43,420 KiB of startup/lazy
loading growth. Total growth in the 15-minute run exceeded that short-run
growth by 5,744 KiB; there was no worker or database-connection retention.

## Refresh and Network Invariants

The isolated live panel and mock server recorded:

- v14.1.1 startup: 4 requests; candidate startup: 4 requests;
- v14.1.1 fresh popup: 0 requests; candidate fresh popup: 0 requests.

The four demo sources therefore receive exactly one startup request each. The
Phase 7 QML gate separately uses a counting backend to validate policy:

- opening a fresh popup issues 0 provider refreshes;
- opening a stale popup issues exactly 1 provider refresh with the popup
  reason;
- the startup coordinator marks the initial refresh before reacting to wallet
  changes and restricts wallet-open refreshes to credential-backed providers,
  preventing a wallet-open event during startup from forcing a second refresh
  for unrelated sources.

Scheduled refresh endpoints remain read-only and the existing
`non_invasive_monitoring` release gate rejects inference or mutating endpoints.

## Accessibility and Visual Resilience

Automated QML, contract, and offscreen gates validate:

- keyboard focus for primary History controls and all top-level actions;
- explicit names for setup, refresh, configuration, tab, History, and Analyst
  actions;
- source-aware critical/warning screen-reader summaries;
- text/symbol distinctions in addition to status color;
- chart summaries with series names, recorded points, and gaps;
- narrow cards with long localized source names;
- 100%, 125%, 150%, and 200% scale;
- Breeze Light, Breeze Dark, and a high-contrast fixture;
- overview, History, Analyst, onboarding, missing-plugin, and plugin-recovery
  states.

## Reproduction

```bash
just phase7-check
python3 scripts/measure_phase7_runtime.py \
  --mode candidate --build-dir build/release --view overview \
  --runs 1 --idle-seconds 900
python3 scripts/measure_phase7_panel.py \
  --mode candidate --build-dir build/release --runs 1
```

The runtime commands require a live KDE Plasma Wayland session. The panel
command creates an isolated nested Plasma session and does not alter the active
desktop layout.
