# Configure sources

Open **Settings → Sources** to configure API services and AI tools in one searchable list. Use **All**, **Enabled**, or **Detected tools** to narrow it; active sources appear first within **AI tools** and **API services**. The detail pane shows one source at a time.

Select a source to see what it reports, the permission it needs, and its required fields. Enable it, enter credentials, and choose **Apply** before **Verify**. Verification stays disabled while configuration or credential changes are pending. Cancel discards those staged changes; failed Wallet writes remain available for another Apply attempt.

## Common fields

- **API key:** stored in KWallet, not in Plasma configuration files
- **Model:** used for model-specific estimates or manual tests
- **Custom base URL:** intended for a trusted proxy or gateway
- **Refresh interval:** configured under General; catalog minimums still apply

Each detail pane follows the same order: source name and status, enable control, connection and data source, plan or pricing tier, quota alerts, verification, and advanced overrides. Plan, billing mode, quota alerts, and Antigravity quota refresh are available without Advanced settings. **Advanced settings** is controlled from **Settings → General**; when enabled, each source has a local expandable section for provider-specific options. Antigravity quota refresh and source verification are explicit actions.

Browser Sync has a separate **Browser sync** section on Sources. It remains off until enabled. Browser, profile, and refresh changes are staged with the other source settings; choose **Apply** before testing a Claude or ChatGPT session. Cancel restores the saved browser settings.

Use HTTPS for remote endpoints. Plain HTTP is accepted only for loopback development addresses such as 127.0.0.1, localhost, and ::1.

## API services with account data

### OpenAI

OpenAI usage and cost endpoints require an Admin API key. A normal project key
can return a permission error even when it works for inference. The optional
project ID narrows usage to one project.

Usage and daily/monthly cost reports follow every cursor with a fixed six-page,
18-attempt ceiling. The widget publishes a new total only after every page
completes; a partial failure retains the prior complete value as stale or makes
the value explicitly unavailable. Daily usage pages request at most 31 buckets
and cost pages at most 180 buckets.

Usage grouping can expose model and project detail. Cost grouping exposes
project and line item only; the widget never infers model-level OpenAI cost.

### Anthropic

Anthropic uses two independent credentials. A standard API key verifies model
access. An optional Admin API key reads organization message-usage and cost
reports. Configure either credential or both; adding the Admin key never
replaces the standard key.

The scheduled Admin requests are read-only. Startup, manual verification, and
credential changes may backfill at most 31 UTC daily buckets; normal refreshes
cover the current and previous UTC day and run no more often than every five
minutes. Usage and cost can succeed independently. Partial or failed refreshes
keep the previous value visibly stale instead of replacing it with zero.
Anthropic cost is parsed as integer micro-USD. Model, workspace, service-tier,
and reported cost-line-item scopes remain attached to local detail observations
without being added to the aggregate total a second time.

Only complete scoped refreshes replace the previous scoped set. A failed page
retains the last complete rows as stale, while a later successful refresh
removes projects or periods no longer returned.

### OpenRouter

OpenRouter reads key usage and remaining-limit fields from its key endpoint. The values belong to the configured key and may not represent every key in the account.

### LiteLLM Proxy

LiteLLM reads gateway spend logs. Enter the proxy base URL and a key that can access the spend endpoint. Keep each returned currency separate.

### DeepSeek

DeepSeek combines model discovery with the account balance endpoint. Balance is not the same as historical spend.

## Connectivity-only providers

Gemini, Mistral, Groq, xAI, Ollama Cloud, Together, Cohere, Google Veo, Azure OpenAI, AWS Bedrock, Cerebras, Fireworks, and Perplexity may confirm credentials or model access without returning account usage or billing. Anthropic remains connectivity-only when only its standard key is configured.

Perplexity's current model discovery is public, so the card can prove endpoint availability without a key. It still cannot infer account usage from that response.

Additional field notes:

- **Azure OpenAI:** provide the resource endpoint, deployment, API key, and compatible API version.
- **AWS Bedrock:** provide access key ID, secret access key, optional session token, region, and model.
- **Google Veo:** published limits stay separate from live remaining quota.

## Detailed setup notes

- [Gemini](../provider-setup/gemini.md)
- [LiteLLM Proxy](../provider-setup/litellm.md)
- [Cerebras](../provider-setup/cerebras.md)
- [Fireworks AI](../provider-setup/fireworks.md)
- [Perplexity](../provider-setup/perplexity.md)

The runtime-generated [capability matrix](../provider-capabilities.md) lists every scheduled call and monitoring level.

## Connection errors

- **401:** the key is missing, expired, revoked, or sent to the wrong endpoint.
- **403:** the key lacks the required role or account permission. OpenAI and Anthropic account reporting require their respective Admin credentials.
- **404:** the base URL, API version, deployment, or model is wrong.
- **429:** the provider rate-limited the request. The scheduler honors retry guidance and backs off.
- **TLS or host error:** remove a custom base URL and retry the default endpoint.

Open **Settings → Diagnostics** to confirm the frontend and plugin match, inspect the typed source-readiness error, and choose the affected source before changing unrelated settings.
