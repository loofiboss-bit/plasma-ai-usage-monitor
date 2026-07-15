#!/usr/bin/env python3
"""Release gate for v13's no-billable-background-traffic invariant."""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def fail(message: str) -> None:
    print(f"Non-invasive monitoring FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)

catalog = json.loads((ROOT / "package/contents/catalog/providers-v4.json").read_text())
for provider in catalog["providers"]:
    safe = provider.get("safeRefresh")
    if safe and (safe.get("method") != "GET" or safe.get("readOnly") is not True):
        fail(f"{provider['key']} scheduled adapter is not read-only GET")
    if provider.get("probePolicy") == "manual_only" and safe:
        fail(f"{provider['key']} manual-only adapter has a scheduled endpoint")

checks = {
    "plugin/openaicompatibleprovider.cpp": ("OpenAICompatibleProvider::refreshImpl", "OpenAICompatibleProvider::testConnectionNow"),
    "plugin/azureopenaiprovider.cpp": ("AzureOpenAIProvider::refreshImpl", "AzureOpenAIProvider::testConnectionNow"),
    "plugin/googleprovider.cpp": ("GoogleProvider::refreshImpl", "GoogleProvider::countTokensDiagnostic"),
    "plugin/anthropicprovider.cpp": ("AnthropicProvider::refreshImpl", "AnthropicProvider::countTokensDiagnostic"),
}
for relative, (scheduled, manual) in checks.items():
    text = (ROOT / relative).read_text()
    start = text.find(scheduled)
    end = text.find(manual, start)
    if start < 0 or end < 0:
        fail(f"cannot identify scheduled/manual boundary in {relative}")
    scheduled_body = text[start:end]
    if re.search(r"networkManager\(\)->(post|put|deleteResource)\s*\(", scheduled_body):
        fail(f"scheduled refresh performs a mutating request in {relative}")
    if "chat/completions" in scheduled_body or ":countTokens" in scheduled_body:
        fail(f"scheduled refresh reaches an inference endpoint in {relative}")

print(f"Non-invasive monitoring OK: {len(catalog['providers'])} provider profiles")
