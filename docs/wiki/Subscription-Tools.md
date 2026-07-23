<!-- Generated from docs/user-guide/subscriptions.md by scripts/generate_wiki_docs.py; do not edit. -->
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

Local monitors watch tool-specific state for activity. They do not read a public vendor quota API. Plan presets supply the window and limit, and the card counts activity against that local model.

Choose the correct plan for locally tracked tools. Antigravity is the exception: its plan is detected automatically and has no custom-limit control.

## Codex CLI

The widget prefers the existing local Codex login and can read its live five-hour and weekly quota windows when the local response format is supported. If that source is unavailable, the card falls back to local plan tracking.

## Claude Code

The standard card tracks local activity against the selected session and weekly plan limits. Browser Sync Labs can add session usage from a supported signed-in browser profile.

## GitHub Copilot

The card tracks local activity and the selected billing mode. An optional GitHub token and organization name can expose organization-level seat metrics when the token has the required permission.

## Browser Sync Labs

Browser Sync Labs is off by default. It reads a selected local browser profile and calls the relevant service directly with the existing session.

Before enabling it:

- sign in to the service in the selected browser
- close and reopen the browser once if its cookie database has not been created
- confirm KWallet or libsecret is available for Chromium-family cookie decryption
- select a specific profile when auto-detection chooses the wrong one

The feature depends on undocumented endpoints and can stop working after a service change. A failed sync does not invalidate local tracking.

The widget does not export browser cookies or save them in its history database. Read [Privacy and security](Privacy-and-Security) for the exact boundary.
