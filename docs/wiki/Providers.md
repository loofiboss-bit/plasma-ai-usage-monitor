# Providers

Open **Settings → Providers**, search or filter the source list, and select one source. The detail pane shows its monitoring level, required permission, scheduled endpoint, enable control, credentials, and safe verification action.

Apply pending changes before choosing **Verify**. Turn on **Advanced** only for model overrides, custom base URLs, or provider-specific fields. Cancelling Settings discards staged credential changes.

OpenAI, OpenRouter, LiteLLM, and DeepSeek can expose account, key, gateway, or balance data. Most other supported providers currently confirm credentials or model access without returning account usage or billing.

Common requirements:

- provider key stored in KWallet when the API requires one
- model from the reviewed catalog when the provider uses a model field
- default HTTPS endpoint or a provider-specific custom endpoint
- additional deployment, region, or project fields for Azure, AWS, or OpenAI

LiteLLM requires the URL of your proxy. Fireworks requires an account-scoped base URL such as `https://api.fireworks.ai/v1/accounts/{account_id}`. Neither integration has a default endpoint in the widget.

OpenAI account usage needs an Admin API key. A normal inference key can return 403.

Custom remote endpoints must use HTTPS. Use plain HTTP only for loopback development.

Common response codes:

- 401: invalid or missing credential
- 403: insufficient permission
- 404: wrong base URL, deployment, API version, or model
- 429: provider rate limit

Provider-specific notes and the exact monitoring contract live in the [provider guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/providers.md).
