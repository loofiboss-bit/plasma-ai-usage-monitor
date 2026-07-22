<!-- Generated from docs/user-guide/getting-started.md by scripts/generate_wiki_docs.py; do not edit. -->
# First successful source

Use **Guided first success** to configure and verify one provider or detected local tool. The final screen tells you whether the result is provider-reported usage or spend, gateway data, a balance, a local estimate, or connectivity only.

## Add the widget

1. Right-click the panel or desktop.
2. Choose **Add Widgets**.
3. Search for **AI Usage Monitor**.
4. Add it to the panel or desktop.
5. Open the widget. The guided setup starts on a new installation.

If setup was skipped earlier, reopen the widget and choose **Resume setup**. Full controls remain available in Settings.

## Complete Guided first success

1. Choose what you want to track first: a local coding tool, provider usage or spend, or a gateway or provider connection.
2. Choose one source. Sources that can return useful reporting data appear before connectivity-only checks.
3. Review the monitoring level and expected result.
4. Enter only the required credential and endpoint fields. Provider credentials are saved in KWallet when you choose **Save and verify**.
5. Run the verification. Provider verification uses the scheduled read-only request and never sends inference. Local-tool verification checks the detected activity path.
6. Read the result quality before opening the dashboard.

Connectivity-only verification is a successful connection test, not proof of token usage or spend. A local-tool result remains an estimate unless an authenticated source reports a live quota window.

## Add another source in Settings

Open **Settings → Providers**. Search or filter the source list, then select one source to open its detail pane. The pane shows its monitoring level, required permission, scheduled endpoint, enable control, credential fields, and safe verification action.

Apply pending changes before choosing **Verify**. Cancelling Settings discards staged credential changes. Applying writes or removes the staged KWallet values.

The **Advanced** switch shows model overrides, custom base URLs, and provider-specific fields. Leave it off for a first setup unless your provider needs a custom endpoint.

## Read Overview

Overview groups useful provider results under **Reporting providers**. Connectivity-only providers stay under the collapsed **Connection checks** section. Local coding tools have their own section, and sources that need configuration or recovery appear as actions near the top.

The header counts actual data, estimates, balances, connection checks, and sources that need attention separately. It does not count a connection check as reported usage.

## Check Diagnostics

Open **Settings → Diagnostics** after the first setup. Confirm:

- the frontend and native-plugin versions match
- the frontend and plugin come from the expected install layers
- the history database is healthy or not created yet
- no enabled source unexpectedly needs recovery
- KWallet and both catalogs are available

Diagnostics can copy the relevant repair command, version check, capability report, or redacted support report. Use the source-readiness action to select a provider that needs repair.

## Add safeguards later

After the first source works:

- set provider budgets only for compatible USD spend data
- enable notifications with a cooldown
- turn on history if you want trends
- enable exports, Prometheus, or webhooks only if you use them
