# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [13.0.0] — 2026-07-17

### Added
- Add Provider Metric Contract v2 with nullable values and source, quality, scope, window, observation, currency, and reset metadata.
- Add transactional SQLite observation schema v4 with a pre-v13 backup and compatibility projections for existing QML.
- Add Catalog v5 adapter profiles, a C++ `ProviderManager`, and descriptor-driven LiteLLM, Cerebras, Fireworks, and Perplexity integrations.
- Add Gemini read-only dynamic model discovery and a release-blocking non-invasive monitoring gate.

### Changed
- Replace scheduled OpenAI-compatible and Azure completion probes with read-only model discovery; billable inference tests are explicit manual actions.
- Move OpenRouter monitoring to `/api/v1/key` and retain native daily, weekly, monthly, remaining-limit, and reset fields.
- Stop presenting published Gemini and Veo caps as live remaining quota.
- Display unavailable provider metrics as Unknown and document scheduled traffic in Trust Center.
- Refresh the canonical KDE screenshots and deterministic demo fixtures around the four v13 launch providers.
- Make release validation fully reproducible without provider API keys, vendor accounts, KWallet entries, or billable probes.

### Fixed
- Include typed `usage_api` gateway spend in provider cards, monthly exposure, and compact panel totals while preserving unavailable costs as `Unknown`.
- Isolate demo and screenshot sessions from KWallet at the native secrets boundary and route descriptor adapters only to the deterministic mock endpoint.
- Align the OpenRouter demo fixture with the native v13 `GET /key` payload and gate all launch-provider demo routes with an executable contract test.
- Block stable publication unless the stable tag, annotated `rc.1` tag, and `origin/main` share one commit; no timed soak is required.
- Stabilize the DeepSeek probe regression test by waiting for its parallel read-only refresh to finish before starting a manual diagnostic.

## [12.0.3] — 2026-07-13

### Fixed
- Drive every provider model picker from Provider Catalog v4 so Settings no longer advertises stale or invalid Claude, Gemini, Groq, xAI, Ollama, OpenRouter, Cohere, or Veo model IDs.
- Keep model pickers editable for custom gateways and newly released models while using the reviewed catalog as the visible source of truth.
- Reject deprecated or retired provider defaults in the release gate and add a regression check requiring all 14 provider pickers to remain catalog-driven.

## [12.0.2] — 2026-07-13

### Added
- Add GPT-5.6 Sol, Terra, and Luna to the OpenAI catalog and move the OpenAI/OpenRouter defaults to GPT-5.6 Terra.
- Parse Codex's live 5-hour and weekly quota windows, including remaining percentages and reset timestamps, from the existing local Codex login.

### Changed
- Refresh current provider model IDs, prices, defaults, and lifecycle metadata for Anthropic, Google, Mistral, Groq, xAI, Ollama Cloud, OpenRouter, Together AI, Cohere, Google Veo, and AWS Bedrock.
- Prefer local Codex authentication for quota sync while retaining the browser-account request as a compatibility fallback.

### Fixed
- Snapshot Firefox's cookie WAL and shared-memory files so recently updated login cookies are visible while Firefox is running.
- Reassemble chunked Auth.js session cookies used by current ChatGPT sessions.
- Feed live Codex quota windows into subscription cards instead of displaying only static plan presets.

## [12.0.1] — 2026-07-13

### Fixed
- Restore native QML plugin discovery in Fedora/system installs by shipping the Linux module as `libaiusagemonitorplugin.so` instead of the build-only filename accepted by the previous smoke harness.
- Declare the tab delegate's required `modelData` property so the v12 popup renders without a Qt 6.11 `ReferenceError`.
- Make the strict QML import smoke use only the actual installed Linux plugin filename, preventing this packaging regression from passing CI again.

## [12.0.0] — 2026-07-13

### Added
- Add typed provider lifecycle, error, refresh reason, metric source and freshness contracts with deterministic retry scheduling.
- Add observation schema v3 with explicit period semantics, source, quality, model scope, correlation IDs and currency-aware queries.
- Add Provider Catalog v4, a C++ Browser Sync boundary, async history/export workers, reliability fixtures and split CI gates.

### Changed
- Coalesce provider refreshes, target configuration changes, cache KWallet reads and refresh stale providers on popup open.
- Group spend by ISO currency throughout the UI, notifications, Prometheus and Analyst views; mixed currencies are never silently summed.
- Split the popup into lazy Overview, History and Analyst views and migrate the native plugin to `qt_add_qml_module()`.
- Stream large JSON/CSV exports from forward-only SQL queries with atomic output files.
- Define COPR/source builds as full supported distributions and label the KDE Store plasmoid as frontend-only.

### Removed
- Remove the non-functional Flatpak scaffold, tracked v7 release tarball, periodic secret polling and error-text control flow.

### Release status
- Released through the v12.0.3 correctness line; historical RC verification is tracked in the release checklist.

## [11.0.0] — 2026-06-30

### Added
- Add DeepSeek v4 Flash and v4 Pro catalog entries and mark the legacy `deepseek-chat` and `deepseek-reasoner` compatibility aliases with the official 2026-07-24 deprecation note.
- Add v11 release checklist coverage for COPR Fedora 43/44 build verification, RPM install/update/remove smoke checks, and issue #12 follow-through.

### Changed
- Refresh Provider Catalog v3 and Subscription Catalog v1 to `catalogVersion: 2026.06.30`, `release: 11.0.0`, and `lastReviewed: 2026-06-30`.
- Refresh provider and subscription source references against official pricing/model/plan pages, keeping manual-review flags visible where exact pricing, route, region, unit, or rebrand semantics remain ambiguous.
- Make Fedora COPR/RPM installation the primary documented Fedora path.
- Switch the default DeepSeek model to `deepseek-v4-flash` while preserving compatibility aliases for existing configurations.

### Fixed
- Keep GitHub Copilot Auto billing mode visible after the 2026-06-01 AI credits transition so the UI and `subscription_tools` regression test agree.

### Known Metadata Uncertainty
- Provider manual-review entries remain for Anthropic, Google, Mistral, DeepSeek, Groq, xAI, Ollama Cloud, OpenRouter, Together AI, Cohere, Google Veo, Azure OpenAI, and AWS Bedrock.
- Windsurf remains a visible subscription source-conflict while public Windsurf, Devin, and Cognition-branded pages describe overlapping plan and quota semantics.

## [10.0.1] — 2026-05-12

### Fixed
- Import the AI Usage Monitor plugin module in `ProviderCard.qml` so the installed v10 provider cards can resolve `ProviderPricingCatalog` when rendering catalog review, source-conflict, and unknown-pricing badges.

## [10.0.0] — 2026-05-12

### Added
- Add source-aware provider state for actual usage, probe usage, cost source, usage source, currency, and data quality.
- Add source-aware SQLite snapshot columns, CSV/JSON export fields, Prometheus source labels, and Analyst summary confidence flags.
- Add `scripts/check_config_defaults_exist_in_catalog.py` to block provider default-model drift in `just check`, `just release-check`, and CTest.
- Add OpenAI Costs API object-amount, legacy numeric fallback, empty result, malformed JSON, retry-after, non-USD warning, and probe-isolation regression tests.

### Changed
- Parse OpenAI Costs API `amount.value` directly and warn on non-USD currencies; only legacy numeric mock data uses cents-to-dollars conversion.
- Treat OpenAI-compatible and Azure chat-completion health checks as connectivity probes, not user usage or spend.
- Rework first-run onboarding around monitoring goals, data level, quick presets, and source/data-quality review.
- Expand Diagnostics with wallet, Copilot token, catalog review, Browser Sync readiness, insecure URL, install-shadowing, and support-report actions.
- Separate dashboard, compact, Prometheus, and card labels for API spend, subscription fees, estimated burn, and monthly exposure.
- Make Copilot/Bedrock/webhook secret loading react to wallet state and delayed key availability, and make subscription plan changes apply without a Plasma reload.
- Refresh Provider Catalog v3 and Subscription Catalog v1 release metadata to 2026-05-12 while keeping remaining manual-review entries visible.

### Fixed
- Fix fresh-install defaults for OpenAI, Azure OpenAI, and AWS Bedrock so they resolve to shipped catalog model IDs.
- Remove hardcoded provider/tool alert threshold decisions from refresh and card logic so configured warning, critical, and budget thresholds drive UI and notifications.

### Known Metadata Uncertainty
- Provider manual-review entries remain for Anthropic, Google, Mistral, DeepSeek, Groq, xAI, Ollama Cloud, OpenRouter, Together AI, Cohere, Google Veo, and AWS Bedrock.
- Subscription manual-review/source-conflict entries remain visible for Windsurf and subscription catalog conflict tracking.

## [9.0.0] — 2026-05-08

### Added
- Add schema-v2 configuration export/import that covers every non-secret KConfig key while keeping API keys, browser cookies, PATs, and webhook URLs in KWallet.
- Add catalog review items to `CatalogLoader` so Diagnostics can show the provider or subscription entries that need manual review or have source conflicts.
- Add config-portability validation and stricter catalog-review reason checks to local, CTest, Fedora 44, and CI validation.

### Changed
- Refresh Provider Catalog v3 and Subscription Catalog v1 for the Confidence release with 2026-05-08 review dates and user-visible review/conflict reasons.
- Resolve JetBrains AI subscription metadata from source-conflict to official range-based pricing and quota labels.
- Expand Diagnostics with loaded plugin path, richer Trust Center catalog details, and clearer config portability copy.
- Refresh the roadmap, README, AppStream, and RPM metadata around v9.0.0 Confidence.

### Removed
- Remove the stale standalone `SetupWizard.qml` path so first-run activation has one maintained inline onboarding path.

## [8.0.3] — 2026-05-06

### Fixed
- Add automatic self-tracked fallback limits for subscription tools whose vendors expose plan allowances qualitatively or as multipliers but do not return live used/limit fields.
- Show Codex and similar local-activity monitors with progress bars while keeping the data source distinct from official live quota sync.

## [8.0.2] — 2026-05-06

### Fixed
- Preserve the existing Codex `Pro` plan position so legacy plan index `3` continues to resolve to ChatGPT Pro rather than the newly added Pro $100 tier.

## [8.0.1] — 2026-05-06

### Fixed
- Add official GitHub Copilot premium-request defaults for Free, Pro, Pro+, Business, and Enterprise so configured Copilot Pro+ shows its plan and monthly limit instead of falling back to Free with a zero cap.
- Add current ChatGPT Pro $100 and Pro $200 Codex usage metadata, including official usage multipliers where OpenAI publishes ratios instead of exact message counts.
- Keep configured Codex Pro tiers when ChatGPT browser sync returns an entitlement but no live quota fields, and surface a catalog-fallback sync status instead of silently downgrading to Plus.

## [8.0.0] — 2026-05-06

### Added
- Add Provider Catalog v3 and Subscription Catalog v1 as local source-of-truth JSON catalogs for model pricing, subscription plans, quota windows, source refs, review dates, precision, and manual-review/source-conflict states.
- Add `CatalogLoader`, `ProviderPricingCatalog`, and `SubscriptionPlanCatalog` plus QML catalog status for schema, catalog version, review date, runtime-scraping state, stale/valid state, and review/conflict counts.
- Add quota-window UI rows with readable source badges for synced, official, official range/approx/note, self-tracked, custom, estimated, needs-review, and unknown data.
- Add subscription catalog and hardcoded-pricing validation scripts and wire them into CTest, `just check`, `just release-check`, and `just fedora44-check`.

### Changed
- Move subscription plan defaults, quota presets, Copilot billing modes, and provider/model token pricing out of C++/QML defaults and into shipped catalogs.
- Add stable string plan ID config keys while keeping old integer plan indexes as migration fallbacks.
- Switch Copilot billing mode to `auto`, `premium_requests_legacy`, and `ai_credits_usage_based` with a date-aware 2026-06-01 helper.
- Replace fake token pricing for non-token Veo cost estimation with explicit video-second pricing metadata.

### Removed
- Remove the private server/API provider, compact KPI mode, demo endpoint, catalog row, and QML/C++ backend registration from the public app.

## [7.0.0] — 2026-04-30

### Added
- Add Fedora KDE 44 / Plasma 6.6 as the primary development, CI, demo, validation, and release target.
- Add `just fedora44-check` with actionable checks for Fedora version, Plasma tools, Qt6/KF6 dependencies, KWallet/libsecret, QML import paths, install shadowing, AppStream, rpmlint, source packaging, plasmoid packaging, and payload validation.
- Add Provider Catalog v2 static metadata plus `scripts/check_provider_catalog.py` to keep provider/model/pricing metadata maintainable without runtime scraping.
- Expand Diagnostics into a Trust Center for actual API usage, estimated cost, rate-limit-only data, local filesystem-derived data, Browser Sync Labs, catalog freshness, KWallet status, and provider health.
- Add Simple / Operator / Analyst popup modes, show-only-problems filtering, direct empty-state actions, adaptive refresh scheduling, and visible refresh/failure/database diagnostics.

### Changed
- Move CI to `fedora:44` and validate source/plasmoid package checks in CI.
- Generalize Fedora KDE demo setup to `scripts/demo/setup_fedora_kde_test_env.sh --fedora 44`.
- Update Copilot tracking language and configuration for premium requests, usage-based billing, credits mode, and configurable monthly reset assumptions.
- Improve Browser Sync Labs readiness for Firefox, Chrome, Chromium, Brave, KWallet/libsecret safe storage, cookie DB readability, and safe local-estimation fallback messaging.
- Keep v7.0.0 explicitly Plasma-native and local-first; backend services, web dashboards, PostgreSQL, and multi-user features remain out of scope.

## [6.0.1] — 2026-04-26

### Fixed
- Fixed version drift across the repository (updated to 6.0.1).
- Ensured `plugin/CMakeLists.txt` is consistent with `qmlRegisterType` calls in `plugin/aiusageplugin.cpp`.
- Updated documentation and roadmap to reflect 6.0.1 stabilization release.

## [6.0.0] — 2026-04-23

### Added
- **First-Run Guided Setup:** A simplified onboarding flow for new users, offering a "Recommended" path to quickly connect top providers.
- **Basic / Advanced Settings Mode:** Hides complex configurations (like custom base URLs, tier selection, deployments) behind an Advanced Mode toggle to reduce visual clutter for everyday users.
- **Data Trust & Confidence Labeling:** Provider cards now clearly indicate the type of data being displayed (Actual, Estimated, Rate-limit only, Local, Subscription-plan derived) and health status (Healthy, Degraded, Disconnected).
- **Diagnostics Center:** A dedicated config tab showing KWallet health, Browser Sync readiness, installed CLI tools, and quick access to run the deep system doctor script.
- **Config Portability:** Export and import non-secret configurations to share presets or backup UI settings safely.
- **Configuration Presets:** Quick one-click setups for common scenarios: Solo Developer, Multi-Provider, Local-First, Budget Watch, and Local Operator.
- **Compact Panel Upgrades:** Pin new key metrics directly in the compact Plasma panel view: Daily Cost, Remaining Requests, or Most Critical Provider.
- **Analyst & History Redesign:** Refined history controls, updated compare mode to look like a first-class view, and improved ranking panels in TrendSummary.
- **Config Screen Polish:** Updated all configuration screens to follow the new v6 visual language with improved spacing, grouping, and context hints.

### Changed
- **Dashboard Actionability:** Replaced static dashboard summaries with actionable insights: Top Risk, Budget Left Today, and Next Reset.
- **Browser Sync to Labs:** Explicitly marked Browser Sync as an experimental Labs feature to set correct reliability expectations.

## [5.4.1] — 2026-04-22

### Fixed
- Fix `checkToolInstalled` mock behavior in `test_subscription_tools.cpp` to correctly handle `chroot` environment checks during COPR/RPM build validation.

## [5.4.0] — 2026-04-22

### Added
- Add support for April 2026 models including OpenAI GPT-5.4 series (gpt-5.4-pro, gpt-5.4-thinking, gpt-5.4-mini, gpt-5.4-cyber), Anthropic Claude 4.7/4.8, Google Gemini 3.1 & Deep Research, and Gemma 4 31b.
- Add deep environment preflight checks in `install_doctor.sh` covering dependencies, SQLite driver, KWallet health, compiled plugin integrity (`ldd`), and Flatpak/Snap browser profile discovery.
- Add AWS Bedrock region validation and improved 403 error reporting for missing IAM permissions.

### Changed
- Update default OpenAI and Azure OpenAI models from `gpt-4o` to `gpt-5.4-pro` across the UI, config schema, OpenRouter integration, and test suite.
- Improve Browser Sync by expanding profile discovery to support Flatpak/Snap installations of Firefox, Chrome, Chromium, and Brave.
- Optimize LocalActivityMonitorBase to reduce CPU overhead on large watched directories by checking top-level folder timestamps before deep scanning.
- Refactor `CopilotMonitor` to utilize the improved `LocalActivityMonitorBase` for consistent activity tracking and debouncing.
- Prevent duplicate counting in local subscription monitors by implementing a 1.5s logical grouping window for rapid filesystem events (like "Save All").

## [5.3.0] — 2026-04-10

### Added

- Add local filesystem-backed subscription monitors for Cursor, Windsurf, and JetBrains AI
- Add reusable local activity monitor infrastructure with install detection, watched-path debounce, and self-tracked increments
- Add AWS Bedrock provider scaffolding with AWS Signature Version 4 request signing support
- Add Linux Chrome/Chromium browser profile discovery and cookie sync support for experimental browser sync
- Add local Prometheus metrics export, Slack/Discord webhook alert delivery, and scheduled JSON/CSV history export
- Add GitHub Actions CI validation for Flatpak scaffold checks, version consistency, build, and tests

### Changed

- Replace the roadmap with the revised Vanguard, Link, and Nexus (Light) release plan
- Extend browser sync messaging and configuration to cover Firefox, Chrome, and Chromium on Linux
- Update release-facing README, AppStream, and configuration surfaces for the expanded provider, tool, export, and notification support
- Refactor Claude Code and Codex CLI monitoring onto shared local activity infrastructure
- Keep the project on the desktop-widget architecture without introducing PostgreSQL, multi-user storage, or a separate team backend

## [5.2.0] — 2026-04-09

### Added

- Add `ProviderCatalog.qml`, `ProviderRegistry.qml`, `NotificationController.qml`, `RefreshScheduler.qml`, and `RuntimeCoordinator.qml` to split provider metadata, notification routing, refresh scheduling, and startup wiring out of `main.qml`
- Add `OpenAICompatibleProviderSection.qml` for reusable OpenAI-compatible provider settings in the KCM
- Add `scripts/dev_smoke_check.sh` and a matching `just smoke` recipe for local install-shadowing and stale-plugin diagnostics
- Add delayed-reply stale-generation regression coverage for `OllamaCloudProvider` in mocked HTTP tests

### Changed

- Refactor `configProviders.qml` to reuse shared OpenAI-compatible forms for Mistral, DeepSeek, Groq, xAI, Ollama Cloud, OpenRouter, Together AI, and Cohere
- Refactor `configAlerts.qml`, `configBudget.qml`, and `configGeneral.qml` around shared provider catalog metadata so provider labels, refresh intervals, budgets, and notification toggles stay aligned
- Rework the Live popup with a `Needs Attention` strip and remembered manual expansion for provider and tool cards so attention items stay open while healthy cards default-collapsed
- Slim down `package/contents/ui/main.qml` into a composition root centered on backend instantiation and shared wiring
- Expand install/version diagnostics to report plasmoid shadowing, compiled QML module presence, and missing-plugin failure modes
- Refresh contributor and planning docs to reflect the current local test suite and the fact that GitHub Actions build workflows are currently disabled

## [5.1.4] — 2026-04-08

### Added

- Add Ollama Cloud as an OpenAI-compatible provider with API-key configuration, per-provider budgets/notifications/refresh intervals, and dashboard history wiring

### Changed

- Fix runtime config-change handling for OpenRouter, Together AI, and Cohere so enabling them or changing their models reloads immediately without restarting Plasma

## [5.1.3] — 2026-04-02

### Changed

- Lower the narrow-layout breakpoints for live cards so provider, subscription, and cost cards keep the wider screenshot-style presentation in tighter popups
- Increase the full widget implicit width so Plasma opens a roomier representation by default
- Refresh release metadata for the 5.1.3 layout hotfix package

## [5.1.2] — 2026-04-02

### Changed

- Fix `EfficiencyMetricCard.qml` to use `PlasmaComponents.Label`, restoring analyst card loading in Plasma
- Fix `AnalystTab.qml` to use `root.usageDb` consistently and guard missing diagnostics selections so the full widget can start cleanly
- Refresh release metadata for the 5.1.2 analyst hotfix package

## [5.1.1] — 2026-04-02

### Changed

- Fix duplicate `activeFocusOnTab` declarations in `FullRepresentation.qml` so the main widget view loads cleanly in Plasma
- Fix duplicate `activeFocusOnTab` declarations in `ProviderCard.qml` so provider cards no longer break applet startup
- Refresh release metadata for the 5.1.1 Plasma widget hotfix package

## [5.1.0] — 2026-04-02

### Added

- Add analyst overview aggregation for anomalies, volatility, week-over-week spend, top cost drivers, and model exposure
- Add model-aware and cost-quality-aware snapshot storage for richer reporting and CSV exports
- Add provider diagnostics and copyable weekly/monthly report generation to the Analyst tab
- Add deterministic insight synthesis backed by real usage data instead of placeholder analyst text
- Add analyst database coverage for overview calculations and daily aggregation behavior

### Changed

- Replace the placeholder Analyst tab with a production operator view built around real history data
- Fix yearly activity and efficiency aggregations so repeated intra-day snapshots do not overcount usage
- Wire runtime snapshot recording to persist provider model and estimated-cost metadata
- Refresh release-facing README, AppStream, RPM, and Browser Sync messaging for the current upstream v5 line

## [5.0.0] — 2026-04-01

### Added

- Add **The Analyst** tab for deep usage visualization and efficiency metrics
- Add GitHub-style **Activity Heatmap** showing a full year of usage intensity (Cost or Volume)
- Add **Prompt Efficiency** metric tracking the Output/Input token ratio over time
- Add high-performance C++ database aggregation for yearly activity and efficiency series
- Add **Analyst Insight Engine** (Ollama integration mockup) for natural language usage summaries
- Add new configuration options for analyst intensity mode and outlier normalization

## [3.9.0] — 2026-03-19

### Added

- Add local mock API server (`scripts/demo/mock_ai_usage_server.py`) for deterministic, offline demo environment and testing
- Add demo mode overrides (`PLASMA_AI_MONITOR_DEMO=1`) to reroute C++ provider and subscription backends to `localhost:8080`
- Add canonical store-ready screenshots captured via the Fedora KDE VM using the new mock data environment
- Add comprehensive mock HTTP test suites for Azure OpenAI, GitHub Copilot, Claude Code, and Codex CLI
- Add deep test coverage for `UpdateChecker` including GitHub release parsing, prerelease filtering, and network timeouts

### Changed

- Refactor `main.qml` startup sequence and snapshot timers to fix popup fragility and eliminate UI blocking on panel open
- Improve theme awareness in `FullRepresentation.qml`, `ProviderCard.qml`, and `SubscriptionToolCard.qml` using `Kirigami.Theme` dynamic palettes
- Enhance keyboard navigation with explicit `KeyNavigation` and `activeFocusOnTab` across primary interactive elements
- Update AppStream metadata (`metainfo.xml`) with updated screenshot URLs and feature lists for KDE Store publication
- Refine `README.md` and walkthrough documentation with polished assets and demo mode instructions
- Add VS Code Remote/Dev Container workflow support for Fedora KDE build/test/demo-server setup without changing the requirement for a real KDE session during UI validation

## [3.8.1] — 2026-03-15

### Changed

- Clean up full popup history-state handling so detail/compare empty states are owned by the parent view instead of rendering duplicate fallback messages from child widgets
- Improve live dashboard responsiveness by splitting dense provider/subscription card headers into title + metadata rows and tightening live/history narrow-width behavior
- Refresh README and roadmap references to reflect the shipped `3.8.0` version and current provider surface
- Fix provider backend build regressions by restoring the missing `setEstimatedCost()` declaration and correcting the custom server URL environment fallback call
- Fix the generated `.plasmoid` archive layout so Plasma/KDE Store installs see `metadata.json` and `contents/` at archive root instead of inside a nested `package/` directory
- Refresh release-facing repository URLs and RPM metadata for the current GitHub repository

## [3.8.0] — 2026-03-08

### Added

- Add interactive `uninstall.sh` script that detects user-local and system installs and removes them interactively, with COPR fallback guidance
- Add `just uninstall-guided` Justfile recipe for the new guided uninstall flow
- Add actionable error hints in ProviderCard — maps common HTTP errors (401, 403, 429), network failures, and KWallet errors to plain-language fix instructions
- Add stale user-local version detection in `install_bootstrap.sh` — warns when a cached `~/.local` copy shadows a freshly installed system package

### Changed

- `install.sh` now uses `--method auto` instead of `--method source`, so Fedora users are routed through COPR (faster, no compiler needed) by default
- `install_bootstrap.sh` now offers to reload plasmashell immediately after any install method completes, so the widget appears without a manual restart
- `install_local_plasmoid.sh` now uses `--install` for first-time installs and `--upgrade` for subsequent updates, fixing a failure when no prior user-local copy existed
- `reload_plasma.sh` now prints the restarted PID and a next-step hint to reduce confusion after the panel briefly disappears

## [3.7.0] — 2026-02-26

### Added

- Add dedicated Azure OpenAI backend class `AzureOpenAIProvider` with Azure-specific request/auth semantics
- Add provider registration/build wiring for `AzureOpenAIProvider` in plugin targets
- Add Azure provider mocked HTTP test coverage for success and auth-failure paths
- Add Azure provider alias/auth-slot mapping coverage in ProviderBackend unit tests
- Add Horizon dashboard overview strip in full view with KPI tiles for providers, connectivity, total cost, and tool monitors
- Add provider section header with connected/enabled status badge and targeted empty-state guidance when providers are enabled but disconnected

### Changed

- Replace Azure aliasing to `OpenAIProvider` with first-class `AzureOpenAIProvider` registration
- Switch Azure QML/runtime wiring from `projectId` aliasing to `deploymentId`
- Redesign full dashboard live layout grouping for clearer scanability before provider and subscription cards
- Improve theme-awareness of provider and subscription cards by using adaptive tinted surfaces and state-aware borders (connected/error/limit)

## [3.6.0] — 2026-02-26

### Added

- Add backend provider dispatch plumbing for Azure OpenAI with deterministic key mapping and fallback handling
- Add mocked/backend provider tests for Azure selection, normalization, and failure-safe parsing
- Add Azure OpenAI provider configuration UI and runtime wiring (`configProviders.qml`, `main.qml`, `main.xml`)
- Add Azure-specific API key handling via KWallet key slot `azure`

### Changed

- Extend Alerts and Budget config pages with Azure OpenAI toggles and per-provider budget controls

## [3.5.3] — 2026-02-26

### Fixed

- Fix `.plasmoid` archive generation in release CI by clamping `SOURCE_DATE_EPOCH` to ZIP's minimum supported timestamp (1980-01-01)

## [3.5.2] — 2026-02-26

### Fixed

- Fix release tarball packaging in CI by generating source archives from tracked files (`git ls-files`) to avoid `tar: .: file changed as we read it`

## [3.5.1] — 2026-02-26

### Fixed

- Fix AppStream metainfo validation by switching to a valid stock icon name (`utilities-system-monitor`) for release workflow compatibility

## [3.5.0] — 2026-02-26

### Added

- Add minimal Flatpak scaffold under `packaging/flatpak/` for distribution kickoff
- Add deterministic local packaging scripts for source tarball and `.plasmoid` archive generation
- Add Flatpak scaffold validation script for CI and local checks
- Add mocked HTTP coverage for `GoogleVeoProvider` (tier fallback, header limits, auth failure)
- Add real Plasma widget screenshots under `assets/screenshots/` (main, panel, settings-oriented)

### Changed

- Strengthen Flatpak scaffold validation to enforce manifest core fields and metadata/CMake version identity checks
- Update build and release workflows to run full packaging consistency checks (`check_version_consistency`, Flatpak scaffold, source/plasmoid check mode)
- Update release workflow to generate source/plasmoid artifacts via packaging scripts
- Improve `GoogleVeoProvider` success-path handling with response validation and request counting
- Prefer API-provided rate-limit headers for Google Veo when available, with known-limit fallback
- Replace README screenshot placeholder with actual captures and add walkthrough doc link

## [3.4.0] — 2026-02-26

### Added

- Add GitHub Copilot activity auto-detection from local IDE state/log paths (`Code`, `VSCodium`, `Code - OSS`) in `CopilotMonitor`
- Add baseline + increment regression test for Copilot activity detection in `plugin/tests/test_subscription_tools.cpp`
- Add subscription-tool cost rows to `CostSummaryCard.qml` for cumulative view transparency
- Add explicit Firefox-only support guidance in Subscriptions config UI

### Changed

- Include enabled subscription tool costs in total cost aggregation (`main.qml`, `CompactRepresentation.qml`, `CostSummaryCard.qml`)
- Expand onboarding flow from 3 to 4 steps and include subscription-tools setup guidance (`FullRepresentation.qml`)
- Clarify browser-sync config schema label to reflect Firefox-only support (`package/contents/config/main.xml`)

## [3.3.0] — 2026-02-25

### Added

- Add `scripts/install_doctor.sh` preflight checker for required commands, Fedora package checks, and SQLite driver diagnostics
- Add `scripts/install_bootstrap.sh` guided installer with `auto|copr|source|user` modes and dry-run support
- Add new Just recipes for install UX: `doctor`, `doctor-fix`, `bootstrap`, `bootstrap-source`, `bootstrap-copr`, `bootstrap-user`
- Add first-run setup wizard state in config (`setupWizardCompleted`, `setupWizardDismissed`)
- Add first-run onboarding wizard in `FullRepresentation.qml` with step-by-step setup guidance
- Add persisted Firefox profile selection for Browser Sync (`browserSyncProfile`) with auto/default fallback

### Changed

- Update `install.sh` to delegate to the guided bootstrap flow (`source` mode with dependency auto-fix)
- Update README and CONTRIBUTING docs with guided bootstrap and doctor workflows
- Expand `hasAnyProvider()` checks to include OpenRouter, Together AI, Cohere, and Google Veo for correct empty-state behavior
- Improve Browser Sync settings UX with profile reload and safer fallback when saved profiles disappear

## [3.2.0] — 2026-02-22

### Added

- Add AppStream metainfo (`com.github.loofi.aiusagemonitor.metainfo.xml`) for KDE Discover and AppStream catalogs
- Add COPR build infrastructure (`.copr/Makefile`, `scripts/build_srpm.sh`) for Fedora package distribution
- Add `.plasmoid` archive as GitHub Release artifact for KDE Store submission
- Add AppStream validation step in release CI workflow and RPM `%check` section
- Add `%license`, `%doc`, and `%check` sections to RPM spec
- Add COPR install instructions to README
- Add `assets/screenshots/` directory for KDE Store listing preparation
- Extend version consistency check to validate AppStream metainfo version

### Changed

- Update RPM spec `Source0` to use full GitHub archive URL (required for COPR)
- Update release workflow to include `libappstream-glib` and `zip` for new validation and packaging steps

## [3.1.0] — 2026-02-20

### Added

- Add OpenRouter provider with 22-model pricing and credits balance endpoint
- Add Together AI provider with 12-model pricing (Llama, Qwen, DeepSeek, Mixtral, Gemma)
- Add Cohere provider with 7-model pricing via OpenAI-compatible endpoint
- Add configProviders.qml sections for all 3 new providers with model selectors, API key management, and custom base URL support
- Add per-provider refresh timers, notification toggles, and budget controls for OpenRouter, Together AI, and Cohere
- Add unit tests for OpenRouter (usage + credits), Together AI, and Cohere providers

## [3.0.0] — 2026-02-19

### Changed

- Update model pricing tables for all 7 providers to 2026 pricing
- Reorder Google Gemini pricing by model generation (2.5 → 2.0 → 1.5)
- Reorder Groq pricing to group Llama models together
- Sync QML model selector dropdowns with registered C++ pricing

### Added

- Add `gemini-2.0-flash-lite` pricing ($0.075/$0.30 per 1M tokens)
- Add `grok-2-mini` pricing ($2.00/$10.00 per 1M tokens)
- Add `deepseek-coder` pricing ($0.14/$0.28 per 1M tokens)
- Add Anthropic date-suffixed model pricing (`claude-3-5-sonnet-20241022`, `claude-3-5-haiku-20241022`)
- Add Mistral `-latest` alias pricing (`mistral-large-latest`, `mistral-medium-latest`, `mistral-small-latest`, `codestral-latest`)
- Add Claude 3.7 Sonnet to Anthropic model selector
- Add Gemini 2.5 Pro and 2.5 Flash (stable) to Google model selector
- Add Llama 3.1 70B Versatile to Groq model selector

### Fixed

- Fix duplicate v2.9.0 changelog entry in RPM spec file

## [2.9.0] — 2026-02-18

### Added

- Add 43 new C++ unit tests across 4 test files covering ProviderBackend, SubscriptionToolBackend, UpdateChecker, and UsageDatabase
- Test budget warning/exceeded signals with dedup validation
- Test token-based cost estimation with exact and prefix model matching
- Test generation counter for stale request detection
- Test disconnect/reconnect signal transitions
- Test subscription limit warnings, period calculations, and auto-reset
- Test version property setters and interval clamping in UpdateChecker
- Test database pruning, CSV/JSON export, summary aggregation, retention clamping, and disabled recording

## [2.8.2] — 2026-02-16

### Added

- Add provider mocked-HTTP test suite covering OpenAI, Anthropic, and DeepSeek response parsing
- Add subscription monitor test suite for plan defaults, install detection, usage reset behavior, and sync diagnostics
- Add blocking `clang-tidy` CI gate with repository `.clang-tidy` config and `scripts/run_clang_tidy_ci.sh`
- Add `syncDiagnostic(toolName, code, message)` signal for deterministic browser-sync failure diagnostics

### Changed

- Improve Browser Sync connection checks with normalized status codes and explicit profile/cookie/session detection
- Improve Subscriptions config UX by mapping diagnostics to user-safe messages with actionable guidance text

## [2.8.1] — 2026-02-16

### Fixed

- Fix stale UI version display by preferring plasmoid metadata version in QML
- Normalize update checker parsing for prefixed/suffixed version strings

### Added

- Add local install/reload scripts to override stale system package installs
- Add diagnostics script for repo/local/system version mismatch visibility
- Add CI/CTest guard to prevent hardcoded semantic versions in QML

## [2.8.0] — 2026-02-16

### Added

- Add `AppInfo.version` singleton and remove hardcoded UI version drift
- Add compare analytics mode for cross-provider and cross-tool history views
- Add multi-series chart with legend, ranked hover tooltip, and crosshair
- Add aggregated series APIs: `getProviderSeries()` and `getToolSeries()`
- Add test coverage for series metrics/bucketing and history mapping regressions
- Add version-consistency CI test and run `ctest` in GitHub Actions

### Fixed

- Fix history mapping (display label vs DB name) for Google/Mistral/xAI
- Fix provider notification gating consistency for quota/budget/connectivity/error alerts
- Add explicit loading/empty states and export gating in History

## [2.7.0] — 2026-02-17

### Fixed

- Fix reply-after-deleteLater in OpenAI costs and monthly costs handlers
- Fix reads-after-deleteLater in OpenAICompatible 429 and success paths
- Fix shared DB connection name crash when multiple UsageDatabase instances exist
- Fix compactDisplayMode config alias writing integer index instead of string
- Fix timer binding breakage — remove imperative handler that broke declarative bindings
- Fix textToCents returning NaN for invalid budget input (now defaults to 0)

### Added

- Add retry with exponential backoff to OpenAI costs and monthly costs endpoints
- Add write throttling to tool snapshot recording (matching provider snapshot throttle)
- Add restrictive permissions (owner-only) on cookie temp file copies
- Add retentionDays range clamping (1–365 days)

## [2.6.0] — 2026-02-16

### Fixed

- Fix m_activeReplies never populated — beginRefresh() now actually aborts in-flight requests
- Fix retry logic sending GET instead of POST for chat completion retries
- Fix pruneOldData() totalDeleted only counting last table (now sums all 3 DELETEs)
- Fix double deleteLater() in OpenAICompatible and OpenAI retry paths

### Added

- Add beginRefresh(), createRequest(), and generation counter to GoogleProvider
- Add generation counter and createRequest() to DeepSeek fetchBalance()
- Add stale-reply guard to CopilotMonitor fetchOrgMetrics()
- Add trackReply() for registering in-flight replies across all providers
- Add short-lived cookie DB cache in BrowserCookieExtractor (avoids triple reads)
- Add i18n() wrapping to all user-visible error messages in providers
- Split updatechecker.h into .h/.cpp (was header-only with full implementation)
- Add 30s timeout to UpdateChecker GitHub API request

## [2.5.0] — 2026-02-16

### Added

- Add centralized request builder with 30s timeout on all API requests
- Add generation counter for request cancellation on re-refresh
- Add retry with exponential backoff for transient HTTP errors (429/500/502/503)
- Add Retry-After header parsing for 429 rate limit responses
- Separate budgetWarning and budgetExceeded signals with notification dedup
- Add HTTPS validation warning for custom base URLs
- Centralize rate limit header parsing in ProviderBackend base class
- Add DB write throttling (60s per provider, skip if data unchanged)
- Wrap pruneOldData() in SQLite transaction for atomic multi-table cleanup
- Add eager database init via UsageDatabase::init() to avoid first-write stall

### Fixed

- Remove duplicate QNetworkAccessManager in CopilotMonitor
- Optimize UsageChart hover — only repaint when hoveredIndex changes

## [2.4.0] — 2026-02-17

### Added

- Add model name badge in ProviderCard header
- Add subscription tool warning/critical indicators in panel badge
- Add All/Day/Month toggle to CostSummaryCard with per-provider breakdown
- Add hover tooltips with crosshair to UsageChart canvas
- Add Bézier smooth curve interpolation to UsageChart

### Fixed

- Fix rate limit bars to show "used" instead of "remaining" for visual consistency
- Fix UsageChart Y-axis to start at zero for accurate visual scale
- Refactor configBudget.qml from copy-paste to data-driven Repeater

## [2.3.0] — 2026-02-16

### Added

- Add browser cookie sync for real-time usage data from Claude.ai and ChatGPT
- Add BrowserCookieExtractor for reading Firefox session cookies (read-only)
- Full dashboard view: session info, extra usage, tertiary limits, credits
- Add subscription cost display per tool ($X/mo badge)
- Extend ClaudeCodeMonitor with API sync (session %, weekly, extra usage/billing)
- Extend CodexCliMonitor with API sync (5h, weekly, code review, credits)
- Add CopilotMonitor subscription cost support
- Redesign SubscriptionToolCard.qml as full dashboard replica
- Add browser sync config section with connection test buttons
- Add sync refresh button and auto-sync timer in main widget
- Mark browser sync as experimental with ToS disclaimer

## [2.2.0] — 2026-02-15

### Added

- Add subscription tool tracking for Claude Code, Codex CLI, and GitHub Copilot
- Add SubscriptionToolBackend abstract base class with rolling time windows
- Add ClaudeCodeMonitor with 5h session + weekly dual limits (Pro/Max5x/Max20x)
- Add CodexCliMonitor with 5h window (Plus/Pro/Business plans)
- Add CopilotMonitor with monthly limits + optional GitHub API org metrics
- Add SubscriptionToolCard.qml with progress bars and time-until-reset
- Add configSubscriptions.qml with per-tool plan/limit/notification settings
- Add subscription_tool_usage table to SQLite database
- Add DeepSeek account balance display in ProviderCard
- Add Google Gemini free/paid tier selector with tier-aware rate limits

### Fixed

- Fix token accumulation bug in OpenAICompatibleProvider (session-level tracking)
- Fix estimatedMonthlyCost returning 0 for non-OpenAI providers
- Fix budget notifications not firing from direct cost setters

## [2.1.0] — 2026-02-15

### Added

- Add OpenAICompatibleProvider base class, dedup Mistral/Groq/xAI/DeepSeek
- Add token-based cost estimation with per-model pricing (~30 models)
- Add per-provider refresh timers (individual timers per provider)
- Add collapsible provider cards with animated transitions
- Add HTTPS security warnings on custom base URL fields
- Add ClipboardHelper C++ class for data export
- Add accessibility annotations (ProviderCard, CostSummaryCard, CompactRepresentation)
- Data-driven provider cards via Repeater (eliminates hardcoded cards)
- Enable cost/usage display for Anthropic and Google providers

### Fixed

- Fix version mismatch (CMakeLists 1.0.0 to 2.0.0)
- Fix xAI default model (grok-3-mini to grok-3)
- Fix token tracking for Mistral/Groq/xAI providers
- Fix DeepSeek cost display (separate balance from cost)
- Fix OpenAI monthly cost tracking (new billing API endpoint)

## [2.0.0] — 2026-02-15

### Added

- Add Mistral AI, DeepSeek, Groq, and xAI/Grok providers
- Add SQLite-based usage history with chart visualization
- Add budget management with daily/monthly limits per provider
- Add per-provider refresh intervals and notification controls
- Add data export (CSV/JSON) and trend analysis
- Add custom base URL support for all providers (proxy support)

### Fixed

- Fix compact display mode rendering (cost/count modes)
- Fix rate limit remaining=0 edge case in OpenAI provider
- Remove dead parseRateLimitHeaders() method

## [1.0.0] — 2026-02-15

### Added

- Initial release
- Support for OpenAI, Anthropic, and Google Gemini providers
- KWallet integration for secure API key storage
- KDE notifications for rate limit warnings

[Unreleased]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v11.0.0...HEAD
[11.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v10.0.1...v11.0.0
[10.0.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v10.0.0...v10.0.1
[10.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v9.0.0...v10.0.0
[9.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v8.0.3...v9.0.0
[8.0.3]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v8.0.2...v8.0.3
[8.0.2]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v8.0.1...v8.0.2
[8.0.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v8.0.0...v8.0.1
[8.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v7.0.0...v8.0.0
[7.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v6.0.1...v7.0.0
[6.0.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v6.0.0...v6.0.1
[6.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.4.1...v6.0.0
[5.4.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.4.0...v5.4.1
[5.4.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.3.0...v5.4.0
[5.3.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.2.0...v5.3.0
[5.2.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.1.4...v5.2.0
[5.1.4]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.1.3...v5.1.4
[5.1.3]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.1.2...v5.1.3
[5.1.2]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.1.1...v5.1.2
[5.1.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.1.0...v5.1.1
[5.1.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v5.0.0...v5.1.0
[5.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.9.0...v5.0.0
[3.9.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.8.1...v3.9.0
[3.8.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.8.0...v3.8.1
[3.8.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.7.0...v3.8.0
[3.7.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.6.0...v3.7.0
[3.6.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.5.3...v3.6.0
[3.5.3]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.5.2...v3.5.3
[3.5.2]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.5.1...v3.5.2
[3.5.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.5.0...v3.5.1
[3.5.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.4.0...v3.5.0
[3.4.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.3.0...v3.4.0
[3.3.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.2.0...v3.3.0
[3.2.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.1.0...v3.2.0
[3.1.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v3.0.0...v3.1.0
[3.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.9.0...v3.0.0
[2.9.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.8.2...v2.9.0
[2.8.2]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.8.1...v2.8.2
[2.8.1]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.8.0...v2.8.1
[2.8.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.7.0...v2.8.0
[2.7.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.6.0...v2.7.0
[2.6.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.5.0...v2.6.0
[2.5.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.4.0...v2.5.0
[2.4.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.3.0...v2.4.0
[2.3.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.2.0...v2.3.0
[2.2.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.1.0...v2.2.0
[2.1.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v2.0.0...v2.1.0
[2.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/compare/v1.0.0...v2.0.0
[1.0.0]: https://github.com/loofiboss-bit/plasma-ai-usage-monitor/releases/tag/v1.0.0
