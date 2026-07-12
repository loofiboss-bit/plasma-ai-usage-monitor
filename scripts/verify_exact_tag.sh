#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

tag="${1:-}"
version="$(tr -d '[:space:]' < VERSION)"
expected="v${version}"
[[ "$tag" == "$expected" ]] || { echo "Expected exact tag ${expected}, got ${tag:-<none>}" >&2; exit 1; }
tag_commit="$(git rev-parse "${tag}^{commit}")"
head_commit="$(git rev-parse HEAD)"
[[ "$tag_commit" == "$head_commit" ]] || { echo "Tag ${tag} does not resolve to HEAD" >&2; exit 1; }
[[ -z "$(git status --porcelain --untracked-files=no)" ]] || { echo "Tracked worktree changes prevent exact-tag build" >&2; exit 1; }
echo "Exact tag verified: ${tag} -> ${head_commit}"
