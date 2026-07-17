# Cerebras Inference setup

Create an API key in the Cerebras Cloud console. In **Settings → Providers**, enable Cerebras, store the key, and keep the default base URL for the first refresh.

## Scheduled traffic

The widget performs read-only model discovery. A successful response confirms that the key and endpoint work. Cerebras does not supply account usage or spend through this request, so those values remain unknown.

The shipped fallback model list is available when discovery fails, but a fallback model is not evidence of connectivity.

## Common failures

- **401 or 403:** the key is invalid or lacks access.
- **404:** the custom base URL is wrong.
- **Connected with unknown spend:** expected for the current monitoring level.
