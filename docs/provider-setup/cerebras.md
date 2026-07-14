# Cerebras Inference setup

1. Store a Cerebras API key in KWallet.
2. Select a shipped model or a model returned by live discovery.
3. Dedicated-endpoint metrics require a separately scoped future profile; the general key is not silently granted broader access.

## Scheduled traffic

The general adapter sends one authenticated, read-only `GET /v1/models` request. It proves connectivity and model availability only. Rate-limit headers are recorded from legitimate responses when a complete limit/remaining pair exists; no background completion is generated.

## Provider card

The canonical v13 provider screenshot demonstrates that Cerebras connectivity is
reported without inventing usage or cost: [provider intelligence screenshot](../../assets/screenshots/provider-intelligence.png).
