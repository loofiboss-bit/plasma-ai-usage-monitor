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
beta_tag="${stable_tag}-beta.99"

git init -q
git config user.name "Promotion Gate Test"
git config user.email "promotion-gate@example.invalid"
git add VERSION scripts/verify_exact_tag.sh
git commit -qm "base"
base_commit="$(git rev-parse HEAD)"

git commit --allow-empty -qm "release candidate"
release_commit="$(git rev-parse HEAD)"

git tag -a "$beta_tag" -m "beta fixture"
if bash scripts/verify_exact_tag.sh "$beta_tag" >/dev/null 2>&1; then
  echo "Direct stable gate unexpectedly accepted a beta tag" >&2
  exit 1
fi

git tag "$stable_tag"
git update-ref refs/remotes/origin/main "$release_commit"
if bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null 2>&1; then
  echo "Direct stable gate unexpectedly accepted a lightweight tag" >&2
  exit 1
fi
git tag -d "$stable_tag" >/dev/null

git tag -a "$stable_tag" -m "stable fixture"
git update-ref refs/remotes/origin/main "$base_commit"
if bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null 2>&1; then
  echo "Direct stable gate unexpectedly passed with divergent origin/main" >&2
  exit 1
fi

git update-ref refs/remotes/origin/main "$release_commit"
bash scripts/verify_exact_tag.sh "$stable_tag" >/dev/null

echo "Direct stable gate OK: prereleases rejected; annotated stable tag equals main"
