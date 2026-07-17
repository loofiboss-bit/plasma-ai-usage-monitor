# Fireworks AI setup

Create an API key in the Fireworks AI account settings. In **Settings → Providers**, enable Fireworks, store the key, and enter the account-scoped base URL:

~~~text
https://api.fireworks.ai/v1/accounts/{account_id}
~~~

Replace `{account_id}` with your Fireworks account ID. Fireworks has no default endpoint in the widget, so the first refresh requires this field.

## Scheduled traffic

The widget performs read-only model discovery. It can confirm credentials and model access, but it does not report account spend.

Fireworks billing remains disabled until a read-only billing contract is covered by deterministic fixtures. The widget does not estimate a live balance from documentation.

## Common failures

- **401 or 403:** the key is invalid or lacks model access.
- **404:** the base URL is not a compatible Fireworks endpoint.
- **Connected with unknown usage:** expected for the current monitoring level.
