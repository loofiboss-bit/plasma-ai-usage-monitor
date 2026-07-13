#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/scripts"
cp "$ROOT_DIR/scripts/verify_exact_tag.sh" "$TMP_DIR/scripts/verify_exact_tag.sh"
cp "$ROOT_DIR/VERSION" "$TMP_DIR/VERSION"
cd "$TMP_DIR"

git init -q
git config user.name "Promotion Gate Test"
git config user.email "promotion-gate@example.invalid"
git add VERSION scripts/verify_exact_tag.sh
git commit -qm "base"
base_commit="$(git rev-parse HEAD)"

git commit --allow-empty -qm "release candidate"
release_commit="$(git rev-parse HEAD)"

git tag -a v13.0.0-alpha.99 -m "alpha fixture"
bash scripts/verify_exact_tag.sh v13.0.0-alpha.99 >/dev/null

git tag -a v13.0.0 -m "stable without rc"
if bash scripts/verify_exact_tag.sh v13.0.0 >/dev/null 2>&1; then
  echo "Stable promotion unexpectedly passed without rc.1" >&2
  exit 1
fi
git tag -d v13.0.0 >/dev/null

recent_date="$(date -u -d '6 days ago' '+%Y-%m-%dT%H:%M:%SZ')"
GIT_COMMITTER_DATE="$recent_date" git tag -a v13.0.0-rc.1 -m "recent rc fixture"
git tag -a v13.0.0 -m "stable before soak"
git update-ref refs/remotes/origin/main "$release_commit"
if bash scripts/verify_exact_tag.sh v13.0.0 >/dev/null 2>&1; then
  echo "Stable promotion unexpectedly passed before seven-day soak" >&2
  exit 1
fi
git tag -d v13.0.0 v13.0.0-rc.1 >/dev/null

soaked_date="$(date -u -d '8 days ago' '+%Y-%m-%dT%H:%M:%SZ')"
GIT_COMMITTER_DATE="$soaked_date" git tag -a v13.0.0-rc.1 -m "soaked rc fixture"
git tag -a v13.0.0 -m "stable fixture"
git update-ref refs/remotes/origin/main "$base_commit"
if bash scripts/verify_exact_tag.sh v13.0.0 >/dev/null 2>&1; then
  echo "Stable promotion unexpectedly passed with divergent origin/main" >&2
  exit 1
fi

git update-ref refs/remotes/origin/main "$release_commit"
bash scripts/verify_exact_tag.sh v13.0.0 >/dev/null

echo "Exact-tag promotion gate OK: prerelease, lineage, main, and seven-day soak"
