# Perplexity API setup

The launch profile is capability-limited and does not require a key for public model discovery. Choose an Agent API model returned by discovery or pin a custom model ID. Sonar remains available only as a compatibility choice where the account supports it.

## Scheduled traffic

The adapter sends one public, read-only `GET /v1/models` request. It reports connectivity and model availability only. Request usage and cost exist only for real requests made by the user; the widget never generates a background inference request to manufacture those values.

## Provider card

The canonical v13 overview includes Perplexity's public discovery-only state and
explicit unavailable metrics: [provider overview screenshot](../../assets/screenshots/main-window.png).
