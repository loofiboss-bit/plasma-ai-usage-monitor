# Gemini Developer API setup

Create a Gemini Developer API key in [Google AI Studio](https://aistudio.google.com/apikey). In **Settings → Providers**, enable Google Gemini and store the key.

Keep the default endpoint unless you use a trusted compatible gateway. Choose a model from the shipped catalog or enter a custom model ID when the provider has released one that the catalog does not yet list.

## Scheduled traffic

The widget uses read-only model discovery and caches successful discovery for 24 hours. This confirms that the key can access the Gemini API. It does not report account spend or live remaining quota.

Published tier caps remain documentation, not live measurements. The card keeps unavailable values marked as unknown.

The explicit token-count diagnostic is separate from scheduled refresh.

## Common failures

- **400:** the model, request parameter, or custom base URL is invalid.
- **401 or 403:** the key is invalid, restricted, or not allowed to use the API.
- **429:** the project or key reached a provider limit.

Remove a custom base URL before debugging the key itself.
