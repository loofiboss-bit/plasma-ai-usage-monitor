# Subscription tools and Browser Sync Labs

Subscription cards combine local activity with plan limits. Treat those figures as local estimates unless the card identifies a supported authenticated source.

## Supported tools

- Claude Code
- Codex CLI
- GitHub Copilot
- Cursor
- Windsurf
- JetBrains AI

Open **Settings → Subscriptions** and enable the tools you use. The detection status checks installed binaries and known local state directories.

## Local tracking

Local monitors watch tool-specific state for activity. They do not read a public vendor quota API. Plan presets supply the window and limit, and the card counts activity against that local model.

Choose the correct plan. Use a custom limit only when your account differs from the shipped preset.

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

The widget does not export browser cookies or save them in its history database. Read [Privacy and security](privacy-and-security.md) for the exact boundary.
