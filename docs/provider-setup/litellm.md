# LiteLLM Proxy setup

1. Enter the HTTPS base URL of your LiteLLM Proxy. Loopback HTTP is accepted for an explicitly local gateway.
2. Store a Proxy API key in KWallet.
3. Enable the provider after confirming that the key may read spend logs.

## Scheduled traffic

The gateway adapter sends one read-only `GET /spend/logs` request per refresh. It reports aggregate input/output tokens, requests, and spend returned by the gateway. Currency buckets remain separate; mixed currencies are never summed.

The widget does not execute proxy-provided code and does not make upstream inference calls.
