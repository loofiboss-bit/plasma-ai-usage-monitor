# Feature Workflows

This guide provides step-by-step instructions for configuring advanced widget features like budgets, notification scheduling, local subscription tracking, Browser Sync, and webhook integrations.

---

## 1. Setting Up Budgets and Spending Limits

You can set spending limits on providers that return cost metrics to prevent unexpected API bills.

1. Right-click the widget and open **Configure AI Usage Monitor...** → **Budget**.
2. Set a **Daily Budget** and/or **Monthly Budget** in USD (or the currency reported by the provider) for each provider.
3. Configure the **Warning Percentage** (default is 80%). The widget will trigger a desktop notification and turn progress bars yellow when your spend reaches this threshold.
4. Once spending hits 100% of the budget, the widget will trigger a critical alert, send a webhook (if enabled), and mark the progress bars red.

> [!WARNING]
> Budgets set on providers that use **Estimated pricing** are approximations. Always verify actual spend in your provider's official billing dashboard. Budgets are disabled for connectivity-only sources.

---

## 2. Tuning Alerts & Notifications

To avoid notification fatigue while keeping track of your API status, customize the alert rules.

1. Open **Settings → Alerts**.
2. **Master Toggle**: Toggle all notifications on or off.
3. **Thresholds**:
   * **Warning Threshold** (default 80%): Alerts when rate limits are nearing exhaustion.
   * **Critical Threshold** (default 95%): Alerts when rate limits are almost fully depleted.
4. **Notification Types**: Toggle individual alerts for API errors, budget thresholds, provider disconnects, and provider reconnects.
5. **Cooldown Period**: Set a cooldown (1-60 minutes, default 15) to prevent repetitive alerts for the same source (such as during a prolonged network outage).
6. **Do Not Disturb (DND)**: Schedule a quiet window to suppress all desktop alerts.

---

## 3. Tracking Subscription Coding Tools

AI Usage Monitor tracks usage limits for local AI coding tools that do not have public quota APIs.

1. Open **Settings → Subscriptions**.
2. Enable the tools you use:
   * **Claude Code**: Tracks session limits (5-hour window) and rolling weekly limits.
   * **Codex CLI**: Tracks 5-hour rolling windows.
   * **GitHub Copilot**: Tracks monthly limits (resets 1st of each month UTC).
3. Select your subscription plan tier (e.g., Free, Pro, Pro+, Business, Enterprise) to populate the limit windows automatically.
4. Check the **Detection Badge** (detected / not found) to confirm the widget successfully identified the local binary or config paths (such as `~/.claude/` or `~/.codex/`).

### GitHub Copilot Seat Metrics
For enterprise or team accounts, you can sync seat assignment and monthly metrics using the GitHub API:
1. Generate a GitHub Personal Access Token (PAT) with `manage_billing:copilot` scope.
2. In **Settings → Subscriptions → GitHub API**, enter your token and organization name.
3. The widget will query the official Copilot billing endpoint to display live seat usage.

---

## 4. Configuring Browser Sync Labs (Experimental)

Browser Sync Labs allows the widget to read session cookies from your browser to pull real-time quota data from web dashboards.

### Prerequisites
* Firefox browser with an active, logged-in session on [claude.ai](https://claude.ai) or [chatgpt.com](https://chatgpt.com).
* Secure KWallet subsystem running (needed to decrypt stored browser profile links).

### Setup Steps
1. Open **Settings → Subscriptions → Browser Sync**.
2. Enable sync and select your active **Firefox profile**.
3. Choose **Save**.
4. The widget will locate Firefox's `cookies.sqlite` database locally, retrieve the required session cookies, and make direct, read-only requests to:
   * **Claude Code**: Syncs session percentage used, weekly limits, and extra usage spending from the internal API.
   * **Codex CLI**: Syncs 5-hour usage, weekly limits, code reviews, and remaining credits.

> [!CAUTION]
> Browser Sync uses undocumented internal APIs. If the vendors update their web interfaces, Browser Sync might fail. The widget does not export your cookies, nor does it write them to logs or the history database.

---

## 5. Webhook Integrations (Slack & Discord)

You can forward budget and rate-limit alerts directly to Slack or Discord.

1. Open **Settings → Alerts**.
2. Scroll to the **Webhooks** section.
3. Enable **Slack** or **Discord**.
4. Paste your incoming webhook URL.
5. Set a cooldown limit to avoid spamming your channels.
6. Webhook URLs are saved securely inside **KWallet**.
