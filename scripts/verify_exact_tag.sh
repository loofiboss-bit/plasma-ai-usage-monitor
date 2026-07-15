#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

tag="${1:-}"
version="$(tr -d '[:space:]' < VERSION)"
expected="v${version}"
if [[ "$tag" != "$expected" && ! "$tag" =~ ^v${version}-(alpha|beta|rc)\.[1-9][0-9]*$ ]]; then
  echo "Expected ${expected} or a numbered alpha/beta/rc tag for that version, got ${tag:-<none>}" >&2
  exit 1
fi
tag_commit="$(git rev-parse "${tag}^{commit}")"
head_commit="$(git rev-parse HEAD)"
[[ "$tag_commit" == "$head_commit" ]] || { echo "Tag ${tag} does not resolve to HEAD" >&2; exit 1; }
[[ -z "$(git status --porcelain --untracked-files=no)" ]] || { echo "Tracked worktree changes prevent exact-tag build" >&2; exit 1; }

if [[ "$tag" == "$expected" ]]; then
  rc_tag="${expected}-rc.1"
  rc_commit="$(git rev-parse "${rc_tag}^{commit}" 2>/dev/null)" || {
    echo "Stable promotion requires ${rc_tag}" >&2
    exit 1
  }
  [[ "$rc_commit" == "$tag_commit" ]] || {
    echo "Stable tag ${tag} and ${rc_tag} do not resolve to the same commit" >&2
    exit 1
  }

  main_commit="$(git rev-parse 'refs/remotes/origin/main^{commit}' 2>/dev/null)" || {
    echo "Stable promotion requires refs/remotes/origin/main" >&2
    exit 1
  }
  [[ "$main_commit" == "$tag_commit" ]] || {
    echo "Stable tag ${tag}, ${rc_tag}, and origin/main must resolve to the same commit" >&2
    exit 1
  }

  rc_tagged_at="$(git for-each-ref --format='%(taggerdate:unix)' "refs/tags/${rc_tag}")"
  [[ "$rc_tagged_at" =~ ^[0-9]+$ ]] || {
    echo "${rc_tag} must be an annotated tag with a verifiable tagger date" >&2
    exit 1
  }
  now_epoch="$(date +%s)"
  minimum_soak_seconds=$((7 * 24 * 60 * 60))
  soak_seconds=$((now_epoch - rc_tagged_at))
  (( soak_seconds >= minimum_soak_seconds )) || {
    echo "${rc_tag} has soaked for ${soak_seconds}s; stable promotion requires ${minimum_soak_seconds}s" >&2
    exit 1
  }

  echo "Stable lineage verified: ${tag} = ${rc_tag} = origin/main; soak ${soak_seconds}s"
fi

echo "Exact tag verified: ${tag} -> ${head_commit}"
