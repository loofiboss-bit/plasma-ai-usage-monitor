## v14.1 GitHub and KDE Store submission checklist

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
- release notes that explain Google Antigravity monitoring, dynamic model quotas, and the local-daemon security boundary
- confirm the COPR package still points at GitHub SCM on `main`; keep auto-rebuild disabled so prerelease tags cannot publish stable RPMs

## KDE Store listing notes

Use wording that highlights the value of the widget without overpromising the current packaging model.

### Short description

Verify provider-reported AI usage and spend, local coding-tool activity, and read-only connection checks from the Plasma panel.

### Suggested longer positioning points

- Guided first success configures and verifies one useful source
- reporting data, balances, local estimates, and connectivity stay distinct
- local history, Analyst, exports, budgets, and notifications
- local-first storage with secrets in KWallet and no hosted backend
- the Store archive is the frontend; install the matching native plugin first from COPR or a source build

## Screenshot inventory

Upload the eight reviewed Breeze Dark captures from the same isolated demo-user session:

- `assets/screenshots/guided-first-success.png`
- `assets/screenshots/verified-success.png`
- `assets/screenshots/main-window.png`
- `assets/screenshots/provider-intelligence.png`
- `assets/screenshots/settings-view.png`
- `assets/screenshots/history-view.png`
- `assets/screenshots/analyst-view.png`
- `assets/screenshots/panel-view.png`

## Manual publication sequence

1. verify the target version in `ROADMAP.md`, `package/metadata.json`, `com.github.loofi.aiusagemonitor.metainfo.xml`, `CMakeLists.txt`, and `plasma-ai-usage-monitor.spec`
2. push the release commit and tag
3. publish the draft GitHub release created by the exact-tag workflow and verify its assets
4. submit the exact stable tag explicitly with `just copr-submit v14.1.0 loofitheboss/plasma-ai-usage-monitor`
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
