# AI Usage Monitor v14.0.0 — First Successful Use

**Status:** Proposed  
**Target baseline:** `main` at `0c31caaf1087914950a950c86667594a47356c9c` (`07a9a7ddd2be66e937a13d7d93433dd98062236c` is its runtime-identical parent)
**Current stable:** `13.0.0`  
**Primary audience:** Plasma 6 users, with Fedora COPR as the supported complete package

## Outcome

v14 should turn installation and first launch into one reliable path to verified value.

A new user must never receive a blank or unexplained widget because the native plugin is missing or mismatched. After the runtime is ready, the product should help the user choose one useful source, configure only the required fields, run a safe read-only verification, and explain whether the result is actual usage, provider-reported spend, an estimate, a balance, local activity, or connectivity only.

The release should optimize for a first successful source, not for a larger provider count.

## Current state

### Strong foundations to preserve

- v13 has a typed refresh lifecycle, nullable Metric Contract v2, SQLite schema v4, KWallet-backed secrets, catalog-driven provider contracts, read-only scheduled requests, local history, diagnostics, COPR packaging, reproducible release artifacts, and 29 passing tests in the v13 release evidence.
- The repository has separate build/test, QML, static, packaging, and sanitizer workflows.
- The README and user guides now state provider limitations and the COPR/KDE Store split clearly.
- There are no open issues or pull requests at the planning baseline.

### User-facing gaps found in the current implementation

1. **The first-run flow is informational, not operational.** `OnboardingFlow.qml` contains four static pages and then opens Settings. It does not select a source, save a credential, verify a connection, or confirm a useful result.
2. **KDE Store installation can fail before guidance is visible.** `main.qml` imports the native QML module at parse time while the Store package is frontend-only. A missing plugin can therefore prevent the widget from rendering the recovery instructions it needs to show.
3. **Secret cancellation semantics are unsafe and surprising.** `configProviders.qml`, `configSubscriptions.qml`, and `configAlerts.qml` write dirty KWallet fields from `Component.onDestruction`, so closing or cancelling the settings page can still persist changed or cleared credentials.
4. **Provider setup is too dense.** `configProviders.qml` is 1,139 lines and presents the provider overview plus detailed forms for the complete catalog in one long page. Users must understand provider names and capabilities before choosing a useful source.
5. **General settings expose implementation detail too early.** Per-provider refresh sliders are always shown for the complete catalog. The four v13 descriptor providers have refresh keys in `main.xml` but no matching `cfg_*RefreshInterval` properties in `configGeneral.qml`.
6. **The “Local-First” preset is misleading.** It enables Ollama Cloud rather than a detected local coding tool, disables only three API providers, and does not guarantee a local-only result.
7. **Connection is presented too much like useful monitoring.** The main status reports connected providers even when most sources only prove model discovery or endpoint availability. Of 18 providers, only OpenAI, OpenRouter, and LiteLLM currently report actual usage/spend or key/gateway usage; DeepSeek reports balance; 14 are connectivity-only.
8. **Provider cards repeat and overexpose technical state.** The current v13 capture shows duplicate “Actual usage” badges for LiteLLM and several expanded cards dominated by `Unknown` values.
9. **Some Diagnostics actions are not viable for installed users.** Shell commands are passed to `Qt.openUrlExternally`, installed script paths are assumed, and the release-checklist action points at a repository-relative document that is not part of the installed plasmoid.
10. **The media set is not fully current.** The Analyst screenshot uses an older navigation and older model examples while the main v13 screenshots use the current Overview layout.
11. **QML validation is mainly lint/import/smoke coverage.** The backend and release contracts are strong, but onboarding, Apply/Cancel behavior, missing-plugin recovery, focus order, and user-visible state semantics do not have direct behavioral coverage.

## Top priority

**First successful use: install readiness → source choice → safe verification → truthful result.**

This outranks new providers, more analytics, a broad visual rewrite, and multi-distribution packaging. The app already has substantial capability. The largest remaining product risk is that a new user cannot reach or understand that capability without knowing the architecture first.

## Scope

### Included

- graceful missing/mismatched native-plugin recovery;
- correct Apply/Cancel handling for secrets;
- a shared source-readiness model for providers and local tools;
- actionable first-run setup with a verified outcome;
- provider settings restructured around source choice and one selected detail view;
- status and card semantics based on useful data rather than connectivity alone;
- actionable native diagnostics and a stronger redacted support report;
- keyboard, focus, screen-reader naming, scaling, and narrow-popup checks for changed surfaces;
- accurate v14 documentation, AppStream/KDE Store copy, screenshots, and release validation.

### Explicitly excluded

- new AI providers or inference features;
- a hosted backend, accounts, telemetry, or cloud sync;
- schema v5 or history/Analyst expansion unless a correctness defect requires it;
- Flatpak revival, AppImage, Windows/macOS ports, or a D-Bus service rewrite;
- broad multi-distro packaging in the same release;
- a visual redesign unrelated to the first-success and daily-monitoring flows;
- runtime scraping of prices or provider pages;
- treating connectivity as usage, treating unknown as zero, or combining currencies.

## Phase 0 — Lock the baseline and fix stop-ship interaction defects

### Objective and user value

Remove correctness traps before restructuring the experience. Cancelling Settings must not change credentials, and every shipped provider setting must have a valid configuration binding.

### Likely components

- `package/contents/ui/configProviders.qml`
- `package/contents/ui/configSubscriptions.qml`
- `package/contents/ui/configAlerts.qml`
- `package/contents/ui/configGeneral.qml`
- `package/contents/ui/configBudget.qml`
- new shared `package/contents/ui/SecretChangeSet.qml`
- `plugin/secretsmanager.*`
- `plugin/tests/`
- `package/contents/ui/configDiagnostics.qml`
- new `scripts/check_kcm_contracts.py`

### Tasks

1. Record the baseline SHA, current 29-test result, QML smoke result, and package payload result.
2. Replace every destruction-time credential write on Providers, Subscriptions, and Alerts with a shared staged transaction that commits additions, replacements, and removals only from the Plasma page-level `saveConfig()` hook defined by the [Plasma configuration contract](https://develop.kde.org/docs/plasma/widget/configuration/).
3. Ensure Cancel and window close discard pending secret edits without modifying KWallet.
4. Make synchronous [KWallet writes and removals](https://api.kde.org/kwallet-wallet.html) return success to QML, reject writes while the wallet is closed, retain failed staged entries for retry, and expose inline localized feedback.
5. Add the missing LiteLLM, Cerebras, Fireworks, and Perplexity refresh and notification bindings plus LiteLLM daily/monthly budget bindings. Keep `main.xml` unchanged; it already contains the keys.
6. Replace Diagnostics shell-as-URL actions with an HTTPS troubleshooting link and a clipboard-only version command.
7. Add Qt Quick regression coverage for replacement, removal, multiple edits, failed writes, repeated Apply, Cancel/discard, and window destruction. Add a KCM/catalog contract check rather than changing the existing portability check.

### Acceptance criteria

- Editing or clearing a key and pressing Cancel leaves the stored secret unchanged.
- Apply writes exactly the intended secret change once and reports success or failure.
- Failed mutations remain staged for retry; a second unchanged Apply performs no mutation.
- Every provider descriptor resolves existing configuration keys and explicit static KCM bindings for its enabled/model, refresh, notification, and advertised cost-budget settings.
- No Diagnostics button attempts to execute an arbitrary command through URL handling.
- Existing configuration, history, and KWallet entries survive an upgrade from v13.

### Verification

```bash
just build-debug
ctest --preset debug --output-on-failure
just check
just qml-lint
PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py --strict --build-dir build/debug --expected-version "$(< VERSION)"
python3 scripts/check_package_payload.py
QT_QPA_PLATFORM=offscreen dbus-run-session -- bash scripts/smoke_test_plasmoid.sh build/debug
```

### Checkpoint

Commit after the tests pass and Settings remains runnable. Suggested commit: `fix(settings): make secret edits transactional`

## Phase 1 — Add a dependency-safe installation bootstrap

### Objective and user value

Make frontend-only Store installs, missing native plugins, and version mismatches understandable and recoverable instead of blank or broken.

### Likely components

- `package/contents/ui/main.qml`
- a new dependency-free bootstrap component
- the current native root moved behind a delayed `Loader`
- `package/metadata.json`
- `scripts/smoke_test_plasmoid.sh`
- package payload checks

### Tasks

1. Prototype a pure-QML bootstrap that does not import `com.github.loofi.aiusagemonitor` at parse time.
2. Load the native monitor implementation only after the module is available.
3. Treat a failed module load as a missing or unusable plugin. Distinguish older/newer versions only after the plugin loads successfully.
4. Provide a Fedora COPR copy-command, a source-install documentation link, installed/frontend version details when available, and an explicit instruction to restart Plasma or log out and in after installation.
5. Keep the Store package honest: it remains a frontend package and never claims that the native dependency is bundled.
6. Add smoke fixtures for no plugin, matching plugin, older plugin, and newer plugin.

### Acceptance criteria

- A Store-only installation renders a recovery screen without QML import failure.
- A matched COPR/source installation reaches the normal widget with no extra prompt.
- A successfully loaded mismatched version names both versions and gives one safe next action.
- Installing the required plugin and restarting Plasma or logging out and in reaches the normal widget without deleting user data; same-engine Retry is not required because failed imports remain cached by `QQmlEngine`.

### Verification

```bash
python3 scripts/qml_lint.py --build-dir build/debug
PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py --strict --build-dir build/debug
dbus-run-session -- xvfb-run -a bash scripts/smoke_test_plasmoid.sh build/debug
```

Run additional frontend-only and mismatched-plugin smoke modes.

### Checkpoint

Suggested commit: `feat(bootstrap): recover from missing native plugin`

## Phase 2 — Introduce one source-readiness contract

### Objective and user value

Give onboarding, Settings, Overview, and Diagnostics the same answer to: “Can this source provide useful data, and what must the user do next?”

### Likely components

- new `SourceReadinessModel` or equivalent C++/QML-facing model
- `plugin/providermanager.*`
- provider/subscription catalogs
- local tool monitors
- `plugin/CMakeLists.txt`
- focused unit tests

### Tasks

1. Define stable source states: disabled, unavailable locally, needs configuration, ready to verify, verifying, connected-connectivity-only, reporting-estimate, reporting-actual, degraded, and failed.
2. Expose source kind, monitoring level, required credential slots, local installation detection, enabled state, last verified time, safe verification capability, and a redacted next action.
3. Rank setup candidates: detected local tools first; actual usage/spend and gateway sources next; balance/connectivity sources after that.
4. Reuse existing Provider Metric Contract v2 and typed refresh errors. Do not infer readiness from localized error strings.
5. Never expose raw secrets, cookies, account IDs, or endpoint query data to the model.

### Acceptance criteria

- All 18 providers and 6 local tools appear exactly once with a deterministic readiness state.
- “Connected” remains distinct from “reporting useful usage/spend.”
- Authentication, permission, configuration, unsupported metric, stale data, and network errors produce different next actions.
- The same fixture produces the same state in onboarding, Settings, Overview, and Diagnostics.

### Verification

```bash
ctest --preset debug --output-on-failure
just check
```

Add table-driven tests for every state transition and catalog entry.

### Checkpoint

Suggested commit: `feat(setup): add unified source readiness model`

## Phase 3 — Replace onboarding with Guided First Success

### Objective and user value

Let a new user reach one verified source without navigating all Settings categories or understanding every provider.

### Likely components

- `package/contents/ui/onboarding/OnboardingFlow.qml`
- new focused onboarding step components
- `package/contents/ui/FullRepresentation.qml`
- `package/contents/config/main.xml`
- source-readiness model and SecretsManager transaction API

### Tasks

1. Start with a goal choice: track a detected coding tool, track API usage/spend, or connect a gateway/provider.
2. Recommend a detected local tool when it can produce immediate value. Otherwise prioritize sources that report actual data.
3. Show the monitoring level and expected result before enabling a source.
4. Ask only for required fields. Keep models, custom URLs, tiers, intervals, budgets, exports, and webhooks out of the first-success path unless essential.
5. Save credentials explicitly to KWallet with visible success/failure feedback.
6. Run only the existing safe read-only verification contract. Never run inference from onboarding.
7. Finish with a result screen that states the source and quality: actual, gateway-reported, balance, local estimate/activity, or connectivity only.
8. Persist completion only after a verified outcome. Support Skip, Resume, and Run setup again without losing existing configuration.

### Acceptance criteria

- A detected local tool can be enabled and verified in at most three primary actions.
- A provider can be selected, minimally configured, saved, and verified without leaving onboarding.
- A failed verification stays in context and gives a specific recovery action.
- Completion survives widget restart; Skip does not masquerade as successful setup.
- Existing v13 users with enabled sources bypass onboarding and keep all settings.

### Verification

- automated QML state tests for local-tool success, provider success, auth failure, KWallet failure, Skip, Resume, and restart;
- keyboard-only walkthrough;
- manual 100%, 125%, 150%, and 200% scale checks;
- narrow panel popup and standalone `plasmawindowed` checks.

### Checkpoint

Suggested commit: `feat(onboarding): guide users to a verified source`

## Phase 4 — Restructure Settings around source choice

### Objective and user value

Make later configuration predictable without exposing all 18 provider forms and all refresh sliders at once.

### Likely components

- split `configProviders.qml` into a source list, selected-source details, credential editor, and verification result components
- `configGeneral.qml`
- shared setup components reused by onboarding
- provider catalog descriptors

### Tasks

1. Replace the long provider page with a searchable master/detail or list/selected-detail layout.
2. Group or filter sources by “Usage & spend,” “Gateway,” “Balance,” and “Connectivity only.”
3. Put actual-data sources and detected local options before connectivity-only providers.
4. Show one selected source form at a time with its monitoring level, required permission, scheduled endpoint summary, and last verification result.
5. Keep custom base URLs, model overrides, tiers, and per-provider refresh intervals under Advanced.
6. In General, show per-source intervals only for enabled sources and only in Advanced mode.
7. Remove the misleading presets or redefine them as deterministic, previewable changes. “Local-First” must use detected local tools and must not enable Ollama Cloud.
8. Add Save and Verify actions with correct Apply/Cancel behavior and inline typed errors.

### Acceptance criteria

- Users see no more than one detailed provider form at a time.
- Search and monitoring-level filters preserve the selected item predictably.
- Default mode contains only settings required for ordinary use.
- Advanced mode contains every current capability without duplicating catalog truth.
- Applying a preset shows the exact changes before committing them.

### Verification

- QML lint and smoke;
- automated selection/filter/Apply/Cancel tests;
- manual keyboard focus-order and screen-reader name checks;
- existing configuration import/export compatibility test.

### Checkpoint

Suggested commit: `refactor(settings): focus provider setup on one source`

## Phase 5 — Make the dashboard outcome-first

### Objective and user value

Help users understand what the monitor actually knows at a glance, without reading repeated badges or expanded rows of unknown metrics.

### Likely components

- `package/contents/ui/views/OverviewView.qml`
- `package/contents/ui/components/StatusHeader.qml`
- `package/contents/ui/components/AttentionList.qml`
- `package/contents/ui/ProviderCard.qml`
- `package/contents/ui/CostSummaryCard.qml`
- `package/contents/ui/Utils.js`

### Tasks

1. Replace “N of N providers connected” as the primary health message with a source-quality summary: reporting actual data, reporting estimates/local activity, connectivity-only, and needs attention.
2. Do not say “All configured sources are ready” when no source provides the user’s intended metric.
3. Separate or collapse connectivity-only providers so they do not dominate the usage dashboard with `Unknown` token and cost fields.
4. Merge duplicate source badges. Use precise labels such as “Gateway usage” and “Provider-reported spend” when two different facts exist.
5. Hide incompatible metric rows instead of repeating `Unknown`; preserve one clear unavailable explanation and next action.
6. Show the cost summary only when compatible spend, estimates, or subscription fees exist. Never convert unknown to zero.
7. Add direct “Fix” actions for authentication, permission, missing local tool, stale data, and version mismatch states.
8. Normalize locale-aware currency spacing and compact-number formatting without combining currencies.

### Acceptance criteria

- The first viewport answers: what is reporting, what is only connected, what needs action, and what cost values mean.
- A connectivity-only source never appears to report zero usage or zero spend.
- LiteLLM does not show duplicate “Actual usage” badges.
- Cards remain readable at the normal 28-grid-unit popup size and 200% scaling.
- All changed interactive elements have logical focus, accessible names, and non-color state cues.

### Verification

- fixture screenshots for actual, estimate, balance, connectivity-only, mixed-currency, stale, error, and empty states;
- keyboard and screen-reader pass;
- QML lint/import/plasmoid smoke;
- regression checks for nullable metrics and mixed currencies.

### Checkpoint

Suggested commit: `feat(overview): prioritize useful source outcomes`

## Phase 6 — Make Diagnostics actionable and native

### Objective and user value

Turn Diagnostics into a recovery surface that works from an installed package and produces a support report capable of resolving the problem.

### Likely components

- `package/contents/ui/configDiagnostics.qml`
- `plugin/appinfo.*`
- source-readiness model
- install/version inspection helpers
- support-report tests

### Tasks

1. Replace repository-relative document actions with stable HTTPS documentation links.
2. Replace shell-launch actions with native version/install-layer checks or safe copyable commands.
3. Report frontend version, native plugin version/path, package/install layer, Plasma version, distribution, KWallet state, database health/size, catalog version, and redacted per-source readiness/error kind.
4. Detect user-local/system shadowing and provide a single safe next step without automatically deleting files.
5. Deep-link recovery actions to the relevant source or Settings page when Plasma APIs permit it.
6. Keep endpoint hosts, query strings, account/project identifiers, credentials, cookies, webhook URLs, and wallet contents out of reports.

### Acceptance criteria

- Every visible action works from a normal COPR installation without a repository checkout.
- The report distinguishes missing plugin, version mismatch, auth failure, permission failure, unsupported metric, database failure, and stale data.
- Redaction tests fail if representative secret-like values enter the report.
- Diagnostics never mutate installation layers or remove data without an explicit separate user action.

### Verification

```bash
just test
just check
just qml-lint
```

Add support-report snapshot/redaction tests and Fedora installed-package manual checks.

### Checkpoint

Suggested commit: `feat(diagnostics): add actionable native recovery`

## Phase 7 — Adoption assets and stable release

### Objective and user value

Publish a truthful, coherent v14 experience whose Store page, README, screenshots, packages, and runtime all describe the same product.

### Likely components

- `README.md`
- `ROADMAP.md`
- `CHANGELOG.md`
- `docs/user-guide/*`
- `docs/wiki/*`
- `com.github.loofi.aiusagemonitor.metainfo.xml`
- `assets/screenshots/*`
- release checklist and packaging metadata

### Tasks

1. Reframe product copy around verified provider usage/spend, coding-tool activity, truthful connectivity checks, and local-first privacy rather than the raw count of providers.
2. State the native-plugin requirement before the Store install action and describe the new in-widget recovery path.
3. Capture a coherent v14 media set from the same Fedora Plasma session: first-run source choice, verified success, Overview, one useful provider detail, Settings master/detail, History, Analyst, and panel state.
4. Remove or replace stale Analyst and provider screenshots. Check model examples and amounts against the deterministic demo fixture.
5. Update the AppStream summary, captions, release notes, and user guides.
6. Verify v13 configuration, history, and KWallet preservation through upgrade, rollback, re-upgrade, removal, and reinstall.
7. Publish only from the exact verified `main` commit and perform public readback for GitHub, COPR, KDE Store, checksums, SBOM, metadata, and screenshots.

### Acceptance criteria

- No public description implies that all 18 providers expose usage or billing.
- Every screenshot matches v14 navigation, terminology, and demo data.
- Frontend-only Store install, clean COPR install, v13 upgrade, rollback, and removal are verified.
- All release artifacts report 14.0.0 and are built from the same commit.

### Verification

```bash
just test
just check
just release-check
just fedora44-check
```

Also require:

- full QML behavioral suite;
- ASan/UBSan CTest;
- frontend-only and plugin-mismatch smoke;
- clean Fedora RPM lifecycle;
- AppStream and RPM validation;
- byte-identical source/plasmoid rebuilds;
- checksum and SPDX SBOM verification;
- real Plasma keyboard, scaling, and screenshot review.

### Checkpoint

Use one release-preparation commit after every prior phase is merged and green. Tag only the verified release commit.

## Release gate

v14.0.0 is complete only when all of the following are true:

- [ ] Cancel never writes or deletes KWallet data; Apply behavior is tested.
- [ ] Every catalog provider has valid settings and refresh bindings.
- [ ] Store-only, missing-plugin, matched-plugin, and mismatch states render correctly.
- [ ] A new user can verify one local tool or provider from onboarding without visiting unrelated settings.
- [ ] Onboarding reports the real data quality and never runs inference.
- [ ] Settings show one selected provider detail at a time and preserve v13 configuration.
- [ ] Overview separates actual, estimated/local, balance, connectivity-only, and error states.
- [ ] Unknown stays distinct from zero in UI, history, exports, alerts, and Prometheus.
- [ ] Diagnostics works from an installed package and support-report redaction tests pass.
- [ ] Keyboard, focus, accessible naming, narrow-popup, and 100–200% scale checks pass.
- [ ] Existing C++/catalog/release tests plus new QML behavior tests pass.
- [ ] Clean install, v13 upgrade, rollback, re-upgrade, removal, and user-data preservation pass.
- [ ] README, user guide, AppStream, KDE Store copy, screenshots, package metadata, and runtime all agree.
- [ ] GitHub, COPR, and KDE Store artifacts are verified by public readback from the exact tag commit.

## Risks and open decisions

1. **Missing-module bootstrap lifecycle.** A dependency-free root plus URL-based [`Loader`](https://doc.qt.io/qt-6.8/qml-qtquick-loader.html) can render recovery UI, but a failed import remains in the current [`QQmlEngine` component cache](https://doc.qt.io/qt-6.8/qqmlengine.html). Recovery after installation therefore requires restarting Plasma or logging out and in. Do not silently begin a D-Bus rewrite.
2. **KCM secret transaction hooks.** Plasma 6.7.3 calls a page-level `saveConfig()` and observes `unsavedChanges`; keep secret persistence on that lifecycle and cover it with a fake-store test.
3. **Verification semantics.** Reuse the v13 read-only refresh contract. A provider without a safe read-only endpoint may be configured but must not be called an onboarding success for usage monitoring.
4. **No telemetry.** v14 should not add analytics to measure the funnel. Use deterministic completion criteria, Store feedback, issues, and support-report quality as adoption proxies.
5. **Distribution ceiling.** COPR remains the only complete prebuilt package. After v14 proves the product flow, evaluate OBS/AUR/deb packaging as a separate version based on maintainer capacity and user demand.

## Recommended execution order

Implement one phase at a time. Commit only after the phase-specific tests pass and the repository is runnable. Do not combine Phase 1 bootstrap work with the onboarding redesign until the missing-plugin prototype is proven. Do not start Phase 7 media capture until UI strings and layouts are frozen.

## Codex handoff

```text
Inspect the repository and follow PLASMA_AI_USAGE_MONITOR_V14_PLAN.md. Start with Phase 0 and verify the current main baseline before changing code. Implement one phase at a time, preserve v13 configuration/history/KWallet data, and keep all scheduled or onboarding verification read-only. Run the phase-specific checks and stop on conflicting repository evidence, unsafe KWallet lifecycle behavior, or a failed release gate. After each phase, report the changes, verification results, remaining risks, and proposed commit message. Do not add providers or broaden distribution scope unless the approved plan is updated first.
```
