#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

pattern='v[0-9]+\.[0-9]+\.[0-9]+|["'\''][0-9]+\.[0-9]+\.[0-9]+["'\'']'

if command -v rg >/dev/null 2>&1; then
  hardcoded="$(rg -n -e "$pattern" package/contents/ui package/contents/config || true)"
else
  hardcoded="$(grep -R -n -E "$pattern" package/contents/ui package/contents/config || true)"
fi

if [[ -n "$hardcoded" ]]; then
  echo "Hardcoded semantic version string detected in QML:"
  echo "$hardcoded"
  exit 1
fi

echo "No hardcoded semantic version strings detected in QML."
