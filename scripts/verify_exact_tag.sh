#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

tag="${1:-}"
version="$(tr -d '[:space:]' < VERSION)"
expected="v${version}"
if [[ "$tag" != "$expected" ]]; then
  echo "Expected direct stable tag ${expected}, got ${tag:-<none>}" >&2
  exit 1
fi
tag_commit="$(git rev-parse "${tag}^{commit}")"
head_commit="$(git rev-parse HEAD)"
[[ "$tag_commit" == "$head_commit" ]] || { echo "Tag ${tag} does not resolve to HEAD" >&2; exit 1; }
[[ -z "$(git status --porcelain --untracked-files=no)" ]] || { echo "Tracked worktree changes prevent exact-tag build" >&2; exit 1; }

tag_object_type="$(git cat-file -t "$tag" 2>/dev/null)"
[[ "$tag_object_type" == "tag" ]] || {
  echo "${tag} must be an annotated tag" >&2
  exit 1
}

main_commit="$(git rev-parse 'refs/remotes/origin/main^{commit}' 2>/dev/null)" || {
  echo "Direct stable publication requires refs/remotes/origin/main" >&2
  exit 1
}
[[ "$main_commit" == "$tag_commit" ]] || {
  echo "Stable tag ${tag} and origin/main must resolve to the same commit" >&2
  exit 1
}

echo "Direct stable tag verified: ${tag} = origin/main -> ${head_commit}"
