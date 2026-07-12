#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

version="$(tr -d '[:space:]' < VERSION)"
semver_re='^[0-9]+\.[0-9]+\.[0-9]+$'
[[ "$version" =~ $semver_re ]] || { echo "Invalid VERSION: ${version}" >&2; exit 1; }

metadata_version="$(python3 -c 'import json; print(json.load(open("package/metadata.json"))["KPlugin"]["Version"])')"
required_plugin_version="$(python3 -c 'import json; print(json.load(open("package/metadata.json"))["X-AIUsageMonitor-RequiredPluginVersion"])')"
provider_release="$(python3 -c 'import json; print(json.load(open("package/contents/catalog/providers-v4.json"))["release"])')"
subscription_release="$(python3 -c 'import json; print(json.load(open("package/contents/catalog/subscriptions-v1.json"))["release"])')"
spec_version="$(sed -n 's/^Version:[[:space:]]*\([0-9.]*\).*/\1/p' plasma-ai-usage-monitor.spec | head -1)"
metainfo_version="$(sed -n 's/.*<release version="\([0-9.]*\)".*/\1/p' com.github.loofi.aiusagemonitor.metainfo.xml | head -1)"

for entry in \
  "metadata:${metadata_version}" \
  "required plugin:${required_plugin_version}" \
  "provider catalog:${provider_release}" \
  "subscription catalog:${subscription_release}" \
  "RPM spec:${spec_version}" \
  "AppStream:${metainfo_version}"; do
  label="${entry%%:*}"
  value="${entry#*:}"
  [[ "$value" == "$version" ]] || { echo "Version mismatch: ${label}=${value}, VERSION=${version}" >&2; exit 1; }
done

grep -Fq 'file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"' CMakeLists.txt || {
  echo "CMake does not derive its version from VERSION" >&2
  exit 1
}

echo "Version consistency OK: ${version}"
