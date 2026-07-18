# Getting started

Use **Guided first success** to configure and verify one provider or detected local tool before adding budgets or integrations.

1. Add **AI Usage Monitor** from Plasma's widget picker.
2. Open the widget and choose what you want to track first.
3. Choose one recommended source and review its monitoring level.
4. Enter only its required fields.
5. Choose **Save and verify** or **Enable and verify**.
6. Read whether the result is provider usage or spend, gateway data, a balance, a local estimate, or connectivity only.

Keys are stored in KWallet. Plasma may ask you to unlock the wallet.

Guided setup uses the safe scheduled read-only request and does not run inference. A connectivity-only result confirms access but does not prove usage or spend.

To add another source, open **Settings → Providers**. Search or filter the list, select one source, apply its settings, then choose **Verify**. Cancelling Settings discards staged KWallet changes.

Overview keeps reporting providers separate from collapsed connection checks. Native Diagnostics shows matching frontend and plugin versions, install layers, database health, source readiness, KWallet, and catalogs.

Read [Understanding the data](Understanding-the-Data) before configuring budgets.

The full flow is in the [first successful source guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/getting-started.md).
