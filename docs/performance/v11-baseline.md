# v11 baseline for Reliability Core

Baseline commit: `13dc09a45abd813e46d4d8f0cb2c524047d1f2a6`  
Canonical target: Fedora KDE 44 / Plasma 6  
Measurement date: 2026-07-13

The repository baseline is reproducible with:

```bash
python3 scripts/demo/generate_reliability_fixtures.py
```

This creates deterministic 1k, 10k, and 100k observation databases plus every
required provider-state fixture under `.demo-output/reliability-v12/`.

Runtime measurements requiring a clean Plasma session must be recorded on the
canonical VM before RC promotion. They are deliberately not inferred from unit
tests or the developer workstation:

| Measurement | v11 baseline | v12 target |
| --- | ---: | ---: |
| Cold `plasmawindowed` startup | pending canonical VM | no regression |
| Warm popup-open latency | pending canonical VM | at least 20% less work/network activity |
| Idle CPU/RSS over 15 minutes | pending canonical VM | no sustained growth |
| Idle KWallet reads | periodic reads observed in source | 0 |
| Fresh popup HTTP requests | unconditional refresh observed in source | 0 |
| Startup refreshes | duplicate scheduling risk observed in source | exactly 1/provider |
| 100k grouped observation query | 0.06 s / 8,760 KiB on developer workstation | responsive, canonical VM result required |

Do not replace the pending VM entries with workstation estimates. Record the VM
hardware, image checksum, Plasma version, installed package version, raw journal,
request counters, and timing commands alongside the final values.
