## GitHub and KDE Store Submission Checklist

Use this checklist after the stabilization work and screenshot refresh are complete.

## Preconditions

- roadmap and changelog reflect the release you are shipping
- `scripts/package_plasmoid.sh --check` passes
- AppStream metadata validates cleanly
- canonical screenshots have been reviewed in a real Fedora KDE session
- the install story is described honestly: the plasmoid archive is valid, but the compiled plugin is still required separately

## GitHub release pack

Prepare the same assets for the manual GitHub release pack:

- source tarball
- `com.github.loofi.aiusagemonitor.plasmoid`
- updated README screenshots
- changelog section for the release
- release notes that call out the source-backed catalogs, quota-window UI, and public-provider packaging model
- confirm the COPR package still points at GitHub SCM on `main`; keep auto-rebuild disabled so prerelease tags cannot publish stable RPMs

## KDE Store listing notes

Use wording that highlights the value of the widget without overpromising the current packaging model.

### Suggested short description

Native KDE Plasma widget for monitoring AI provider usage, costs, limits, and subscription-tool quotas from the panel.

### Suggested longer positioning points

- multi-provider AI cost and rate-limit visibility without a proxy or SaaS gateway
- local history, export, compare analytics, budgets, and notifications
- subscription-tool tracking for Claude Code, Codex CLI, GitHub Copilot, Cursor, Windsurf, and JetBrains AI
- privacy-conscious local workflow with secure key storage in KWallet

## Screenshot inventory

Keep the existing filenames stable so README and AppStream links continue to work:

- `assets/screenshots/main-window.png`
- `assets/screenshots/panel-view.png`
- `assets/screenshots/settings-view.png`

If the store allows additional images, also prepare:

- history/compare analytics view
- subscriptions and budget/limits view

## Manual publication sequence

1. verify the target version in `ROADMAP.md`, `package/metadata.json`, `com.github.loofi.aiusagemonitor.metainfo.xml`, `CMakeLists.txt`, and `plasma-ai-usage-monitor.spec`
2. push the release commit and tag
3. create the GitHub release manually and attach the tarball and `.plasmoid` artifacts
4. submit the exact stable tag explicitly with `just copr-submit v13.0.0 loofitheboss/plasma-ai-usage-monitor`
5. confirm the COPR build succeeded before announcing the release
6. update the README-linked screenshots if filenames stayed stable but content changed
7. upload the refreshed screenshot set and listing copy to KDE Store
8. confirm the listing language mentions the compiled plugin requirement clearly

## COPR verification

Use this to confirm the Fedora update path is still healthy:

```bash
curl -s 'https://copr.fedorainfracloud.org/api_3/package/?ownername=loofitheboss&projectname=plasma-ai-usage-monitor&packagename=plasma-ai-usage-monitor&with_latest_build=true'
```

Expected fields:

- `"source_type": "scm"`
- `"clone_url": "https://github.com/loofiboss-bit/plasma-ai-usage-monitor.git"`
- `"committish": "main"`
- `"source_build_method": "make_srpm"`
- `"auto_rebuild": false`

## Final review prompts

- does the first screenshot immediately explain what the widget does?
- do the images show the widget in action instead of empty or ambiguous states?
- does the listing avoid implying a fully self-contained store install if that is still untrue?
- do the GitHub release notes and store listing tell the same story?
