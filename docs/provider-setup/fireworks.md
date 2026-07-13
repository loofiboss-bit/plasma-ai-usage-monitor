# Fireworks AI setup

1. Store a Fireworks API key in KWallet.
2. Enter the account-scoped HTTPS base URL, for example the documented `/v1/accounts/{account_id}` base.
3. Select a discovered or pinned model.

## Scheduled traffic

The adapter sends one read-only `GET /models` request under the configured account base. It reports connectivity and model discovery only.

Scheduled billing/spend collection is intentionally deferred until a real account fixture proves a stable read-only permission contract. The widget does not substitute an inference probe or undocumented dashboard endpoint.
