# Understanding the data

The source label matters more than the size of the number.

Overview places useful provider results under **Reporting providers** and keeps connectivity-only providers under the collapsed **Connection checks** section. Local tools appear separately. Its header counts actual data, estimates, balances, connection checks, and sources that need attention without treating them as the same outcome.

| Label | Meaning |
| --- | --- |
| Actual usage or billing | The provider supplied account data |
| Key usage | The provider supplied data for the configured key |
| Gateway aggregate | A proxy such as LiteLLM supplied totals |
| Balance | The provider supplied credit or account balance |
| Connectivity only | A read-only request confirmed endpoint or model access |
| Estimated | The widget combined observed tokens with catalog pricing |
| Unknown | The source did not provide a compatible value |

Unknown is different from zero. Connectivity is different from usage. A price estimate is different from an invoice.

The widget keeps currencies and time windows separate. It does not silently add different currencies or relabel rolling totals as calendar-day spend.

Scheduled provider calls are read-only. An explicit manual inference test may consume quota or money.

See the [full data guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/understanding-data.md) and generated [provider matrix](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/provider-capabilities.md).
