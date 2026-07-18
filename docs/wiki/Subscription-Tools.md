# Subscription tools

The widget tracks Google Antigravity, Claude Code, Codex CLI, GitHub Copilot, Cursor, Windsurf, and JetBrains AI.

Open **Settings → Subscriptions**, enable the installed tools, and select the correct plan. Local monitors count activity from known state directories. Plan presets are estimates unless a supported authenticated source supplies live quota windows.

Google Antigravity is opt-in and Linux-only. Keep the Antigravity desktop app running and signed in. The widget detects the plan and reads the account's live per-model quota from the validated localhost daemon over pinned TLS. It does not read prompts, conversations, OAuth tokens, or logs, and the in-memory daemon CSRF value never enters diagnostics, history, or exports. Plans, model access, and limits are taken from the live response rather than a static model matrix.

Codex CLI can use its existing local login for five-hour and weekly windows. Browser Sync Labs can read supported signed-in browser profiles for selected services.

Browser Sync Labs is optional and disabled by default. It depends on undocumented endpoints and can stop working after a service update. Cookies do not enter QML, logs, diagnostics, history, or exports.

Read the [subscription tool guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/subscriptions.md) for profile selection and failure guidance.
