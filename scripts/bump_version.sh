#!/usr/bin/env bash
# Bump version across all 4 version files atomically.
# Usage: bash scripts/bump_version.sh <new-version>
#   e.g: bash scripts/bump_version.sh 3.3.0
set -euo pipefail

NEW_VERSION="${1:-}"

if [[ -z "$NEW_VERSION" ]]; then
    echo "Usage: $0 <new-version>"
    echo "  Example: $0 3.3.0"
    exit 1
fi

if ! [[ "$NEW_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in X.Y.Z semver format, got: $NEW_VERSION"
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TODAY=$(date +%Y-%m-%d)

echo "Bumping version to ${NEW_VERSION} (release date: ${TODAY})"
echo ""

# 1. CMakeLists.txt — project(plasma-ai-usage-monitor VERSION X.Y.Z)
sed -i "s/project(plasma-ai-usage-monitor VERSION [0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*)/project(plasma-ai-usage-monitor VERSION ${NEW_VERSION})/" \
    "${ROOT_DIR}/CMakeLists.txt"
echo "  [OK] CMakeLists.txt"

# 2. package/metadata.json — "Version": "X.Y.Z"
sed -i "s/\"Version\": \"[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\"/\"Version\": \"${NEW_VERSION}\"/" \
    "${ROOT_DIR}/package/metadata.json"
echo "  [OK] package/metadata.json"

# 3. plasma-ai-usage-monitor.spec — ^Version: <whitespace>X.Y.Z
sed -i "s/^Version:[[:space:]]*.*/Version:        ${NEW_VERSION}/" \
    "${ROOT_DIR}/plasma-ai-usage-monitor.spec"
echo "  [OK] plasma-ai-usage-monitor.spec"

# 4. metainfo.xml — update the newest release entry only.
perl -0pi -e "s/<release version=\"[0-9]+\\.[0-9]+\\.[0-9]+\" date=\"[0-9-]+\"\\/>/<release version=\"${NEW_VERSION}\" date=\"${TODAY}\"\\/>/" \
    "${ROOT_DIR}/com.github.loofi.aiusagemonitor.metainfo.xml"
echo "  [OK] com.github.loofi.aiusagemonitor.metainfo.xml"

echo ""
echo "Running version consistency check..."
bash "${ROOT_DIR}/scripts/check_version_consistency.sh"

echo ""
echo "Done! Version bumped to ${NEW_VERSION} across all 4 files."
echo "Next steps:"
echo "  1. Update CHANGELOG.md with v${NEW_VERSION} entry"
echo "  2. Commit: git commit -am 'chore: bump version to v${NEW_VERSION}'"
echo "  3. Tag:    git tag v${NEW_VERSION}"
