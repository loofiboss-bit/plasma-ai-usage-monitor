# Understand the data

Provider cards answer different questions because provider APIs expose different information. Check the source and quality labels before comparing providers or setting a budget.

## Monitoring levels

| Level | What the widget can establish |
| --- | --- |
| Actual usage and spend | The provider reports account usage, billing, or both |
| Actual key usage | The provider reports usage attached to the current API key |
| Gateway aggregate | A gateway such as LiteLLM reports traffic and spend that passed through it |
| Balance and connectivity | The provider reports a balance and confirms account access |
| Connectivity only | A read-only request confirms credentials or model access, but not usage or billing |

The generated [provider capability matrix](../provider-capabilities.md) is the exact contract used by the runtime.

## Source labels

- **Actual API usage** comes from a provider usage endpoint.
- **Billing API** means the provider supplied spend.
- **Estimated pricing** combines observed tokens with the reviewed local pricing catalog.
- **Response headers** contain rate-limit values returned with a request.
- **Local tool data** comes from local files and counters.
- **Browser Sync Labs** comes from an authenticated browser or CLI session.
- **Published documentation** describes a plan or cap, not live remaining quota.

An estimate is useful for trends but is not an invoice. A connectivity check proves that an endpoint answered; it does not prove that usage is zero.

## Unknown and zero

**Unknown** means the source did not provide a compatible value. Zero means the source explicitly reported zero. The widget preserves that distinction in the UI, database, exports, alerts, and Prometheus output.

## Currency handling

The widget does not convert currencies. It keeps observations in their reported ISO currency and does not silently add USD and EUR. USD budgets turn off when the observed data uses another currency.

LiteLLM spend logs may contain several currencies. Each remains separate.

## Time windows

Daily, weekly, monthly, rolling, and cumulative values are not interchangeable. History keeps the source window and aggregation meaning. A rolling provider value is not relabeled as calendar-day spend.

## Background traffic

Scheduled provider calls are read-only and do not run inference. The Trust Center lists the scheduled endpoint and request budget for each provider.

Some settings pages offer an explicit manual inference test. That action may use quota or incur a small provider charge. It does not run on the background schedule.

## Budgets

Set budgets only when the card has compatible spend data. A budget based on estimated pricing remains an estimate. Connectivity-only providers cannot produce a meaningful spend warning.
