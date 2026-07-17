# First setup

Start with one provider or one local tool. Confirm that its card reports the expected source before enabling budgets, exports, or webhooks.

## Add the widget

1. Right-click the panel or desktop.
2. Choose **Add Widgets**.
3. Search for **AI Usage Monitor**.
4. Add it to the panel or desktop.
5. Right-click it and open **Configure AI Usage Monitor**.

## Choose a simple preset

The **General** page includes presets for common panel layouts. You can change the default refresh interval and choose whether the panel shows an icon, cost, provider count, daily cost, remaining requests, or the most critical provider.

Provider descriptors enforce a minimum safe refresh interval. A shorter global setting does not make a provider exceed its catalog request budget.

## Connect an API provider

1. Open **Providers**.
2. Enable the provider.
3. Enter the required key or account fields.
4. Leave the default model and base URL unchanged for the first test.
5. Apply the settings.
6. Open the widget and refresh.

Keys are stored in KWallet. If KWallet is closed, Plasma may ask you to unlock it.

The provider card may show connectivity without usage or spend. Read [Understanding the data](understanding-data.md) before treating an empty metric as an error.

## Enable a local coding tool

Open **Subscriptions** and enable the installed tool. The detection label shows whether its binary or state directory was found. Plan limits are local estimates unless a supported live source supplies quota windows.

## Check Diagnostics

Open **Diagnostics** after the first setup. Confirm:

- the displayed version matches the installed package
- the loaded plugin path belongs to the same installation
- KWallet is open
- the enabled provider has no authentication or configuration error
- provider and subscription catalogs are loaded

Use **Copy support report** if something is wrong. The report redacts secrets and identifying endpoint details.

## Add safeguards later

After the first source works:

- set provider budgets only for compatible USD spend data
- enable notifications with a cooldown
- turn on history if you want trends
- enable exports, Prometheus, or webhooks only if you use them
