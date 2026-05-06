# Codex Task Brief: Plasma AI Usage Monitor v8.0.0

Repository: `loofiboss-bit/plasma-ai-usage-monitor`
Target release: `v8.0.0`
Date prepared: `2026-05-06`
Primary goal: make subscription limits, pricing, provider/model metadata, and UI quota display accurate, source-backed, maintainable, and testable.

Use this file as the implementation brief for Codex. Do not treat the numeric catalog examples below as permanent truth unless they are re-verified against the linked official sources during implementation. If source verification is not possible, keep the field as `needsManualReview` and do not present it as exact in the UI.

---

## 0. Required outcome

Implement v8.0.0 as a data-accuracy and subscription-visibility release.

The release must:

1. Move subscription pricing and quota data out of hardcoded monitor C++ methods and into a versioned JSON catalog.
2. Replace or migrate provider/model pricing catalog v2 to a richer v3 schema.
3. Make the subscription UI show all relevant quota windows per vendor/tool, including 5-hour, weekly, monthly, credits, extra usage, code review, and reset data when available.
4. Show the data source and precision for every quota/pricing row.
5. Prevent stale or unverified pricing/subscription data from silently shipping.
6. Preserve Plasma/KDE stability, KF6/Qt6 compatibility, KWallet usage, and current local-only design.
7. Avoid runtime scraping of vendor pricing pages.

---

## 1. First commands to run

Start by inspecting the current repo state. Do not modify files before this inventory.

```bash
git status --short
git branch --show-current
git log --oneline -5
find . -maxdepth 4 -type f | sort
```

Then locate version metadata and existing tests:

```bash
grep -R "VERSION 7.0.0\|7.0.0\|catalogVersion\|lastReviewed\|runtimeScraping" -n . || true
find . -iname '*test*' -o -iname 'CMakeLists.txt' | sort
```

Expected current architecture from prior review:

- Top-level `CMakeLists.txt` currently defines project version `7.0.0`.
- `plugin/CMakeLists.txt` includes provider backends and subscription monitors.
- `package/contents/catalog/providers-v2.json` exists and is tied to release `7.0.0`.
- `scripts/check_provider_catalog.py` exists and validates provider catalog freshness.
- Subscription tools are currently represented by classes such as:
  - `ClaudeCodeMonitor`
  - `CodexCliMonitor`
  - `CopilotMonitor`
  - `CursorMonitor`
  - `WindsurfMonitor`
  - `JetBrainsAiMonitor`
- `SubscriptionToolBackend` already supports primary, secondary, tertiary, credits, extra usage, cost, and sync state, but the UI does not render all of it in a complete normalized stack.

---

## 2. Non-negotiable constraints

Do not implement runtime scraping of vendor pricing pages.

Do not add more hardcoded subscription plan prices or quota values inside monitor C++ classes.

Do not show estimated or range-based data as exact. Every rendered quota/pricing row must include a source/precision badge.

Do not remove existing local activity monitoring or browser sync behavior unless replacing it with a tested equivalent.

Do not create a network dependency for normal Plasma widget rendering. Static catalog data must be shipped in the package. Browser sync remains optional and user-controlled.

Do not add large UI rewrites unless the existing widget remains stable in compact/narrow Plasma panels.

Do not silently fall back to stale catalog values. If stale, show a visible `Needs review` / `Catalog stale` state.

---

## 3. Current correctness issues to fix

### 3.1 Subscription data is hardcoded and incomplete

Several monitors hardcode plan names, prices, and limits. Move this to a new catalog:

- `plugin/claudecodemonitor.*`
- `plugin/codexclimonitor.*`
- `plugin/copilotmonitor.*`
- `plugin/cursormonitor.*`
- `plugin/windsurfmonitor.*`
- `plugin/jetbrainsaimonitor.*`

Current hardcoded values are not sufficient for v8 because current vendor plans have changed and several vendors now use credits, usage-based billing, ranges, or temporary promotions.

### 3.2 Provider catalog v2 is too narrow

`providers-v2.json` only stores `id`, `inputPerMillion`, and `outputPerMillion`. It cannot accurately represent:

- cached input pricing
- batch discounts
- context-dependent pricing
- video/image/generation pricing
- credit-based pricing
- provider-routed pricing such as OpenRouter
- source URLs and review dates per model
- unknown or non-applicable pricing

### 3.3 Subscription UI does not show enough data

`SubscriptionToolCard.qml` must be upgraded so it does not only show the primary usage bar. It must display all relevant usage windows and credits.

The user specifically requested that subscription information show essential details such as:

- 5-hour windows
- weekly limits
- plan-dependent limits
- vendor-specific pricing
- actual/synced data when available
- accurate and current information

---

## 4. Official sources reviewed for v8 planning

Re-check these sources before committing catalog data. Store reviewed dates in the catalog.

### OpenAI API pricing

URL: https://openai.com/api/pricing/

As reviewed on 2026-05-06:

- GPT-5.5: input `$5.00 / 1M tokens`, cached input `$0.50 / 1M tokens`, output `$30.00 / 1M tokens`.
- GPT-5.4: input `$2.50 / 1M tokens`, cached input `$0.25 / 1M tokens`, output `$15.00 / 1M tokens`.
- GPT-5.4 mini: input `$0.75 / 1M tokens`, cached input `$0.075 / 1M tokens`, output `$4.50 / 1M tokens`.
- These standard processing rates are stated for context lengths under 270K.

### OpenAI Codex with ChatGPT plans

URL: https://help.openai.com/en/articles/11369540-codex-in-chatgpt

As reviewed on 2026-05-06:

- Codex is included with ChatGPT Plus, Pro, Business, and Enterprise/Edu.
- For a limited time, Codex is included with Free and Go.
- Other plans have 2x Codex rate limits for a limited time.
- Codex limits vary by plan, task complexity, and whether work runs locally or in the cloud.
- ChatGPT file upload, image, video, and voice limits are separate from Codex limits.
- Treat exact limits as dynamic unless verified from an official current source.

### Claude / Claude Code

URLs:

- https://claude.com/pricing
- https://support.claude.com/en/articles/11145838-use-claude-code-with-your-pro-or-max-plan
- https://support.claude.com/en/articles/11014257-about-claude-s-max-plan-usage/
- https://support.claude.com/en/articles/8325606-what-is-claude

As reviewed on 2026-05-06:

- Claude Pro includes Claude Code and is listed as `$20/mo` monthly or `$17/mo` with annual billing.
- Claude Max starts at `$100/mo` and offers 5x or 20x more usage than Pro.
- Claude and Claude Code share usage limits; activity in either counts against the same capacity.
- Claude usage has session-based limits that reset every 5 hours.
- Max plans also have weekly limits, including all-model and Sonnet-specific weekly limits.
- Exact usable message/prompt counts vary by message length, project complexity, model, attachments, conversation length, and current capacity.

### GitHub Copilot billing

URLs:

- https://docs.github.com/en/copilot/reference/copilot-billing/models-and-pricing
- https://docs.github.com/en/billing/concepts/product-billing/github-copilot-premium-requests

As reviewed on 2026-05-06:

- GitHub Copilot is moving from request-based billing to usage-based billing starting `2026-06-01`.
- New model pricing is based on tokens and converted to GitHub AI Credits.
- `1 AI credit = $0.01 USD`.
- Legacy premium request allowances reset on the 1st of each month at `00:00:00 UTC`.
- Premium requests apply to premium models and features such as Chat, CLI, Code Review, Extensions, Spaces, Copilot cloud agent, and Spark SKUs.
- The app must support both legacy premium-request mode and new AI-credit mode, with a date-aware transition.

### Cursor pricing

URL: https://cursor.com/pricing

As reviewed on 2026-05-06:

- Hobby: Free, limited Agent requests and limited Tab completions.
- Pro: `$20/mo`.
- Pro+: `$60/mo`, 3x usage on OpenAI/Claude/Gemini models.
- Ultra: `$200/mo`, 20x usage on OpenAI/Claude/Gemini models.
- Teams: `$40/user/mo`.
- Enterprise: custom.
- Do not keep old hardcoded monthly request counts unless a current official source explicitly gives them.

### Windsurf pricing and usage

URLs:

- https://windsurf.com/redirect/windsurf/learn-pricing
- https://docs.windsurf.com/windsurf/accounts/usage
- https://docs.windsurf.com/windsurf/accounts/quota

As reviewed on 2026-05-06:

- Pricing page currently lists prompt credits such as Free `25 credits/mo`, Pro `$15/mo` with `500 credits/mo`, Teams `$30/user/mo` with `500 credits/user/mo`, and Enterprise variants.
- Docs also mention a March 2026 quota-based usage system with daily and weekly budgets.
- Treat Windsurf as `needsManualReview` or `sourceConflict` until the catalog records both the pricing-page credit model and the docs quota model clearly.

### JetBrains AI plans and usage

URL: https://www.jetbrains.com/help/ai-assistant/licensing-and-subscriptions.html

As reviewed on 2026-05-06:

- JetBrains AI uses AI Credits as monthly quota.
- AI Free: `3 AI Credits per 30 days`.
- AI Pro: source shows `$10 / 10 AI Credits per 30 days` and also a separate table with `$20 / 20 AI Credits per 30 days`.
- AI Ultimate: source shows `$30 / 35 AI Credits per 30 days` and also a separate table with `$60 / 70 AI Credits per 30 days`.
- AI Enterprise quota is not exactly disclosed.
- Each AI Credit corresponds to `$1 USD` charged in local currency.
- Quota renews every 30 days from first use.
- One AI Credit only roughly maps to request counts; do not show request counts as exact.
- Top-up AI Credits can be purchased and are valid for 12 months.

---

## 5. New data catalogs

### 5.1 Add `subscriptions-v1.json`

Create:

```text
package/contents/catalog/subscriptions-v1.json
```

Required top-level fields:

```json
{
  "schemaVersion": 1,
  "catalogVersion": "2026.05.06",
  "release": "8.0.0",
  "lastReviewed": "2026-05-06",
  "runtimeScraping": false,
  "notes": "Static local metadata for shipped subscription defaults. Browser sync may override with actual account data. Do not scrape vendor pages at runtime.",
  "tools": []
}
```

Each tool must use this shape:

```json
{
  "key": "claude-code",
  "label": "Claude Code",
  "vendor": "Anthropic",
  "dataQuality": "official-plus-browser-sync",
  "pricingFreshness": "reviewed",
  "needsManualReview": false,
  "sourceConflict": false,
  "sourceRefs": [
    {
      "label": "Claude pricing",
      "url": "https://claude.com/pricing",
      "reviewedAt": "2026-05-06"
    }
  ],
  "plans": []
}
```

Each plan must use this shape:

```json
{
  "id": "pro",
  "label": "Pro",
  "price": {
    "amount": 20,
    "currency": "USD",
    "period": "month",
    "precision": "official_exact"
  },
  "quotaWindows": [],
  "notes": []
}
```

Each quota window must use this shape:

```json
{
  "kind": "rolling_5h",
  "label": "5-hour session",
  "unit": "messages",
  "limit": 45,
  "rangeMin": null,
  "rangeMax": null,
  "reset": {
    "type": "rolling_duration",
    "durationHours": 5
  },
  "precision": "official_approx",
  "source": "official_preset",
  "visibleByDefault": true,
  "notes": []
}
```

Supported `kind` values:

```text
rolling_5h
rolling_weekly
rolling_30d
monthly_utc
monthly_billing_cycle
ai_credits
premium_requests
api_usage_usd
extra_spend
code_review
cloud_tasks
local_messages
prompt_credits
daily_budget
weekly_budget
custom
```

Supported `precision` values:

```text
browser_sync_actual
official_exact
official_range
official_approx
official_qualitative
provider_catalog
self_tracked_local
estimated
needs_manual_review
deprecated
unknown
```

Supported `source` values:

```text
browser_sync
official_preset
provider_api
user_config
local_activity
manual_entry
estimated
unknown
```

### 5.2 Example subscription catalog entries

These examples show shape and policy. Re-verify before committing.

```json
{
  "key": "cursor",
  "label": "Cursor",
  "vendor": "Anysphere",
  "dataQuality": "official-pricing-no-exact-quota",
  "pricingFreshness": "reviewed",
  "needsManualReview": false,
  "sourceConflict": false,
  "sourceRefs": [
    {
      "label": "Cursor pricing",
      "url": "https://cursor.com/pricing",
      "reviewedAt": "2026-05-06"
    }
  ],
  "plans": [
    {
      "id": "pro",
      "label": "Pro",
      "price": { "amount": 20, "currency": "USD", "period": "month", "precision": "official_exact" },
      "quotaWindows": [
        {
          "kind": "custom",
          "label": "Extended Agent limits",
          "unit": "agent_usage",
          "precision": "official_qualitative",
          "source": "official_preset",
          "visibleByDefault": true,
          "notes": ["Vendor does not publish an exact request count on the reviewed pricing page."]
        }
      ]
    },
    {
      "id": "pro_plus",
      "label": "Pro+",
      "price": { "amount": 60, "currency": "USD", "period": "month", "precision": "official_exact" },
      "quotaWindows": [
        {
          "kind": "custom",
          "label": "3x usage on OpenAI, Claude, Gemini models",
          "unit": "usage_multiplier",
          "limit": 3,
          "precision": "official_exact",
          "source": "official_preset",
          "visibleByDefault": true
        }
      ]
    },
    {
      "id": "ultra",
      "label": "Ultra",
      "price": { "amount": 200, "currency": "USD", "period": "month", "precision": "official_exact" },
      "quotaWindows": [
        {
          "kind": "custom",
          "label": "20x usage on OpenAI, Claude, Gemini models",
          "unit": "usage_multiplier",
          "limit": 20,
          "precision": "official_exact",
          "source": "official_preset",
          "visibleByDefault": true
        }
      ]
    }
  ]
}
```

```json
{
  "key": "github-copilot",
  "label": "GitHub Copilot",
  "vendor": "GitHub",
  "dataQuality": "official-date-aware",
  "pricingFreshness": "reviewed",
  "sourceRefs": [
    {
      "label": "GitHub Copilot models and pricing",
      "url": "https://docs.github.com/en/copilot/reference/copilot-billing/models-and-pricing",
      "reviewedAt": "2026-05-06"
    },
    {
      "label": "GitHub Copilot premium requests",
      "url": "https://docs.github.com/en/billing/concepts/product-billing/github-copilot-premium-requests",
      "reviewedAt": "2026-05-06"
    }
  ],
  "billingModes": [
    {
      "id": "premium_requests_legacy",
      "validUntil": "2026-05-31",
      "quotaWindows": [
        {
          "kind": "premium_requests",
          "label": "Monthly premium requests",
          "reset": { "type": "monthly_utc", "day": 1, "time": "00:00:00" },
          "precision": "official_exact",
          "source": "official_preset",
          "visibleByDefault": true
        }
      ]
    },
    {
      "id": "ai_credits_usage_based",
      "validFrom": "2026-06-01",
      "quotaWindows": [
        {
          "kind": "ai_credits",
          "label": "GitHub AI Credits",
          "unit": "credits",
          "creditUsdValue": 0.01,
          "precision": "official_exact",
          "source": "official_preset",
          "visibleByDefault": true
        }
      ]
    }
  ]
}
```

```json
{
  "key": "jetbrains-ai",
  "label": "JetBrains AI",
  "vendor": "JetBrains",
  "dataQuality": "official-credit-based",
  "pricingFreshness": "reviewed",
  "sourceRefs": [
    {
      "label": "JetBrains AI plans and usage",
      "url": "https://www.jetbrains.com/help/ai-assistant/licensing-and-subscriptions.html",
      "reviewedAt": "2026-05-06"
    }
  ],
  "plans": [
    {
      "id": "ai_free",
      "label": "AI Free",
      "price": { "amount": 0, "currency": "USD", "period": "30d", "precision": "official_exact" },
      "quotaWindows": [
        {
          "kind": "ai_credits",
          "label": "AI Credits per 30 days",
          "unit": "credits",
          "limit": 3,
          "reset": { "type": "rolling_30d_from_first_use" },
          "precision": "official_exact",
          "source": "official_preset",
          "visibleByDefault": true
        }
      ]
    }
  ]
}
```

### 5.3 Migrate provider catalog to v3

Create:

```text
package/contents/catalog/providers-v3.json
```

Required top-level fields:

```json
{
  "schemaVersion": 3,
  "catalogVersion": "2026.05.06",
  "release": "8.0.0",
  "lastReviewed": "2026-05-06",
  "runtimeScraping": false,
  "providers": []
}
```

Each model entry should support:

```json
{
  "id": "gpt-5.5",
  "displayName": "GPT-5.5",
  "modalities": ["text"],
  "pricing": {
    "currency": "USD",
    "unit": "1M_tokens",
    "input": 5.0,
    "cachedInput": 0.5,
    "output": 30.0,
    "batchDiscountPercent": 50,
    "precision": "official_exact"
  },
  "context": {
    "pricingContextLimitTokens": 270000,
    "maxContextTokens": null,
    "sourceQuality": "official"
  },
  "sourceRefs": [
    {
      "label": "OpenAI API pricing",
      "url": "https://openai.com/api/pricing/",
      "reviewedAt": "2026-05-06"
    }
  ]
}
```

OpenAI v3 entries must include these reviewed models unless a fresher source changes them:

```json
{
  "id": "gpt-5.5",
  "displayName": "GPT-5.5",
  "pricing": { "currency": "USD", "unit": "1M_tokens", "input": 5.0, "cachedInput": 0.5, "output": 30.0, "precision": "official_exact" }
}
```

```json
{
  "id": "gpt-5.4",
  "displayName": "GPT-5.4",
  "pricing": { "currency": "USD", "unit": "1M_tokens", "input": 2.5, "cachedInput": 0.25, "output": 15.0, "precision": "official_exact" }
}
```

```json
{
  "id": "gpt-5.4-mini",
  "displayName": "GPT-5.4 mini",
  "pricing": { "currency": "USD", "unit": "1M_tokens", "input": 0.75, "cachedInput": 0.075, "output": 4.5, "precision": "official_exact" }
}
```

Rules for non-token pricing:

- Do not store video/image/generation prices as fake `0.0` token prices.
- Use `pricing.unit` values such as `generation`, `image`, `video_second`, `credit`, `request`, or `unknown`.
- Use `pricing.status: "unknown"` when the price cannot be verified.
- Use `pricing.status: "not_applicable"` for local/free/self-hosted models where API vendor pricing does not apply.

---

## 6. C++ implementation plan

### 6.1 Add catalog loader classes

Add new files:

```text
plugin/catalogloader.h
plugin/catalogloader.cpp
plugin/subscriptionplancatalog.h
plugin/subscriptionplancatalog.cpp
plugin/providerpricingcatalog.h
plugin/providerpricingcatalog.cpp
```

Update `plugin/CMakeLists.txt` to include these files.

Requirements:

- Load JSON from the packaged catalog path.
- Validate schema version at runtime.
- Expose lookup functions by provider/tool/plan key.
- Cache loaded catalogs in memory.
- Fail safely if the catalog is missing or invalid.
- Emit warnings using `qWarning()` but do not crash the plasmoid.
- Expose stale/invalid state to QML so UI can show `Catalog stale`.

Suggested C++ API shape:

```cpp
class SubscriptionPlanCatalog : public QObject
{
    Q_OBJECT
public:
    explicit SubscriptionPlanCatalog(QObject *parent = nullptr);

    bool load();
    bool isValid() const;
    bool isStale(int maxAgeDays = 30) const;
    QString lastReviewed() const;
    QString catalogVersion() const;

    QStringList plansForTool(const QString &toolKey) const;
    QVariantMap plan(const QString &toolKey, const QString &planId) const;
    QVariantList quotaWindows(const QString &toolKey, const QString &planId) const;
    QVariantMap price(const QString &toolKey, const QString &planId) const;
};
```

### 6.2 Normalize quota data in `SubscriptionToolBackend`

Add a normalized QML-facing property:

```cpp
Q_PROPERTY(QVariantList quotaWindows READ quotaWindows NOTIFY quotaWindowsChanged)
```

Add signal:

```cpp
Q_SIGNAL void quotaWindowsChanged();
```

Each quota row should be a `QVariantMap` like:

```json
{
  "kind": "rolling_5h",
  "label": "5-hour session",
  "unit": "messages",
  "used": 61,
  "limit": 225,
  "rangeMin": null,
  "rangeMax": null,
  "remaining": 164,
  "percentUsed": 27.1,
  "resetAt": "2026-05-06T16:10:00Z",
  "timeUntilReset": "2h 14m",
  "source": "browser_sync",
  "precision": "browser_sync_actual",
  "badge": "Synced"
}
```

Keep existing primary/secondary/tertiary properties during v8 to avoid breaking QML, but build the new UI from `quotaWindows` where possible.

### 6.3 Fix dynamic QML properties

Current tertiary and credits values can change after sync. Do not keep these as `CONSTANT` if the value is dynamic.

Change any dynamic properties like these from `CONSTANT` to `NOTIFY usageUpdated` or a more specific signal:

```cpp
Q_PROPERTY(bool hasTertiaryLimit READ hasTertiaryLimit NOTIFY usageUpdated)
Q_PROPERTY(bool hasCredits READ hasCredits NOTIFY usageUpdated)
```

If `hasSubscriptionCost` can vary with catalog loading or plan selection, do not keep it `CONSTANT` either.

### 6.4 Replace hardcoded monitor plan methods

For each subscription monitor, replace hardcoded plan data with catalog lookups.

Example target behavior:

```cpp
QStringList CursorMonitor::availablePlans() const
{
    return SubscriptionPlanCatalog::instance()->plansForTool(QStringLiteral("cursor"));
}
```

Use stable internal tool keys:

```text
claude-code
codex-cli
github-copilot
cursor
windsurf
jetbrains-ai
```

Do not use display labels as catalog keys.

### 6.5 Browser sync should override presets but preserve source metadata

Priority order for quota rows:

1. Browser sync actual account data.
2. Provider API data.
3. Official preset from catalog.
4. User manual override.
5. Local self-tracked usage.
6. Estimated fallback.
7. Unknown / needs review.

If browser sync fails due to auth, HTTP, schema change, or empty cookies, keep official preset rows visible but clearly mark them as `Official preset` or `Needs sync`.

If browser sync response schema changes, show:

```text
Browser sync format changed — using official preset only
```

Do not display stale synced data as current unless it has a visible timestamp.

### 6.6 Copilot date-aware billing mode

Implement Copilot mode selection:

```text
Before 2026-06-01: premium_requests_legacy
On/after 2026-06-01: ai_credits_usage_based
```

For annual users staying on legacy request-based billing, support a manual override:

```text
billingMode = auto | premium_requests_legacy | ai_credits_usage_based
```

Default should be `auto`.

### 6.7 Reset calculations

Support these reset strategies:

```text
rolling_duration: e.g. 5 hours from period start
rolling_7d_from_session_start
rolling_30d_from_first_use
monthly_utc: day 1 at 00:00:00 UTC
monthly_billing_cycle
vendor_provided_reset_at
manual_unknown
```

Do not infer weekly reset dates for vendors that only expose them via sync unless the source explicitly states the reset rule.

---

## 7. QML/UI implementation plan

### 7.1 Add reusable components

Create:

```text
package/contents/ui/QuotaStack.qml
package/contents/ui/QuotaRow.qml
package/contents/ui/SourceBadge.qml
package/contents/ui/CatalogTrustPanel.qml
```

### 7.2 Update `SubscriptionToolCard.qml`

Replace the single primary usage section with `QuotaStack`.

The card should show:

- tool label and icon
- status: installed / not installed / active / sync failed
- selected plan
- subscription price if available
- last sync time if available
- all quota windows from `monitor.quotaWindows`
- compact collapsed summary showing the most constrained quota first
- sync status and manual sync button

Example display:

```text
Claude Code
Max 5x · $100/mo · Synced 4m ago

5-hour session
61 / ~225 messages · resets in 2h 14m · Synced

Weekly all-model
382 / synced cap · resets Monday · Synced

Extra usage
$4.20 / $25.00 · resets Jun 1 · Synced
```

For ranges:

```text
Claude Code prompts
10–40 prompts per 5h · Official range
```

For unknowns:

```text
Windsurf quota
Daily/weekly budget model detected in docs · Needs review
```

### 7.3 Source badges

`SourceBadge.qml` should map precision/source to user-visible labels:

```text
browser_sync_actual -> Synced
official_exact -> Official
official_range -> Official range
official_approx -> Official approx
official_qualitative -> Official note
self_tracked_local -> Self-tracked
estimated -> Estimated
needs_manual_review -> Needs review
unknown -> Unknown
```

Do not color-code only. Include readable text for accessibility.

### 7.4 Trust panel

Add `CatalogTrustPanel.qml` or a section in existing settings/config UI showing:

```text
Provider catalog: providers-v3.json · reviewed 2026-05-06
Subscription catalog: subscriptions-v1.json · reviewed 2026-05-06
Runtime scraping: disabled
Stale providers: 0
Stale subscriptions: 0
Needs manual review: Windsurf quota model
```

---

## 8. Validation scripts

### 8.1 Update provider catalog check

Modify or replace:

```text
scripts/check_provider_catalog.py
```

New requirements:

- schemaVersion must be `3`
- `runtimeScraping` must be `false`
- top-level `lastReviewed` must be ISO date
- model-level `sourceRefs` must exist
- model-level `pricing.precision` must exist
- token prices cannot be negative
- token models must include `input` and `output`
- cached input is optional but must be non-negative when present
- non-token pricing must not fake `0.0` token pricing
- no provider/model pricing older than 45 days unless marked `not_applicable` or `unknown`

### 8.2 Add subscription catalog check

Create:

```text
scripts/check_subscription_catalog.py
```

Requirements:

- schemaVersion must be `1`
- `runtimeScraping` must be `false`
- top-level `lastReviewed` must be ISO date and not older than 30 days
- every tool must have `key`, `label`, `vendor`, `plans` or `billingModes`, and `sourceRefs`
- every plan with a price must include `amount`, `currency`, `period`, and `precision`
- every quota window must include `kind`, `label`, `precision`, `source`, and `visibleByDefault`
- exact numeric limits must have `precision` of `official_exact`, `browser_sync_actual`, or `provider_api`
- estimated/range/qualitative values must not use exact labels
- any `sourceConflict: true` or `needsManualReview: true` must be counted and printed
- fail the release unless a command-line flag allows manual review items for development only

Suggested CLI behavior:

```bash
python scripts/check_subscription_catalog.py
python scripts/check_subscription_catalog.py --allow-manual-review
```

### 8.3 Add hardcoded pricing/limit scanner

Create:

```text
scripts/check_no_hardcoded_pricing.py
```

This script should scan monitor C++ files for suspicious hardcoded values after migration.

Flag patterns like:

```text
return 20.0
return 200.0
return 500
return 1500
$20
/mo
premium requests
credits/mo
```

Allowlist:

- test fixtures
- JSON catalogs
- comments that explicitly say they are examples
- UI formatting constants

Required output:

```text
Hardcoded pricing check OK
```

or detailed failures with file/line.

---

## 9. Tests to add

Use existing CTest/Qt Test setup if available. Add tests under `plugin/tests` or the existing test location.

Required tests:

1. Provider catalog v3 loads successfully.
2. Subscription catalog v1 loads successfully.
3. Stale provider catalog fails validation.
4. Stale subscription catalog fails validation.
5. Missing sourceRefs fails validation.
6. Non-token pricing cannot be represented as fake 0/0 token pricing.
7. `SubscriptionToolBackend::quotaWindows` returns normalized rows.
8. Dynamic `hasCredits` and `hasTertiaryLimit` update after sync data changes.
9. 5-hour reset calculation works.
10. Weekly reset calculation works.
11. Monthly UTC reset calculation works for Copilot legacy mode.
12. Rolling 30-day reset works for JetBrains AI.
13. Copilot auto mode selects legacy before `2026-06-01` and AI Credits on/after `2026-06-01`.
14. Browser sync schema failure falls back to official preset and sets a diagnostic.
15. QML quota row does not crash when limit is missing, range-only, or qualitative-only.

Add JSON fixtures:

```text
tests/fixtures/catalog/providers-v3.valid.json
tests/fixtures/catalog/providers-v3.stale.json
tests/fixtures/catalog/subscriptions-v1.valid.json
tests/fixtures/catalog/subscriptions-v1.stale.json
tests/fixtures/browser_sync/claude/usage_valid.json
tests/fixtures/browser_sync/claude/usage_schema_changed.json
tests/fixtures/browser_sync/codex/accounts_check_valid.json
tests/fixtures/browser_sync/codex/accounts_check_schema_changed.json
```

Do not include real cookies, tokens, account IDs, or personal data in fixtures.

---

## 10. Version bump

Bump all release metadata to `8.0.0`.

Likely files to inspect/update:

```text
CMakeLists.txt
com.github.loofi.aiusagemonitor.metainfo.xml
package/metadata.json
package/contents/catalog/providers-v3.json
package/contents/catalog/subscriptions-v1.json
CHANGELOG.md
README.md
```

Do not assume every file exists. Search first.

---

## 11. Changelog entry

Add a v8.0.0 changelog entry similar to:

```markdown
## 8.0.0 - Source of Truth

### Added
- Added versioned subscription catalog with source-reviewed plan prices, quota windows, precision metadata, and no runtime scraping.
- Added provider pricing catalog v3 with cached input pricing, source refs, review dates, non-token pricing support, and model-level precision.
- Added QuotaStack UI for 5-hour, weekly, monthly, credits, extra usage, code review, and reset display.
- Added data source badges: Synced, Official, Official range, Self-tracked, Estimated, Needs review.
- Added Trust/Data Accuracy panel showing catalog review status and stale/manual-review warnings.

### Changed
- Moved subscription plan prices and limits out of monitor C++ into catalog data.
- Updated OpenAI model pricing for GPT-5.5, GPT-5.4, and GPT-5.4 mini.
- Updated Codex, Claude Code, Copilot, Cursor, Windsurf, and JetBrains subscription metadata according to reviewed source status.
- GitHub Copilot now supports date-aware transition from premium requests to AI Credits.

### Fixed
- Fixed dynamic QML updates for credits and tertiary/code-review limits.
- Fixed misleading exact quota display for vendors that only publish ranges or qualitative limits.
- Fixed fallback behavior when browser sync schemas change.

### Validation
- Added subscription catalog validation.
- Added provider catalog v3 validation.
- Added scanner for hardcoded pricing and quota values.
```

---

## 12. Acceptance criteria

The implementation is complete only when all of these are true:

- `project(... VERSION 8.0.0)` is set.
- `providers-v3.json` exists and replaces/migrates v2 usage.
- `subscriptions-v1.json` exists and is used by subscription monitors.
- `ProviderCatalog.qml` no longer duplicates stale provider facts without reading or being generated from the v3 source of truth.
- Subscription monitors no longer hardcode plan prices or quota limits.
- Subscription UI shows all relevant quota windows, not only primary usage.
- Every visible quota/pricing row has a source/precision badge.
- OpenAI model pricing is updated for GPT-5.5, GPT-5.4, and GPT-5.4 mini.
- Codex metadata reflects temporary 2x rate limit status without pretending exact limits are static.
- Claude Code metadata reflects shared Claude/Claude Code usage and 5-hour/weekly limit structure.
- GitHub Copilot supports both legacy premium requests and AI Credits with the `2026-06-01` transition.
- Cursor includes Hobby, Pro, Pro+, Ultra, Teams, Enterprise plan structure.
- Windsurf is either fully resolved against current official docs or marked as `needsManualReview/sourceConflict` visibly.
- JetBrains AI uses AI Credits and 30-day reset logic, not old request-count limits.
- Browser sync failure does not show stale synced data as exact current data.
- New validation scripts pass.
- Existing tests still pass.

Run at minimum:

```bash
python scripts/check_provider_catalog.py
python scripts/check_subscription_catalog.py
python scripts/check_no_hardcoded_pricing.py
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

If the repo has project-specific commands such as `just check`, `just test`, or Fedora-specific checks, run those too:

```bash
just check
just test
just fedora44-check
```

If any command does not exist, document that in the final summary instead of inventing a passing result.

---

## 13. Final Codex response format

When done, report:

```markdown
## Summary
- ...

## Files changed
- ...

## Data sources reviewed
- ...

## Validation
- [pass/fail] python scripts/check_provider_catalog.py
- [pass/fail] python scripts/check_subscription_catalog.py
- [pass/fail] python scripts/check_no_hardcoded_pricing.py
- [pass/fail] cmake build
- [pass/fail] ctest

## Remaining risks
- ...
```

Do not claim that data is up to date unless the source review was actually performed during the implementation.
