# v26 performance evidence

The baseline is the immutable v25.0.1 source archive (`f71d9d685e6830ae59cf63f04daa3bc518923145`). The candidate is an uncommitted source tree. Measurements record the product source SHA-256 and compiled module SHA-256 before and after the run, and reject a changed source/module. A Git HEAD identifier alone does not identify the candidate.

## Conditions and isolation

Fedora 44, Qt 6.11.2, Plasma 6.8.2, GCC 16.2.1. Baseline configured with Ninja, `CMAKE_BUILD_TYPE=Release`, `BUILD_TESTING=ON`, built with `-j2`. The baseline source was extracted into `/tmp/ai-monitor-v26-baseline`; neither the v25 checkout nor the host Plasma installation was modified.

The panel harness creates a private D-Bus session and headless virtual KWin output. HOME and every XDG writable directory point to temporary directories. Installation is under a temporary prefix. Providers use the loopback mock server; the release updater is suppressed in the measurement copy. The actual host panel, providers, credentials and personal history are outside the measurement profile. The probe addresses the exact isolated plasmashell PID through AT-SPI and records CPU ticks and RSS from `/proc/PID` once per second. CPU percentage means one logical CPU core. CPU/RSS cover the entire isolated plasmashell process and its threads, including Qt/Plasma overhead. RSS is resident memory, with shared library pages included. Idle checks hold the popup closed for 900 seconds, then open for 900 seconds, using the same process. Closed state is measured before first opening; the open state is measured after its first rendered frame.

The full popup comparison retains three warmups and twenty recorded samples per variant in alternating AB/BA order. The per-second 900-second measurements use one process per variant, separate from that popup distribution. These are virtual-session measurements; physical desktop, GPU, power and accessibility qualification remain separate and unmeasured here.

## CostEngine parser experiment

Only digit validation in `parseScaled` changes: the two per-call regular expressions become a small ASCII loop. All conversion, sign handling, complete fractional validation, truncation, overflow handling and rounding remain in place. The helper preserves the original regular expression's single final-LF allowance, including the unusual accepted `1\n.2` input.

`v26-cost-parse-repeat.json` records the final candidate loop versus the baseline implementation extracted verbatim from the archive. Eight alternating rounds of 10,000 typical string rate parses, after two warmup rounds, measured a median **406.34 ms** for the original and **10.12 ms** for the candidate: **97.51% less parser time**. A preceding independent repeat measured 97.66%. These measurements ran while compilation was active, and are a parser microbenchmark rather than an application speedup. The 10% acceptance gate passed by a wide margin. **810 parity checks** compare both the complete 128-bit result and the success flag, including BMP Unicode digits in discarded fractional suffixes, invalid suffixes, integer overflow, exponents, floats, signed/empty decimals and line endings. Public CostEngine regression coverage is added to the existing catalog test target.

## Reproduction

```bash
mkdir -p /tmp/ai-monitor-v26-baseline
git archive v25.0.1 | tar -x -C /tmp/ai-monitor-v26-baseline
git rev-parse 'v25.0.1^{commit}' > /tmp/ai-monitor-v26-baseline/.measurement-commit
cmake -S /tmp/ai-monitor-v26-baseline -B /tmp/ai-monitor-v26-baseline/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build /tmp/ai-monitor-v26-baseline/build/release -j2 --target aiusagemonitorplugin test_phase7_performance test_localsessionmodel
python3 scripts/tools/benchmark_cost_parse.py --source /tmp/ai-monitor-v26-baseline/plugin/costengine.cpp --iterations 10000 --output docs/performance/v26-cost-parse-repeat.json
python3 scripts/tools/measure_scale_tests.py --build-dir /tmp/ai-monitor-v26-baseline/build/release --runs 5 --output docs/performance/v26-baseline-scale.json
python3 scripts/tools/benchmark_session_import.py --build-dir /tmp/ai-monitor-v26-baseline/build/release --runs 5 --output docs/performance/v26-baseline-import.json
python3 scripts/measure_phase7_panel.py --baseline-build-dir /tmp/ai-monitor-v26-baseline/build/release --baseline-commit v25.0.1 --variant baseline --runs 1 --warmups 0 --idle-seconds 900 --output docs/performance/v26-baseline-runtime.json
# After candidate source freeze and Release build:
python3 scripts/measure_phase7_panel.py --baseline-build-dir /tmp/ai-monitor-v26-baseline/build/release --baseline-commit v25.0.1 --candidate-build-dir build/release --variant candidate --runs 1 --warmups 0 --idle-seconds 900 --output docs/performance/v26-candidate-runtime.json
python3 scripts/measure_phase7_panel.py --baseline-build-dir /tmp/ai-monitor-v26-baseline/build/release --baseline-commit v25.0.1 --candidate-build-dir build/release --runs 20 --warmups 3 --output docs/performance/v26-panel-comparison.json
python3 scripts/tools/measure_scale_tests.py --build-dir build/release --runs 5 --output docs/performance/v26-candidate-scale.json
python3 scripts/tools/benchmark_session_import.py --build-dir build/release --runs 5 --output docs/performance/v26-candidate-import.json
```

The scale harness repeats existing deterministic 100,000-row History/Analyst queries, local-session queries, and 100,000-event export tests. The recorded Qt test wall times include fixture creation and assertions; only `PHASE7_METRIC` times isolate History/Analyst query execution. They must not be described as file-import timings.

## Tool verification

Python compilation, Ruff checks (`--ignore EXE001`, matching existing non-executable script conventions), CLI help, archived baseline identity repeatability, p95 interpolation, and live process RSS/tick parsing passed. An independent temporary C++ executable compiled the modified CostEngine and passed 16 public-API validation/rounding cases. Runtime, repeated scale fixture and candidate results are recorded in the corresponding JSON artifacts when completed; an absent artifact is not a passing measurement.

## Initial scale sample under concurrent compilation

`v26-baseline-scale.json` preserves five baseline runs per fixture. Session queries passed 5/5 (median whole-test 926.96 ms); event export passed 5/5 (5427.89 ms). History/Analyst passed 3/5; successful 100k History timings were 785/1176/1395 ms, and Analyst 405/635/587 ms. Two runs exceeded existing Release budgets (Analyst 1832 > 1000 ms; History 2996 > 2000 ms). This was a noisy preliminary baseline while other builds ran, and provides no candidate regression verdict. A quiet comparison is required before making such a claim.

## Initial real import baseline

`v26-baseline-import.json` records five successful public-API Codex imports of a synthetic 19,233,433-byte JSONL file. Every run independently verified exactly 100,000 stored usage events; every run completed in one import cycle. The median was **21,242 ms** from enabling the tool through import completion and SQL count verification, excluding file generation. Load averages were around six during concurrent compilation; this preliminary baseline is not an isolated CPU comparison. The probe links the existing Release model/parser/schema objects, creates a fresh private data profile per run, and never reads the user's session files.

## Host transition and current qualification

The session resumed on 2026-11-05 after a host transition from Fedora 44 to Fedora 45. Qt/Plasma/GCC package builds now carry `fc45`, and protobuf is 6.33.5. All files without a date suffix above retain the original November 1 measurements. They are historical evidence and must not be used as the baseline for a November 5 candidate speedup claim. Fresh, same-host baseline/candidate measurements use the suffix `-2026-11-05.json` and include current OS/package metadata. Stale generated protobuf files were regenerated only in the build directory. The baseline archive was recreated from the exact tag because `/tmp` had been cleared.

## Fresh-popup network accounting

The November 1 long baseline profile's `fresh_popup_network_requests=4` was mislabeled: its first opening occurred after 900 closed seconds. The scheduler uses at least 900 seconds while closed and a shorter interval while open, and refreshes stale sources on opening. The measurement was a real request-count delta around that delayed opening, rather than cumulative boot traffic, but it did not establish a fresh-cache request budget. Its `passed=true` indicated successful measurement only.

The final harness waits for the fixed demo's four boot requests and a full second without any new request before timing a fresh popup. It stores the pre-open and post-open counts and evaluates their delta against the unchanged zero-request gate in the ordinary ABBA run. Long idle profiles record `post_idle_popup_network_requests` separately and leave `fresh_popup_network_requests` null; their successful execution does not evaluate popup acceptance. The first opening after 15 minutes is intentionally outside the fresh-cache test. A delayed-request fixture verified that boot settling waits for all four requests and the complete quiet window.
