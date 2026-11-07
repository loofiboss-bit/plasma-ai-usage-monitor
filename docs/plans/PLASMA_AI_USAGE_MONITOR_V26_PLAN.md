# V26 — AI Control Center & Smart Usage Experience

- **Target release:** `26.0.0`
- **Base:** `bbb616f24245dc0ab5613f259a5a29dd96156b29` (main)
- **Status:** Implementation and local qualification; no publication authorized.

## Delivery order

1. Record v25.0.1 performance and coverage contracts and consolidate PRs #93–#105 (`UsageDatabase::getHistoryCatalog` query reuse, integer/ASCII hot-path optimizations in `CostEngine`, `CurrencyMinorUnits`, `BudgetPolicyRepository`, `LocalSessionParser`, `Utils.js`, and `LocalActivityMonitorBase` lifecycle tests).
2. Share favorites, personalized Daily Focus, Overview Control Strip (quick filter pills for *All*, *Favorites*, *Needs attention*), multi-window quota strips with reset countdown + local clock time for Codex, Claude Code, Antigravity and API providers, keyboard navigation (`Alt+1..3`, `Escape`) and mouse-wheel favorite cycling on the KDE panel.
3. Add snapshot-consistent completed-day session comparisons across *All tools* (`toolBreakdown`), projects and models with token share (`sharePercent`), cache efficiency (`cacheHitPercent`), category coverage and drill-down.
4. Enhance notifications with *Quota Reset Ready* (`quotaReset`), clickable *Open Source Details* actions, relative + clock reset times, and optional *Favorites only for warnings* (while never suppressing critical alerts).
5. Verify upgrade, isolated UI, packaging, docs and final source-bound media.

## Contracts

Favorites never suppress global alerts. Numeric zero differs from unavailable.
Actual spend, estimates, balances, fixed fees, subscription quotas and local
session tokens remain distinct. Local comparisons default to seven completed
calendar days versus the preceding seven in the local time zone. Cache tokens
are subsets of input. Missing observations are not zero usage.

Reuse SQLite v9, config schema v3 and session export v1. Preserve opt-ins,
HMAC identities and local-only aliases. No new telemetry or network calls.

## Acceptance

Focused and complete CTest, warning-free QML lint, sanitizers, Fedora 44/45
package/installation checks, isolated virtual Plasma UI and source-bound media.
Compare Release builds on identical fixtures; retain existing absolute budgets.
Record limitations and measured outcomes in the qualification/performance reports.
No commit, push, host installation or remote publication belongs to this task.
