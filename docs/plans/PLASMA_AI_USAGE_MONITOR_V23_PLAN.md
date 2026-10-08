# V23 — Clearer and More Reliable Daily Use

- **Target release:** `23.0.0`

Status: implementation is prepared for review. Public release, COPR, KDE Store,
and wiki publication remain separate tasks. The candidate keeps the existing
three main views, local storage, KWallet boundaries, providers, and dependencies.

## Accurate daily metrics

- Aggregate spend only across matching currencies, exact periods, semantics,
  and actual/estimated quality. Keep unbounded current, unknown-window, and
  all-time values source-specific.
- Carry the primary metric's currency, unit, period, and quality through source
  cards, overview, and compact spend views. Preserve available zero and never
  format tokens, requests, or credits as money.

## Privacy and export contracts

- Cancel tracked subscription-tool network replies and invalidate callbacks
  when monitoring is disabled. Stop endpoint fallbacks, history writes, and
  notifications after cancellation; gate native and startup entry points.
- Serialize popup selected-series exports natively with the documented
  schema-v6 allowlist. Redact raw scopes and internal fields while preserving
  periods, units, quality, gaps, nulls, and zero values.

## Catalog lifetime and refresh scheduling

- Recheck hard catalog expiry during long sessions and after wake. Disable new
  estimates after expiry, refresh Trust Center status, and load verified catalog
  updates without restarting the widget. Preserve historical provenance and
  actual billing data.
- Use one single-shot deadline per source based on completed success or failed
  attempt. Honor retry delay, backoff, and jitter; allow attempts when network
  reachability is unknown and pause external automatic calls only when offline.

## Source selection and runtime status

- Add searchable onboarding and overview source lists without changing
  recommendation order, whole-monitor totals, or aggregate warnings. Preserve
  keyboard focus and keep setup navigation outside the scrolling list.
- Show the existing Prometheus listener's address and typed failure without
  creating another server in Settings. Show only redacted recent webhook
  delivery results by channel and retain the existing budget-policy receipts.

## Qualification and rollout boundary

Run the Debug build and CTest, `just check`, QML lint, sanitizers, performance
budgets, and Fedora packaging checks. Qualify narrow popup layout, scaling,
keyboard navigation, and screen-reader behavior; record physical checks that
cannot be performed as unverified. Keep installation and public publication
separate from this implementation plan.
