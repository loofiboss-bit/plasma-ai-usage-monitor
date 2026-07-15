# Gemini Developer API setup

This profile is for an AI Studio / Gemini Developer API key. It is deliberately separate from Google Cloud and Vertex telemetry.

1. Create an API key in Google AI Studio and store it through the widget's KWallet-backed key field.
2. Select a discovered model or type a pinned custom model ID.
3. Use **Refresh models** when you need to bypass the 24-hour discovery cache.

## Scheduled traffic

The widget sends read-only `GET /v1beta/models` requests, including pagination when supplied by Google. The API key is placed in Google's required query parameter, is never written to diagnostics, and the shipped catalog remains available if discovery fails.

`countTokens` is an explicit diagnostic only. The profile never turns published RPM/TPM/RPD caps into live remaining quota. Active limits remain visible in Google AI Studio.
