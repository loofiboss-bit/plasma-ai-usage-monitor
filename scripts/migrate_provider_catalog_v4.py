#!/usr/bin/env python3
"""One-time, deterministic Provider Catalog v3 to v4 migration."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "package/contents/catalog/providers-v3.json"
TARGET = ROOT / "package/contents/catalog/providers-v4.json"

DESCRIPTORS = {
    "openai": ("OpenAI", "OpenAI", "#10A37F", "https://api.openai.com/v1", "bearer", ["openai"]),
    "anthropic": ("Anthropic", "Anthropic", "#D4A574", "https://api.anthropic.com/v1", "x-api-key", ["anthropic"]),
    "google": ("Google Gemini", "Google", "#4285F4", "https://generativelanguage.googleapis.com/v1beta", "query-key", ["google"]),
    "mistral": ("Mistral AI", "Mistral", "#FF7000", "https://api.mistral.ai/v1", "bearer", ["mistral"]),
    "deepseek": ("DeepSeek", "DeepSeek", "#5B6EE1", "https://api.deepseek.com/v1", "bearer", ["deepseek"]),
    "groq": ("Groq", "Groq", "#F55036", "https://api.groq.com/openai/v1", "bearer", ["groq"]),
    "xai": ("xAI / Grok", "xAI", "#1DA1F2", "https://api.x.ai/v1", "bearer", ["xai"]),
    "ollama": ("Ollama Cloud", "OllamaCloud", "#111827", "https://ollama.com/v1", "bearer", ["ollama"]),
    "openrouter": ("OpenRouter", "OpenRouter", "#6366F1", "https://openrouter.ai/api/v1", "bearer", ["openrouter"]),
    "together": ("Together AI", "Together", "#0EA5E9", "https://api.together.xyz/v1", "bearer", ["together"]),
    "cohere": ("Cohere", "Cohere", "#39D353", "https://api.cohere.com/v2", "bearer", ["cohere"]),
    "googleveo": ("Google Veo", "GoogleVeo", "#EA4335", "https://generativelanguage.googleapis.com/v1beta", "query-key", ["googleveo"]),
    "azure": ("Azure OpenAI", "AzureOpenAI", "#0078D4", "", "api-key", ["azure_openai_api_key"]),
    "bedrock": ("AWS Bedrock", "AWS Bedrock", "#FF9900", "https://bedrock-runtime.{region}.amazonaws.com", "aws-sigv4", ["bedrock_access_key_id", "bedrock_secret_access_key", "bedrock_session_token"]),
}


def main() -> None:
    payload = json.loads(SOURCE.read_text(encoding="utf-8"))
    payload["schemaVersion"] = 4
    payload["catalogVersion"] = "2026.07.13"
    payload["lastReviewed"] = "2026-07-13"
    for provider in payload["providers"]:
        key = provider["key"]
        name, db_name, color, endpoint, auth, slots = DESCRIPTORS[key]
        provider.update({
            "stableId": key,
            "displayName": name,
            "dbName": db_name,
            "icon": key,
            "colorToken": color,
            "auth": {"scheme": auth, "credentialSlots": slots},
            "endpoint": {"default": endpoint, "customPolicy": "required" if key == "azure" else "allowed"},
            "capabilities": ["connectivity", "usage", "cost", "rateLimits"],
            "expectedSources": ["billing_api", "usage_api", "response_headers", "estimate"],
            "probePolicy": "read_only_account_api",
            "reviewExpiresAt": "2026-08-27",
            "config": {
                "key": key,
                "enabled": f"{key}Enabled",
                "model": f"{key}Model",
                "refreshInterval": f"{key}RefreshInterval",
                "notifications": f"{key}NotificationsEnabled",
                "dailyBudget": f"{key}DailyBudget",
                "monthlyBudget": f"{key}MonthlyBudget",
            },
        })
        for model in provider["models"]:
            lifecycle = model.setdefault("lifecycle", {})
            lifecycle.setdefault("status", "active")
            if lifecycle["status"] == "current":
                lifecycle["status"] = "active"
            if key == "deepseek" and model["id"] in {"deepseek-chat", "deepseek-reasoner"}:
                lifecycle.update({
                    "status": "deprecated",
                    "deprecationDate": "2026-07-24",
                    "replacementId": "deepseek-v4-flash",
                })
    TARGET.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
