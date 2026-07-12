# Contributing to AI Usage Monitor

Thank you for your interest in contributing to the AI Usage Monitor plasmoid. This document covers how to set up the project for development, coding standards, and the contribution workflow.

## Development Setup

### Prerequisites

Fedora 44 KDE (or any distro with KDE Plasma 6):

```bash
sudo dnf install cmake extra-cmake-modules gcc-c++ \
    qt6-qtbase qt6-qtbase-devel qt6-qtdeclarative-devel \
    libplasma-devel kf6-kwallet-devel kf6-ki18n-devel kf6-knotifications-devel
```

### Building

```bash
git clone https://github.com/loofiboss-bit/plasma-ai-usage-monitor.git
cd plasma-ai-usage-monitor
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
```

### Installing for Testing

```bash
# Install the C++ QML plugin to the system path
sudo cmake --install build

# Or use the install script
./install.sh

# Or run guided bootstrap + dependency checks
./scripts/install_bootstrap.sh --method source --install-missing

# Or upgrade the plasmoid only in your user profile (no sudo)
./scripts/install_local_plasmoid.sh
```

### Testing Changes

For QML-only changes, use the fast local dev loop:

```bash
just dev
```

For C++ plugin changes, rebuild/install the plugin and reload Plasma:

```bash
just install
just reload
```

If the UI still shows an old app version or stale behavior, run:

```bash
just versions
just smoke
```

`just smoke` is the quickest check for version shadowing and the common case where the plasmoid package updated but the compiled plugin did not.

To test in a standalone window (without adding to panel):

```bash
plasmawindowed com.github.loofi.aiusagemonitor
```

Run automated tests before submitting:

```bash
just test
just check
just doctor
```

## Project Structure

- **`package/`** — The plasmoid package (QML UI, metadata, config schema). Changes here are pure QML/JS and don't require a recompile, but do require `plasmashell --replace`.
- **`plugin/`** — C++ QML plugin providing backend classes. Changes here require a full rebuild and reinstall.

### Key Classes

| Class | File | Purpose |
|-------|------|---------|
| `SecretsManager` | `plugin/secretsmanager.{h,cpp}` | KWallet wrapper for API key storage |
| `AppInfo` | `plugin/appinfo.{h,cpp}` | Build-version singleton exposed to QML (`AppInfo.version`) |
| `ProviderBackend` | `plugin/providerbackend.{h,cpp}` | Abstract base for all providers (token usage, rate limits, cost, budget, errors, proxy support, per-model cost estimation) |
| `OpenAICompatibleProvider` | `plugin/openaicompatibleprovider.{h,cpp}` | Intermediate base for OpenAI-compatible APIs (chat completions, rate limit headers, token usage parsing) |
| `OpenAIProvider` | `plugin/openaiprovider.{h,cpp}` | OpenAI usage/costs/rate-limit integration (Admin API key required) |
| `AnthropicProvider` | `plugin/anthropicprovider.{h,cpp}` | Anthropic rate-limit header parsing via `count_tokens` |
| `GoogleProvider` | `plugin/googleprovider.{h,cpp}` | Google Gemini connectivity + static limits |
| `MistralProvider` | `plugin/mistralprovider.{h,cpp}` | Mistral AI (extends `OpenAICompatibleProvider`) |
| `DeepSeekProvider` | `plugin/deepseekprovider.{h,cpp}` | DeepSeek (extends `OpenAICompatibleProvider`, adds balance endpoint) |
| `GroqProvider` | `plugin/groqprovider.{h,cpp}` | Groq (extends `OpenAICompatibleProvider`) |
| `XAIProvider` | `plugin/xaiprovider.{h,cpp}` | xAI/Grok (extends `OpenAICompatibleProvider`) |
| `ClipboardHelper` | `plugin/clipboardhelper.h` | Clipboard copy/paste helper for data export |
| `UsageDatabase` | `plugin/usagedatabase.{h,cpp}` | SQLite persistence for usage history and subscription tool snapshots |
| `SubscriptionToolBackend` | `plugin/subscriptiontoolbackend.{h,cpp}` | Abstract base for subscription tool monitors (rolling time windows, dual limits, limit warnings) |
| `ClaudeCodeMonitor` | `plugin/claudecodemonitor.{h,cpp}` | Claude Code CLI usage monitor (filesystem watching, Pro/Max5x/Max20x plans) |
| `CodexCliMonitor` | `plugin/codexclimonitor.{h,cpp}` | OpenAI Codex CLI usage monitor (filesystem watching, Plus/Pro/Business plans) |
| `CopilotMonitor` | `plugin/copilotmonitor.{h,cpp}` | GitHub Copilot usage monitor (monthly limits, optional GitHub API org metrics) |
| `UpdateChecker` | `plugin/updatechecker.{h,cpp}` | Checks GitHub releases API for new versions with 30s timeout |
| `BrowserCookieExtractor` | `plugin/browsercookieextractor.{h,cpp}` | Reads Firefox session cookies for browser sync (read-only, short-lived cache, restrictive temp file permissions) |

### QML Components

| Component | Purpose |
|-----------|---------|
| `main.qml` | Root PlasmoidItem — keeps provider/tool instantiation and applet wiring while delegating orchestration to controller components |
| `ProviderRegistry.qml` | Central provider/tool descriptor registry for popup sections, scheduling, notifications, and aggregate stats |
| `NotificationController.qml` | Notification routing, cooldown, DND, and provider/tool alert handling |
| `RefreshScheduler.qml` | Registry-driven refresh timers plus recurring browser-sync/prune/org-metrics timers |
| `RuntimeCoordinator.qml` | Startup sequencing, API-key load, browser sync bootstrap, and snapshot recording |
| `CompactRepresentation.qml` | Panel icon with 3 display modes (icon/cost/count) and accessibility |
| `FullRepresentation.qml` | Popup with status bar, Live/History tabs, detail and compare history modes, responsive history controls, loading/empty states, export buttons |
| `ProviderCard.qml` | Collapsible provider stats card with budget bars, cost estimation labels, error details, and accessibility |
| `SubscriptionToolCard.qml` | Subscription tool usage card with progress bars, time-until-reset, dual limits, manual increment/reset |
| `CostSummaryCard.qml` | Aggregate cost breakdown across all providers with accessibility |
| `UsageChart.qml` | Canvas line/area chart (cost/tokens/requests/rateLimit) |
| `MultiSeriesChart.qml` | Multi-series comparison chart for provider/tool analytics with compact legend chips and ranked hover tooltip |
| `TrendSummary.qml` | Summary stats grid for a time range |
| `configGeneral.qml` | General settings + per-provider refresh intervals |
| `configProviders.qml` | Provider enable/key/model/proxy settings with HTTPS security warnings |
| `configAlerts.qml` | Thresholds, notification types, per-provider toggles, cooldown, DND |
| `configBudget.qml` | Per-provider daily/monthly budgets |
| `configSubscriptions.qml` | Subscription tool enable/plan/limit settings with auto-detect |
| `configHistory.qml` | History enable/retention/prune settings |

### Test Targets

- `just test` currently runs 12 local CTest targets covering provider mocked HTTP behavior, provider backend helpers, subscription tools, database/history flows, and update/version checks
- `providers_mocked_http` includes regression coverage for shipped provider request flows, including stale-generation handling for delayed replies
- `subscription_tools` covers local tool usage tracking, reset windows, and sync-related edge cases
- `version_consistency` validates version alignment across `CMakeLists.txt`, `package/metadata.json`, and `plasma-ai-usage-monitor.spec`

## Coding Standards

### C++

- C++20 standard
- Follow existing naming conventions: `camelCase` for methods and variables, `PascalCase` for class names
- Use `Q_EMIT` instead of bare `emit`
- Use `QStringLiteral()` for string literals
- All Q_PROPERTYs must have NOTIFY signals
- New providers should inherit from `OpenAICompatibleProvider` (if OpenAI-compatible) or `ProviderBackend` (if custom API) and implement the required virtual methods
- Use `effectiveBaseUrl(BASE_URL)` instead of hardcoded base URLs to support custom proxy URLs
- Use `registerModelPricing()` in the constructor and `updateEstimatedCost(model)` after parsing tokens to enable automatic cost estimation
- Budget values are managed through the base class — call `setDailyCost()` / `setMonthlyCost()` in your `refresh()` implementation

### QML

- Plasma 6 APIs only (`import org.kde.plasma.plasmoid`, `import org.kde.kirigami as Kirigami`)
- Config pages must use `KCM.SimpleKCM` as root element (from `org.kde.kcmutils`)
- Use `Kirigami.Units` and `Kirigami.Theme` for sizing and colors — no hardcoded pixel values or colors
- Child components access root PlasmoidItem properties via the `root` id (dynamic scoping)
- Provider cards are data-driven via `Repeater` over `root.allProviders` — do not hardcode per-provider cards in `FullRepresentation.qml`
- Shared provider metadata lives in `ProviderRegistry.qml`; keep provider descriptors there instead of reintroducing scattered per-provider arrays/timers in `main.qml`
- Add `Accessible.role` and `Accessible.name` to interactive and informational components for screen reader support
- Budget config values are stored as integers in cents (e.g., 1050 = $10.50) — convert with `/ 100.0` when passing to C++ backends

### Config Conventions

- Budget entries in `main.xml` use `type="Int"` storing **cents** (not dollars)
- DND hours in `main.xml` use `type="Int"` with -1 meaning disabled and 0-23 for hours
- Config pages use explicit `property int` (not `property alias`) when the UI value differs from the stored config value

## Adding a New Provider

### OpenAI-compatible providers (recommended path)

If the new provider uses an OpenAI-compatible chat completions API:

1. Create `plugin/newprovider.h` and `plugin/newprovider.cpp` inheriting from `OpenAICompatibleProvider`
2. Implement only `name()`, `iconName()`, and `defaultBaseUrl()` — typically ~15 lines per file
3. In the constructor, set the default model and call `registerModelPricing()` for each supported model
4. Register the type in `plugin/aiusageplugin.cpp`; `qt_add_qml_module()` generates `qmldir` and type metadata.
5. Add source files to `plugin/CMakeLists.txt`
6. Add config entries in `package/contents/config/main.xml` (enable, model, customBaseUrl, budget, notifications, refresh interval)
7. Add UI elements in `configProviders.qml`, `configBudget.qml`, `configAlerts.qml`, and `configGeneral.qml`
8. Add the provider descriptor to `ProviderRegistry.qml` and keep the config key, display name, color, and runtime wiring aligned there
9. Instantiate the backend in `main.qml` with budget conversion (`/ 100.0`) and expose it to the registry/controller wiring

See `MistralProvider` or `GroqProvider` for minimal examples (~15 lines of C++ each).

### Custom API providers

If the provider has a completely different API:

1. Create `plugin/newprovider.h` and `plugin/newprovider.cpp` inheriting from `ProviderBackend`
2. Implement the pure virtual `refresh()` method to call the provider's API
3. Use `effectiveBaseUrl(BASE_URL)` for the API URL to support custom proxy URLs
4. Call `setInputTokens()`, `setOutputTokens()`, `setRequestCount()` to update usage
5. Call `setDailyCost()` / `setMonthlyCost()` / `setCost()` if the provider supports billing
6. Optionally call `registerModelPricing()` + `updateEstimatedCost()` for token-based estimation
7. Follow steps 4-9 from the OpenAI-compatible path above

## Contribution Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes and test locally
4. Ensure the project builds cleanly with no warnings (`-DCMAKE_BUILD_TYPE=Debug`)
5. Commit with clear, descriptive messages
6. Push to your fork and open a Pull Request

## Reporting Issues

Open an issue at [github.com/loofiboss-bit/plasma-ai-usage-monitor/issues](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/issues) with:

- Your Plasma version (`plasmashell --version`)
- Your distro and version
- Steps to reproduce
- Any relevant error output from `journalctl --user -u plasma-plasmashell -f`

## License

By contributing, you agree that your contributions will be licensed under the GPL-3.0-or-later license.
