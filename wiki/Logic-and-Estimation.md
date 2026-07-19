# Logic and Cost Estimation

This page explains the under-the-hood mechanics of **AI Usage Monitor**: API query patterns, cost estimation, rate-limit parsing, KWallet storage, and local database operations.

---

## 1. Provider API Mechanics

AI Usage Monitor queries endpoints on a background schedule. To preserve privacy and prevent quota consumption, scheduled calls are strictly read-only and **never execute user prompts or inference jobs**.

The widget supports different levels of reporting depending on what the provider exposes:

| Monitoring Level | Target Endpoints | Data Retrieved |
|---|---|---|
| **Actual usage and spend** | `/organization/usage/completions`, `/organization/costs` | Historical token counts and exact dollar billing records. |
| **Actual key usage** | OpenRouter Key endpoint | Quota consumed by the configured key. |
| **Gateway aggregate** | LiteLLM spend logs | All proxy traffic and spend across multiple currencies. |
| **Balance and connectivity** | `/user/balance` (DeepSeek) | Prepaid credit balance and API responsiveness. |
| **Connectivity only** | `/v1/models` or minimal completions test | Checks model availability or keys without returning usage. |

---

## 2. Cost Estimation Engine

Since most providers (like Anthropic, Gemini, or Groq) do not offer a public usage or billing API, the widget includes an automatic cost estimator inside its C++ backend.

### The Estimation Formula
When a provider returns token consumption counts, the widget computes the cost using the local model pricing catalog:

$$\text{Estimated Cost} = (\text{Input Tokens} \times \text{Input Price Per Token}) + (\text{Output Tokens} \times \text{Output Price Per Token})$$

### Catalog Matching Logic
1. **Model Name Matching**: The engine checks the model name configured in the settings.
2. **Exact Match**: Looks for an exact match in the internal pricing table (containing ~30 major models updated for 2026 pricing).
3. **Prefix Match**: If no exact match is found, the engine falls back to prefix matching (e.g., if you specify `gpt-4o-2024-08-06`, it will match the `gpt-4o` prefix rule).
4. **Unknown Flag**: If no prefix match exists, the cost is flagged as **Unknown** rather than displaying a false zero.

---

## 3. Rate-Limit Parsing

The widget tracks your remaining request and token rate limits by parsing HTTP response headers returned during background refreshes.

* **OpenAI & compatible providers**: Parses `x-ratelimit-remaining-requests` and `x-ratelimit-remaining-tokens`.
* **Anthropic**: Parses `anthropic-ratelimit-requests-remaining` and `anthropic-ratelimit-tokens-remaining`.
* **Google Gemini**: Lacks rate limit headers. The widget displays static documentation limits for free tiers.

---

## 4. SQLite History Database

Usage history is recorded locally in an SQLite database.

* **Database Path**: `~/.local/share/plasma-ai-usage-monitor/usage_history.db`
* **Performance (WAL Mode)**: The database is configured with **Write-Ahead Logging (WAL)**. This allows concurrent read/write operations so that background C++ update tasks do not lock or freeze the QML UI.
* **Auto-Pruning**: A background retention routine runs on widget startup and after scheduled intervals. It deletes entries older than the configured retention period (7 to 365 days, default 90).
* **Data Integrity**: If a provider connection fails, the widget marks the data as **Unknown** instead of inserting a zero, ensuring that charts do not show false dips in spending history.

---

## 5. Secure Key Storage (KWallet)

The widget enforces strict security boundaries for credentials.

* **Zero Plaintext Storage**: API keys, Personal Access Tokens (PATs), Firefox session paths, and webhook URLs are never written to the Plasma configuration files on disk.
* **KWallet Integration**: Credentials are saved inside the KWallet folder `"ai-usage-monitor"`.
* **Asynchronous Access**: The C++ CSecretsManager opens KWallet asynchronously to prevent blocking the Plasma desktop shell if a password prompt is delayed.
* **No Fallback**: If the wallet subsystem is disabled or locked, the widget will not fall back to insecure plaintext storage. The card will display a recovery warning instead.
