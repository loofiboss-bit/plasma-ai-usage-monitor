# LiteLLM Proxy setup

LiteLLM is a gateway integration. It can report aggregate spend for traffic that passed through the configured proxy.

In **Settings → Providers**:

1. enable LiteLLM Proxy
2. enter the proxy base URL
3. enter a key that can read the spend endpoint
4. apply settings and refresh

Use HTTPS for a remote proxy. Plain HTTP is allowed only for loopback development endpoints.

## Scheduled traffic

The widget reads the gateway spend log endpoint. It does not run inference and does not contact the upstream model providers directly.

Spend can contain several currencies. The widget preserves each currency and does not add them together.

## Common failures

- **401 or 403:** the gateway key cannot read spend logs.
- **404:** the base URL does not point to a compatible LiteLLM proxy or the endpoint is disabled.
- **Empty data:** no compatible spend rows exist for the gateway, time range, or key.

Check LiteLLM access policy before replacing a working inference key with a broader administrative key.
