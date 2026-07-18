Name:           plasma-ai-usage-monitor
Version:        14.1.1
Release:        1%{?dist}
Summary:        KDE Plasma 6 widget to monitor AI API token usage, rate limits, and costs
License:        GPL-3.0-or-later
URL:            https://github.com/loofiboss-bit/plasma-ai-usage-monitor
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  git-core
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  libplasma-devel
BuildRequires:  kf6-kwallet-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-knotifications-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  libappstream-glib
BuildRequires:  openssl-devel
BuildRequires:  protobuf-compiler
BuildRequires:  protobuf-devel
BuildRequires:  qt6-qtbase-private-devel

Requires:       plasma-workspace >= 6.0
Requires:       kf6-kwallet
Requires:       kf6-kirigami
Requires:       kf6-kcmutils
Requires:       qt6-qtbase

%description
A native KDE Plasma 6 plasmoid that monitors AI API token usage,
rate limits, costs, and budgets across multiple providers including
OpenAI, Azure OpenAI, AWS Bedrock, Anthropic (Claude), Google Gemini,
Mistral AI, DeepSeek, Groq, xAI (Grok), Ollama Cloud, OpenRouter,
Together AI, Cohere, Google Veo, LiteLLM Proxy, Cerebras Inference,
Fireworks AI, and Perplexity API.

Features:
- Capability-aware rate-limit monitoring where providers expose complete live pairs
- Usage and cost tracking with historical trends
- Catalog-backed cost estimation with local JSON metadata
- Budget management with daily/monthly limits
- SQLite-based usage history with chart visualization
- Secure API key storage via KDE Wallet
- Configurable alerts with KDE notifications
- Per-provider refresh intervals and notification controls
- Collapsible provider cards with accessibility support
- Data export (CSV/JSON) and non-secret configuration portability
- Trust Center diagnostics for catalog review, KWallet, and local readiness
- Panel icon with status badge indicator
- HTTPS security warnings for custom base URLs

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%check
%ctest
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/com.github.loofi.aiusagemonitor.metainfo.xml

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_datadir}/plasma/plasmoids/com.github.loofi.aiusagemonitor/
%{_libdir}/qt6/qml/com/github/loofi/aiusagemonitor/
%{_datadir}/icons/hicolor/scalable/apps/com.github.loofi.aiusagemonitor.svg
%{_datadir}/knotifications6/plasma_applet_com.github.loofi.aiusagemonitor.notifyrc
%{_datadir}/metainfo/com.github.loofi.aiusagemonitor.metainfo.xml

%changelog
* Sun Jul 19 2026 Loofi <loofi@github.com> - 14.1.1-1
- Prepare v14.1.1 release

* Sat Jul 18 2026 Loofi <loofi@github.com> - 14.1.0-1
- Prepare v14.1.0 release

* Sat Jul 18 2026 Loofi <loofi@github.com> - 14.0.1-1
- Prepare v14.0.1 release

* Sat Jul 18 2026 Loofi <loofi@github.com> - 14.0.0-1
- Prepare v14.0.0 release

* Fri Jul 17 2026 Loofi <loofi@github.com> - 13.0.0-2
- Add git-core to the isolated buildroot for exact-tag policy tests

* Fri Jul 17 2026 Loofi <loofi@github.com> - 13.0.0-1
- Prepare v13.0.0 release

* Mon Jul 13 2026 Loofi <loofi@github.com> - 12.0.3-1
- Prepare v12.0.3 release

* Mon Jul 13 2026 Loofi <loofi@github.com> - 12.0.2-1
- Prepare v12.0.2 release

* Mon Jul 13 2026 Loofi <loofi@github.com> - 12.0.1-1
- Fix installed QML plugin discovery and popup tab delegate startup

* Mon Jul 13 2026 Loofi <loofi@github.com> - 12.0.0-1
- Begin Reliability Core: deterministic refresh, typed provider state, and currency-aware history

* Tue Jun 30 2026 Loofi <loofi@github.com> - 11.0.0-1
- Ship Distribution & Catalog Truth with provider/subscription catalogs reviewed on 2026-06-30
- Make the COPR RPM path the primary Fedora install route and document install/update/remove validation
- Add DeepSeek v4 catalog entries and preserve legacy aliases with visible deprecation metadata
- Keep Copilot Auto billing labels accurate after the 2026-06-01 AI credits transition

* Tue May 12 2026 Loofi <loofi@github.com> - 10.0.1-1
- Fix provider card catalog singleton import so the installed plasmoid loads v10 source badges without runtime ReferenceError noise

* Tue May 12 2026 Loofi <loofi@github.com> - 10.0.0-1
- Ship Accuracy with OpenAI Costs API object amount parsing and source-aware usage metadata
- Keep connectivity probes out of displayed usage, spend, history spend, and budget warning state
- Add catalog/default drift validation and refresh catalog release metadata for v10
- Improve onboarding, Trust Center actions, source badges, and API/subscription cost separation

* Fri May 08 2026 Loofi <loofi@github.com> - 9.0.0-1
- Ship Confidence as a setup, diagnostics, catalog trust, and validation hardening release
- Add schema-v2 non-secret config export/import coverage for all KConfig keys
- Expose catalog review/source-conflict reasons in Diagnostics and validation gates
- Remove the stale standalone setup wizard from the shipped plasmoid payload

* Wed May 06 2026 Loofi <loofi@github.com> - 8.0.3-1
- Add self-tracked fallback limits for qualitative subscription-tool plans
- Show Codex and similar local activity monitors with progress bars when live quota fields are unavailable

* Wed May 06 2026 Loofi <loofi@github.com> - 8.0.2-1
- Preserve legacy Codex Pro plan index after adding Pro $100 metadata
- Fix zero-usage quota progress bars so they render empty instead of full

* Wed May 06 2026 Loofi <loofi@github.com> - 8.0.1-1
- Add official Copilot premium-request defaults for Free, Pro, Pro+, Business, and Enterprise
- Add current ChatGPT Pro tier Codex metadata and protect configured Pro from quota-less sync downgrades
- Surface Codex catalog fallback status when ChatGPT returns no live quota fields

* Wed May 06 2026 Loofi <loofi@github.com> - 8.0.0-1
- Ship Source Of Truth catalogs for provider pricing and subscription plans
- Add catalog validation, hardcoded-pricing checks, and quota-window UI badges
- Keep browser sync optional and local-first with visible source and precision metadata

* Thu Apr 30 2026 Loofi <loofi@github.com> - 7.0.0-1
- Prepare Beacon as the Fedora KDE 44 native reliability release
- Add Fedora 44 validation, Provider Catalog v2 checks, and Trust Center UX
- Improve Browser Sync Labs readiness, Copilot billing assumptions, and refresh diagnostics

* Fri Apr 10 2026 Loofi <loofi@github.com> - 5.3.0-1
- Add Cursor, Windsurf, and JetBrains AI local quota monitors
- Add AWS Bedrock provider scaffolding and AWS SigV4 signing support
- Add Linux Chrome and Chromium browser sync support
- Add local Prometheus metrics export, webhook notifications, and scheduled JSON/CSV export
- Refresh roadmap, AppStream metadata, and release documentation for Vanguard

* Thu Apr 09 2026 Loofi <loofi@github.com> - 5.2.0-1
- Add reusable OpenAI-compatible provider settings sections for shared KCM provider forms
- Add attention-first popup summaries with remembered provider and tool card expansion state
- Split runtime orchestration into provider registry, notification, scheduler, and coordinator QML components
- Add local smoke diagnostics for install shadowing and compiled plugin visibility

* Wed Apr 08 2026 Loofi <loofi@github.com> - 5.1.4-1
- Add Ollama Cloud as a supported OpenAI-compatible provider
- Add provider budgets, notifications, refresh controls, and history wiring for Ollama Cloud
- Fix runtime reload handling for OpenRouter, Together AI, and Cohere model/config changes

* Thu Apr 02 2026 Loofi <loofi@github.com> - 5.1.3-1
- Keep live dashboard cards in the wider layout at tighter popup widths
- Increase the default full widget width for a roomier Plasma presentation

* Thu Apr 02 2026 Loofi <loofi@github.com> - 5.1.2-1
- Fix analyst QML component loading by using PlasmaComponents.Label in EfficiencyMetricCard
- Fix AnalystTab database access and null diagnostics handling so the full widget starts cleanly

* Thu Apr 02 2026 Loofi <loofi@github.com> - 5.1.1-1
- Fix duplicate activeFocusOnTab declarations that prevented the Plasma widget from loading
- Refresh package metadata for the 5.1.1 hotfix release

* Thu Apr 02 2026 Loofi <loofi@github.com> - 5.1.0-1
- Add model-aware analyst overview metrics, anomaly surfacing, and provider diagnostics
- Add deterministic analyst reports and improved usage aggregation without daily overcounting
- Refresh release metadata and package surfaces for the 5.1.0 COPR/GitHub release

* Wed Apr 01 2026 Loofi <loofi@github.com> - 5.0.0-1
- Add The Analyst tab with yearly heatmap and efficiency KPIs
- Expand demo assets and analyst-focused usage history coverage
- Refresh release packaging metadata for the Lighthouse release

* Thu Mar 19 2026 Loofi <loofi@github.com> - 3.9.0-1
- Add demo-mode mock server and deterministic showcase assets
- Harden update checks, provider tests, and popup startup orchestration
- Refresh AppStream metadata, screenshots, and release-facing documentation

* Sun Mar 15 2026 Loofi <loofi@github.com> - 3.8.1-1
- Add guided uninstall flow and stale local install shadow detection
- Polish release/install UX and provider troubleshooting hints
- Fix KDE Store plasmoid archive layout to ship metadata.json and contents/ at archive root
- Add missing KF6 CoreAddons build dependency for RPM builds

* Thu Feb 26 2026 Loofi <loofi@github.com> - 3.7.0-1
- Add Horizon dashboard overview strip in full view with KPI tiles for providers, connectivity, total cost, and tool monitors
- Add provider section header with connected/enabled status badge and targeted empty-state guidance
- Redesign full dashboard live layout grouping for clearer scanability before provider and subscription cards
- Improve provider/subscription card theme adaptation with state-aware tinted surfaces and borders

* Thu Feb 26 2026 Loofi <loofi@github.com> - 3.4.0-1
- Add GitHub Copilot activity auto-detection using IDE state/log paths (Code, VSCodium, Code OSS)
- Add Copilot activity baseline and incremental usage tracking tests
- Include subscription tool monthly costs in compact/popup total cost summaries
- Expand onboarding from 3 to 4 steps with subscription tools guidance
- Restrict Browser Sync browser selection to Firefox and clarify unsupported options

* Sun Feb 22 2026 Loofi <loofi@github.com> - 3.2.0-1
- Add AppStream metainfo for KDE Discover and AppStream catalogs
- Add COPR build infrastructure (.copr/Makefile, scripts/build_srpm.sh)
- Add .plasmoid archive to GitHub Release artifacts
- Add AppStream validation to release CI and RPM spec
- Update README with COPR install instructions
- Extend version consistency check to cover metainfo XML
- Add SubscriptionToolBackend abstract base class with rolling time windows
- Add ClaudeCodeMonitor with 5h session + weekly dual limits (Pro/Max5x/Max20x)
- Add CodexCliMonitor with 5h window (Plus/Pro/Business plans)
- Add CopilotMonitor with monthly limits + optional GitHub API org metrics
- Add SubscriptionToolCard.qml with progress bars and time-until-reset
- Add configSubscriptions.qml with per-tool plan/limit/notification settings
- Add subscription_tool_usage table to SQLite database
- Fix token accumulation bug in OpenAICompatibleProvider (session-level tracking)
- Fix estimatedMonthlyCost returning 0 for non-OpenAI providers
- Fix budget notifications not firing from direct cost setters
- Add DeepSeek account balance display in ProviderCard
- Add Google Gemini free/paid tier selector with tier-aware rate limits

* Sun Feb 15 2026 Loofi <loofi@github.com> - 2.1.0-1
- Add OpenAICompatibleProvider base class, dedup Mistral/Groq/xAI/DeepSeek
- Add token-based cost estimation with per-model pricing (~30 models)
- Add per-provider refresh timers (individual timers per provider)
- Add collapsible provider cards with animated transitions
- Add HTTPS security warnings on custom base URL fields
- Add ClipboardHelper C++ class for data export
- Add accessibility annotations (ProviderCard, CostSummaryCard, CompactRepresentation)
- Data-driven provider cards via Repeater (eliminates hardcoded cards)
- Deduplicate provider arrays (single allProviders source of truth)
- Enable cost/usage display for Anthropic and Google providers
- Fix version mismatch (CMakeLists 1.0.0 to 2.0.0)
- Fix xAI default model (grok-3-mini to grok-3)
- Fix token tracking for Mistral/Groq/xAI providers
- Fix DeepSeek cost display (separate balance from cost)
- Fix OpenAI monthly cost tracking (new billing API endpoint)

* Sun Feb 15 2026 Loofi <loofi@github.com> - 2.0.0-1
- Add Mistral AI, DeepSeek, Groq, and xAI/Grok providers
- Add SQLite-based usage history with chart visualization
- Add budget management with daily/monthly limits per provider
- Add per-provider refresh intervals and notification controls
- Add data export (CSV/JSON) and trend analysis
- Add custom base URL support for all providers (proxy support)
- Fix compact display mode rendering (cost/count modes)
- Fix rate limit remaining=0 edge case in OpenAI provider
- Remove dead parseRateLimitHeaders() method

* Sun Feb 15 2026 Loofi <loofi@github.com> - 1.0.0-1
- Initial release
- Support for OpenAI, Anthropic, and Google Gemini providers
- KWallet integration for secure API key storage
- KDE notifications for rate limit warnings
