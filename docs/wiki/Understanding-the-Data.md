<!-- Generated from docs/user-guide/understanding-data.md by scripts/generate_wiki_docs.py; do not edit. -->
# Understand the data

Provider cards answer different questions because provider APIs expose different information. Check the source and quality labels before comparing providers or setting a budget.

Overview puts providers with useful reported metrics under **Reporting providers**. Providers that only prove endpoint access stay under the collapsed **Connection checks** section. Local tools appear separately. The header counts actual data, estimates, balances, connectivity, and sources that need attention without merging them into one success number.

The panel and popup footer use the same source summary. **Active sources** means
sources with verified actual data, an estimate, or a balance; connectivity-only
and needs-attention sources are called out separately. A verified local tool can
therefore be active even when no API provider is configured.

## Monitoring levels

| Level | What the widget can establish |
| --- | --- |
| Actual usage and spend | The provider reports account usage, billing, or both |
| Actual key usage | The provider reports usage attached to the current API key |
| Gateway aggregate | A gateway such as LiteLLM reports traffic and spend that passed through it |
| Balance and connectivity | The provider reports a balance and confirms account access |
| Connectivity only | A read-only request confirms credentials or model access, but not usage or billing |

The generated [provider capability matrix](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/provider-capabilities.md) is the exact contract used by the runtime.

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

Compact cost modes include only available provider-reported spend and keep each
currency separate. Remaining requests show an em dash when no compatible metric
exists and `0 req` only when a source explicitly reports zero.

The Analyst activity heatmap uses a neutral cell for both missing days and
explicit zero activity. Hover text distinguishes them: only a missing day says
**No recorded data**. The **Output / Input Ratio** is descriptive, not a score of
prompt quality. It appears only when compatible snapshots contain positive input
tokens; a reported zero output remains a valid ratio of zero.

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
