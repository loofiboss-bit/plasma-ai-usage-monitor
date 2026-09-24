# Plasma AI Usage Monitor v21 Plan

- **Target release:** `21.0.0`
- **Status:** local candidate; not published
- **Theme:** Source Health and Data Quality

## Goal

Make the source detail answer three questions without implying that missing or
old data is current: what this source can report, when it was last checked, and
what should happen next.

## Scope

1. Add `waiting_for_activity` for detected local tools without an activity
   observation. A successful local check in this state completes onboarding but
   does not count as an estimate or useful data. Preserve a reported numeric
   zero as available data.
2. Apply observation-time validity to normalized metrics before daily quality
   classes and aggregates use them. Expired, future-dated, reset, or undated
   metrics stay unavailable and may be shown only as timestamped last-known
   values.
3. Add capability-aware source health to Source Detail: data quality, expected
   capability, last attempt, last successful check, Retry-After, next scheduled
   refresh, and the relevant action. Keep technical metric detail collapsible
   and accessible. Respect active Retry-After windows in the refresh action.
4. Remove `ai_usage_total_monthly_exposure`. Preserve separate actual API spend,
   estimated burn, and fixed subscription-fee metrics by known ISO currency.
   Do not export unknown currency as USD or missing values as observed zero.

## Compatibility note

The v21 Prometheus interface removes `ai_usage_total_monthly_exposure` because
it combined billing, estimates, and fixed fees. Consumers should use
`ai_usage_api_spend`, `ai_usage_estimated_burn`, and
`ai_usage_subscription_fees`; tool subscription fees remain available as
`ai_usage_tool_subscription_fee` when the plan price and currency are known.

## Verification

- Add C++ and QML regressions for no local activity, valid zero, stale metrics,
  retry timing, onboarding, source health actions, unknown currency, missing
  metrics, and separated Prometheus cost classes.
- Run debug build, the full debug CTest suite, `just check`, QML lint, and
  relevant performance and accessibility gates.
- Review Source Detail at narrow and wide sizes in light and dark themes with
  keyboard navigation and screen-reader labels.
- Update the English user guide, architecture, changelog, and roadmap. Generate
  the wiki mirror from the user guide.
- Qualify the package candidate with Fedora lifecycle checks before planning
  any publication. No publication, tag, or remote write is part of this plan.

## Constraints

- Use existing local data and verified read-only sources.
- Add no provider, network endpoint, or history database.
- Preserve the existing main baseline and unrelated work on older branches.
