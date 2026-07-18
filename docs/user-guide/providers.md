# Configure providers

Open **Settings → Providers**, search or filter the source list, and select one source to configure. Usage and spend sources appear before connection checks. The detail pane shows one source at a time.

Use the monitoring-level filter to find usage and spend, detected local tools, gateways, balances, or connectivity-only sources. Enable a source, enter its required fields, apply the changes, then choose **Verify**. Verification is disabled while Settings has pending changes.

## Common fields

- **API key:** stored in KWallet, not in Plasma configuration files
- **Model:** used for model-specific estimates or manual tests
- **Custom base URL:** intended for a trusted proxy or gateway
- **Refresh interval:** configured under General; catalog minimums still apply

The default detail pane shows the monitoring level, required permission, scheduled read-only endpoint, and credential fields. Turn on **Advanced** only for model overrides, custom base URLs, or provider-specific fields.

Use HTTPS for remote endpoints. Plain HTTP is accepted only for loopback development addresses such as 127.0.0.1, localhost, and ::1.

## Providers with account data

### OpenAI

OpenAI usage and cost endpoints require an Admin API key. A normal project key can return a permission error even when it works for inference. The optional project ID narrows usage to one project.

### OpenRouter

OpenRouter reads key usage and remaining-limit fields from its key endpoint. The values belong to the configured key and may not represent every key in the account.

### LiteLLM Proxy

LiteLLM reads gateway spend logs. Enter the proxy base URL and a key that can access the spend endpoint. Keep each returned currency separate.

### DeepSeek

DeepSeek combines model discovery with the account balance endpoint. Balance is not the same as historical spend.

## Connectivity-only providers

Anthropic, Gemini, Mistral, Groq, xAI, Ollama Cloud, Together, Cohere, Google Veo, Azure OpenAI, AWS Bedrock, Cerebras, Fireworks, and Perplexity may confirm credentials or model access without returning account usage or billing.

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
- **403:** the key lacks the required role or account permission. OpenAI account usage commonly fails here when the key is not an Admin key.
- **404:** the base URL, API version, deployment, or model is wrong.
- **429:** the provider rate-limited the request. The scheduler honors retry guidance and backs off.
- **TLS or host error:** remove a custom base URL and retry the default endpoint.

Open **Diagnostics** to confirm the frontend and plugin match, inspect the typed source-readiness error, and select the affected provider before changing unrelated settings.
