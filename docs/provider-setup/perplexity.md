# Perplexity API setup

Enable Perplexity in **Settings → Providers**. The current model discovery route is public, so scheduled connectivity checks do not require a key.

Choose a shipped model for display or manual testing. A custom base URL should normally remain empty.

## Scheduled traffic

The widget reads the public model list. A successful response proves that the endpoint is available. It does not prove account access, usage, billing, or remaining quota.

Any explicit inference test is separate from the background refresh and requires suitable credentials. It may consume quota or money.

## Common failures

- **Network or TLS error:** the public endpoint could not be reached.
- **404:** a custom base URL points to an incompatible service.
- **Connected with unknown usage:** expected for the current monitoring level.
