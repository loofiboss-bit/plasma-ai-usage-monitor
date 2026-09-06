# Subscription catalog review — 2026-09-05

The 30-day freshness gate blocked both incoming maintenance PRs because the
catalog was last reviewed on July 18. The gate remains unchanged. This review
updates source evidence and corrects claims that no longer match current sources.

| Tool | Reviewed evidence and decision |
| --- | --- |
| Antigravity | [Plans](https://antigravity.google/docs/plans?hl=en), [models](https://antigravity.google/docs/models), and [plan changes](https://antigravity.google/blog/changes-to-antigravity-plans): keep dynamic daemon-reported identity and quota; no numeric preset. |
| Claude Code | [Pricing](https://claude.com/pricing), [Code access](https://support.claude.com/en/articles/11145838-use-claude-code-with-your-pro-or-max-plan), [Pro](https://support.claude.com/en/articles/8325606-what-is-the-pro-plan), and [Max](https://support.claude.com/en/articles/11049741-what-is-the-max-plan): retain monthly prices and relative session allowances. Weekly reset is account-assigned, not seven days after local session start. |
| Codex | [Plan inclusion](https://help.openai.com/en/articles/11369540-using-codex-with-your-chatgpt-plan), [Pro tiers](https://help.openai.com/en/articles/9793128-about-chatgpt-pro-tiers), and [credits](https://help.openai.com/en/articles/12642688-using-credits-for-flexible-usage-in-chatgpt-freegopluspro): retain confirmed Pro prices and general plan multipliers. Remove unconfirmed temporary 25x/10x Codex numeric allowances. |
| Copilot | [Plans](https://docs.github.com/en/copilot/get-started/plans), [request billing](https://docs.github.com/en/copilot/concepts/billing/copilot-requests), and [current billing](https://docs.github.com/en/copilot/concepts/billing/billing-for-individuals): current plans use AI credits. Remove premium-request caps from current plan presets; do not reinterpret requests as credits. Retain the separately dated billing-mode records. |
| Cursor | [Pricing](https://cursor.com/pricing) and [models/pricing](https://cursor.com/docs/models-and-pricing): retain prices; describe relative allowance as Agent usage rather than a fixed provider list. |
| Windsurf | [Pricing](https://windsurf.com/pricing), [usage](https://docs.windsurf.com/windsurf/accounts/usage), [quota](https://docs.windsurf.com/windsurf/accounts/quota), and [Devin pricing](https://devin.ai/pricing): redirects and existing account terms require continued manual review. Remove a misleading total team price: the current offer includes both a base fee and seat charges. |
| JetBrains | [Plans and usage](https://www.jetbrains.com/help/ai-assistant/licensing-and-subscriptions.html): retain personal/commercial ranges; pooled organizational allowances cannot be inferred for an individual account. |

No customer credentials or live subscription accounts were used. This is public
catalog evidence, not proof of any user's entitlement. Native contract tests
assert that Copilot presets no longer provide a numeric request cap.
