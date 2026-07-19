# Privacy and Security

AI Usage Monitor is designed to be **local-first and privacy-focused**. It has no telemetry, cloud accounts, external aggregation backends, or diagnostic trackers. All information remains on your local machine.

---

## 1. Local Data Boundaries

The widget stores its configuration and data in separate, restricted local layers:

| Data Type | Storage Location | Security Level |
|---|---|---|
| **API Keys & Webhook URLs** | KDE KWallet folder `"ai-usage-monitor"` | **Encrypted** (System-level protection) |
| **Usage History Database** | `~/.local/share/plasma-ai-usage-monitor/usage_history.db` | **Plaintext** (Restricted to owner account permissions) |
| **Widget Preferences** | Standard Plasma KConfig configurations | **Plaintext** (Restricted to owner account permissions) |
| **Export Files** | Selected local directory | **Plaintext** (Owner-controlled permissions) |

> [!IMPORTANT]
> Configuration backup exports generated through Settings **explicitly exclude** secret data such as API keys, session tokens, browser cookies, and webhook URLs. Secret credentials must be configured manually when migrating to a new machine.

---

## 2. KWallet Integration

There is **no fallback plaintext storage** for secrets. If KWallet is disabled, locked, or unavailable:
* The C++ plugin queue halts credentials retrieval.
* Affected provider cards display a configuration warning.
* Plaintext keys are never cached to disk or written to Plasma's standard config files.

---

## 3. Network Traffic Rules

1. **Direct Communication**: Enabled provider backends connect directly from your machine to the provider's API. Requests do not pass through intermediate servers.
2. **Read-Only Scheduled Polling**: Scheduled polling retrieves usage data only and does not execute prompt completions or inference tests.
3. **HTTPS Enforcement**: Custom base URLs for proxies or gateways must use secure `https://` protocols. Plain `http://` is strictly blocked unless the destination is a loopback address (e.g., `127.0.0.1`, `localhost`, or `::1`).
4. **Local Prometheus Binding**: The optional Prometheus server binds exclusively to the local loopback address (`127.0.0.1`). It is not exposed to other external network interfaces.

---

## 4. Browser Sync Labs Data Safety

When Browser Sync Labs is enabled to pull Claude Code or Codex CLI quotas:
* The widget copies the browser's `cookies.sqlite` database to a temporary directory with owner-only access permissions (`0600`).
* Session cookies are extracted into memory and used to verify quotas.
* **Security boundary**: Extracted browser cookies remain in volatile RAM. They are never written to log files, local history tables, support reports, or settings files.

---

## 5. Google Antigravity Daemon Security

The integration with the **Google Antigravity 2.x** client is designed with strict local validation rules:
* **Loopback only**: The daemon must run on `127.0.0.1` and be owned by the active desktop user.
* **Certificate pinning**: The widget pins and validates the local TLS certificate packaged with the Antigravity installation.
* **CSRF Token protection**: The CSRF security token is held purely in-memory and is never logged, stored in history, or exported.
* **Read-only queries**: The widget only queries account plan and quota endpoints (`GetUserStatus` and `RetrieveUserQuotaSummary`). It has no access to conversations, prompts, history logs, or OAuth credentials.

---

## 6. Redacted Support Reports

If you generate a diagnostic support report to submit with a bug report:
* The C++ plugin automatically redacts credentials, project IDs, account IDs, host names, and API query strings.
* Always review the support report output before posting it on public forums.
