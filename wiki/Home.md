# AI Usage Monitor Wiki

Welcome to the wiki for the **AI Usage Monitor** KDE Plasma 6 widget!

This wiki serves as the comprehensive user guide, diagnostic manual, and design reference for the widget. It is designed to help you set up providers, manage spending budgets, understand token-based cost tracking logic, and solve common issues.

## Quick Navigation

| If you are... | Start here |
|---|---|
| **Installing or updating the widget** | [Getting Started](./Getting-Started.md) |
| **Wanting to understand the UI elements & charts** | [App User Guide](./App-User-Guide.md) |
| **Configuring budgets, notifications, or subscription tools** | [Feature Workflows](./Feature-Workflows.md) |
| **Curious about how costs are estimated and stored** | [Logic and Cost Estimation](./Logic-and-Estimation.md) |
| **Evaluating data boundaries, KWallet, and cookies** | [Privacy & Security](./Privacy-and-Security.md) |
| **Encountering error codes or widget failures** | [Troubleshooting & FAQ](./Troubleshooting-and-FAQ.md) |

---

## What is AI Usage Monitor?

AI Usage Monitor is a native KDE Plasma 6 widget (plasmoid) paired with a C++ plugin helper. It sits in your Plasma panel as a compact icon and expands into a rich dashboard to track your AI usage across multiple providers and subscription-based local coding utilities.

### Supported Providers
The widget tracks 18+ hosted providers:
* **OpenAI** (via billing and usage APIs; requires Admin API Key)
* **OpenRouter** (via key usage and remaining limits API)
* **LiteLLM Proxy** (via gateway spend logs)
* **DeepSeek** (via model discovery and prepaid balance endpoint)
* **Connectivity-only providers**: Anthropic Claude, Google Gemini, Mistral AI, Groq, xAI (Grok), Google Veo, Azure OpenAI, AWS Bedrock, Ollama Cloud, Together AI, Cohere, Cerebras, Fireworks AI, Perplexity

### Supported Subscription & Local Tools
* **Google Antigravity** (local daemon for Antigravity 2.x signed-in plan and live per-model quota windows)
* **Claude Code** (via filesystem watcher on `~/.claude/` for local activity)
* **OpenAI Codex CLI** (via filesystem watcher on `~/.codex/` and live quota window checks)
* **GitHub Copilot** (via local IDE activity/log paths and optional organization billing API sync)
* **Cursor** (via local chat/db activity tracking)
* **Windsurf** (via local activity tracking)
* **JetBrains AI** (via local `idea.log` monitoring for request counts)

---

## Documentation Map

* **In-Repository Docs:**
  * [README.md](../README.md) — Quick installation, quick-start, and project architecture overview
  * [docs/provider-capabilities.md](../docs/provider-capabilities.md) — Generated provider capability/monitoring level contract matrix
  * [CONTRIBUTING.md](../CONTRIBUTING.md) — C++ and QML development workflow, test setups, and guidelines
  * [SECURITY.md](../SECURITY.md) — Security policies and vulnerability disclosures
  * [CHANGELOG.md](../CHANGELOG.md) — Complete release logs and version milestone records
