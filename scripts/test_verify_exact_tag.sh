#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/scripts"
cp "$ROOT_DIR/scripts/verify_exact_tag.sh" "$TMP_DIR/scripts/verify_exact_tag.sh"
cp "$ROOT_DIR/VERSION" "$TMP_DIR/VERSION"
cd "$TMP_DIR"

version="$(tr -d '[:space:]' < VERSION)"
stable_tag="v${version}"
rc_tag="${stable_tag}-rc.1"
alpha_tag="${stable_tag}-alpha.99"

git init -q
git config user.name "Promotion Gate Test"
git config user.email "promotion-gate@example.invalid"
git add VERSION scripts/verify_exact_tag.sh
git commit -qm "base"
base_commit="$(git rev-parse HEAD)"

git commit --allow-empty -qm "release candidate"
release_commit="$(git rev-parse HEAD)"

git tag -a "$alpha_tag" -m "alpha fixture"
bash scripts/verify_exact_tag.sh "$alpha_tag" >/dev/null

git tag -a "$stable_tag" -m "stable without rc"
if bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null 2>&1; then
  echo "Stable promotion unexpectedly passed without rc.1" >&2
  exit 1
fi
git tag -d "$stable_tag" >/dev/null

recent_date="$(date -u -d '6 days ago' '+%Y-%m-%dT%H:%M:%SZ')"
GIT_COMMITTER_DATE="$recent_date" git tag -a "$rc_tag" -m "recent rc fixture"
git tag -a "$stable_tag" -m "stable before soak"
git update-ref refs/remotes/origin/main "$release_commit"
if bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null 2>&1; then
  echo "Stable promotion unexpectedly passed before seven-day soak" >&2
  exit 1
fi
git tag -d "$stable_tag" "$rc_tag" >/dev/null

soaked_date="$(date -u -d '8 days ago' '+%Y-%m-%dT%H:%M:%SZ')"
GIT_COMMITTER_DATE="$soaked_date" git tag -a "$rc_tag" -m "soaked rc fixture"
git tag -a "$stable_tag" -m "stable fixture"
git update-ref refs/remotes/origin/main "$base_commit"
if bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null 2>&1; then
  echo "Stable promotion unexpectedly passed with divergent origin/main" >&2
  exit 1
fi

git update-ref refs/remotes/origin/main "$release_commit"
bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null

echo "Exact-tag promotion gate OK: prerelease, lineage, main, and seven-day soak"
