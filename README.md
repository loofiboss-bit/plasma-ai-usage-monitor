# AI Usage Monitor for KDE Plasma 6

<p align="center">
  <img src="assets/logo.png" alt="AI Usage Monitor logo" width="180">
</p>

AI Usage Monitor puts provider-reported usage and spend, local coding-tool activity, and read-only connection checks in your Plasma panel. It stores API keys in KWallet and keeps history on your computer.

Version **14.0.1 (First Successful Use)** guides you from source choice to a safe verification. The result says whether the source returned usage or spend, a balance, a local estimate, or connectivity only. Scheduled provider checks are read-only, and missing values stay unknown instead of becoming false zeroes.

![AI Usage Monitor main window](assets/screenshots/main-window.png)

## Install on Fedora

The COPR package is the supported Fedora installation. It includes both the Plasma widget and the compiled Qt plugin.

~~~bash
sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor
sudo dnf install plasma-ai-usage-monitor
~~~

Log out and back in after the first install. Then:

1. Right-click the panel or desktop and choose **Add Widgets**.
2. Search for **AI Usage Monitor**.
3. Add it to the panel or desktop.
4. Open the widget and complete **Guided first success** for one source.

To update later:

~~~bash
sudo dnf upgrade plasma-ai-usage-monitor
~~~

For source builds and other installation details, read the [installation guide](docs/user-guide/installation.md).

## Start here

- [Install or update the widget](docs/user-guide/installation.md)
- [Complete the first setup](docs/user-guide/getting-started.md)
- [Understand actual, estimated, and unavailable values](docs/user-guide/understanding-data.md)
- [Configure API providers](docs/user-guide/providers.md)
- [Track subscription tools and Browser Sync Labs](docs/user-guide/subscriptions.md)
- [Use history, exports, Prometheus, and webhooks](docs/user-guide/history-and-integrations.md)
- [Fix common problems](docs/user-guide/troubleshooting.md)
- [Review privacy and security behavior](docs/user-guide/privacy-and-security.md)

The [GitHub wiki](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/wiki) provides the same task-based entry points for people who prefer GitHub's wiki navigation.

## What it monitors

OpenAI can report account usage and spend with an Admin API key. OpenRouter can report key usage, LiteLLM can report gateway spend, and DeepSeek can report a balance. Many other providers expose only model discovery or account connectivity. AI Usage Monitor labels those results as connection checks and does not turn them into invented usage data.

Hosted-provider support includes:

- OpenAI, Anthropic, Google Gemini, DeepSeek, OpenRouter, and Google Veo
- Mistral AI, Groq, xAI, Ollama Cloud, Together AI, and Cohere
- Azure OpenAI, AWS Bedrock, LiteLLM Proxy, Cerebras, Fireworks AI, and Perplexity

The generated [provider capability matrix](docs/provider-capabilities.md) lists the scheduled endpoint and monitoring level for every provider.

The widget also tracks Claude Code, Codex CLI, GitHub Copilot, Cursor, Windsurf, and JetBrains AI. Local activity and configured plan limits are estimates unless a supported authenticated source provides a live quota window.

## Main features

- Guided first success for one provider or detected local tool
- Source-focused Settings with search, monitoring-level filters, and one detail pane
- Overview sections for reporting providers, connection checks, local tools, and sources that need attention
- Provider usage, spend, balances, limits, and connection state where the provider exposes them
- Local history with single-provider and comparison views
- Analyst view with spend trends, activity heatmap, anomalies, and top drivers
- Daily and monthly budget warnings
- KDE notifications plus optional Slack and Discord webhooks
- Scheduled JSON or CSV exports
- A loopback-only Prometheus endpoint
- Configuration export that excludes keys, tokens, cookies, and webhook URLs
- Native Diagnostics for the loaded plugin, install layers, database, source readiness, KWallet, catalogs, and browser profiles
- No telemetry or hosted backend

## Data boundaries

Scheduled provider refreshes do not run inference requests. Explicit inference tests are labeled in the widget and may consume provider quota or money.

Secrets stay in KWallet. Usage history stays in a local SQLite database. Browser Sync Labs is off by default and uses undocumented service endpoints that can change without notice.

Read [Understanding the data](docs/user-guide/understanding-data.md) before setting budgets or treating a provider card as a billing record.

## Screenshots

| View | Screenshot |
| --- | --- |
| Guided first success | ![Guided first success source choice](assets/screenshots/guided-first-success.png) |
| Verified result | ![Verified setup result with its monitoring level](assets/screenshots/verified-success.png) |
| Overview | ![Overview with reporting providers and connection checks](assets/screenshots/main-window.png) |
| Provider result | ![Provider result with source and quality labels](assets/screenshots/provider-intelligence.png) |
| Provider settings | ![Provider settings](assets/screenshots/settings-view.png) |
| History | ![History with compatible units and currency](assets/screenshots/history-view.png) |
| Analyst | ![Analyst view with the current navigation](assets/screenshots/analyst-view.png) |
| Panel | ![Compact panel state](assets/screenshots/panel-view.png) |

## Distribution notes

- **Fedora COPR:** supported and recommended. Includes the widget and compiled plugin.
- **Source build:** supported. Builds the same two parts.
- **KDE Store plasmoid:** frontend-only. Install the matching compiled plugin from COPR or a source build before installing the Store package. A missing or mismatched plugin opens an in-widget recovery screen; restart Plasma after repairing the installation.
- **Flatpak:** not supported because the native QML plugin is not packaged by the old scaffold.

## Development

The project uses C++20, Qt 6, KDE Frameworks 6, QML, CMake, and a repo-owned Justfile.

~~~bash
git clone https://github.com/loofiboss-bit/plasma-ai-usage-monitor.git
cd plasma-ai-usage-monitor
just doctor
just build-debug
just test
just check
~~~

Use `just dev` for QML-only changes. Use `just install`, `just reload`, and `just smoke` when the compiled plugin changes.

Contributor setup, architecture, code conventions, and release checks live in [CONTRIBUTING.md](CONTRIBUTING.md).

## Project documents

| Document | Purpose |
| --- | --- |
| [User guide](docs/user-guide/README.md) | Task-based help for installation, setup, daily use, and troubleshooting |
| [Provider capabilities](docs/provider-capabilities.md) | Generated provider monitoring contract |
| [Architecture](docs/architecture/reliability-core.md) | Runtime, data, secret, and distribution boundaries |
| [Changelog](CHANGELOG.md) | Release history |
| [Roadmap](ROADMAP.md) | Current product direction and next work |
| [Security policy](SECURITY.md) | Vulnerability reporting and supported releases |
| [Contributing](CONTRIBUTING.md) | Developer workflow and contribution rules |

## Support

For a bug, open a [GitHub issue](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/issues) and include:

- Plasma version from **plasmashell --version**
- Linux distribution and version
- installed widget version from **rpm -q plasma-ai-usage-monitor** on Fedora
- steps to reproduce
- the redacted support report from **Settings → Diagnostics**

Report security problems privately through [GitHub Security Advisories](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/security/advisories/new).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
