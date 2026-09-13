# Subscription tools and Browser Sync Labs

Subscription cards combine local activity with plan limits. Treat those figures as local estimates unless the card identifies a supported authenticated source.

## Supported tools

- Claude Code
- Google Antigravity
- Codex CLI
- GitHub Copilot
- Cursor
- Windsurf
- JetBrains AI

Open **Settings → Subscriptions** and enable the tools you use. The detection status checks installed binaries and known local state directories.

A tool-only setup is supported. Synchronized quota windows can drive the
Overview lowest-quota and next-reset facts; local activity and configured plan
limits remain estimates and are labeled separately.

## Google Antigravity

Antigravity monitoring is opt-in and supports the current Antigravity 2.x Linux desktop app. Antigravity must be installed, running, and signed in. The widget detects the plan automatically, shows the shared five-hour and weekly quota buckets returned by Antigravity 2.x, and keeps the per-model rows returned for the account. Models without quota progress remain visible as available, and temporarily disabled models keep their reported reason.

The monitor calls only the local, read-only `GetUserStatus` and `RetrieveUserQuotaSummary` RPCs over pinned TLS. It accepts the Antigravity 2.x `resources/bin/language_server` layout and the older extension layout, but only when the process belongs to the current user, listens on loopback, and presents the certificate shipped with that executable. It does not start Antigravity, sign in, read OAuth tokens, prompts, conversations, logs, or other cache data. The daemon CSRF value is kept in memory and sent only to the validated loopback process. It is never logged, persisted, diagnosed, or exported.

Model access and quotas are dynamic, so the catalog does not contain a fixed model matrix or numeric limits. If a temporary error occurs after a successful refresh, the last snapshot remains visible as **Stale**. Use **Refresh / Test connection** after restarting or updating Antigravity.

`gemini.google.com` usage is not included in the Antigravity card. The Gemini web app uses a separate product quota and does not expose a stable, documented read-only quota interface that the widget can query safely.

## Local tracking

Local monitors watch tool-specific state for activity. They do not read a public vendor quota API. A custom activity target counts local requests. It is not a vendor entitlement, credit balance, or allowance. Without an explicit target, local activity has no quota percentage. Public presets describe prices and qualitative allowances separately.

Choose the correct plan for locally tracked tools. Antigravity is the exception: its plan is detected automatically and has no custom-limit control.

## Codex CLI

The widget automatically uses the existing local Codex login to refresh live
five-hour and weekly quota windows at startup, on its refresh schedule, and
after Codex monitoring is enabled. This local-auth refresh does not inspect
browser profiles, even when Browser Sync is unavailable or its circuit breaker
has stopped browser requests. If local auth is unavailable, the card falls back
to local plan tracking.

When you explicitly enable and run Browser Sync Labs, Codex still tries local
auth first and may use the selected browser session as a compatibility fallback.

## Claude Code

The standard card tracks local activity against the selected session and weekly plan limits. Browser Sync Labs can add session usage from a supported signed-in browser profile.

## GitHub Copilot

The card tracks local activity and the selected billing mode. An optional GitHub token and organization name can expose organization-level seat metrics when the token has the required permission.

## Browser Sync Labs

Browser Sync Labs is off by default. Only enabling it permits the widget to read
a selected local browser profile and call the relevant service directly with
the existing session. Automatic Codex local-auth refresh does not require this
setting and does not read browser storage.

Before enabling it:

- sign in to the service in the selected browser
- close and reopen the browser once if its cookie database has not been created
- confirm KWallet or libsecret is available for Chromium-family cookie decryption
- select a specific profile when auto-detection chooses the wrong one

The feature depends on undocumented endpoints and can stop working after a service change. A failed sync does not invalidate local tracking.

The widget does not export browser cookies or save them in its history database. Read [Privacy and security](privacy-and-security.md) for the exact boundary.

## Public evidence and prices

Each price and allowance carries its own source references, review date,
effective date, and expiry date. Public evidence is valid through the expiry date
(inclusive), at most 30 days after the oldest supporting source review. A recent
catalog header cannot renew individual rows. Missing, malformed, future, expired,
or manually disputed evidence is unavailable; Settings → Diagnostics lists the
affected entries and official references. Catalog expiry does not invalidate a
separately authenticated live account response.

JetBrains personal and commercial prices remain ranges when the account type is
unknown. A range is not reduced to its minimum fee, and unknown organization totals
are unavailable. A per-seat price alone does not establish a team bill. Windsurf
and Devin account transitions remain under review until a supported account
contract is verified. Historical Copilot premium requests are archived and never
serve as the denominator for current AI credits.
