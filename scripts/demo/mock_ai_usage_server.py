#!/usr/bin/env python3
"""Mock AI usage endpoints for Fedora KDE demo and live testing sessions."""

from __future__ import annotations

import argparse
import json
import re
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_PRESET = SCRIPT_DIR / "showcase_preset.json"
DEFAULT_PORT = 8080


def _json_bytes(payload: Any) -> bytes:
    return json.dumps(payload, separators=(",", ":")).encode("utf-8")


class DemoData:
    def __init__(self, payload: dict[str, Any]) -> None:
        self.payload = payload

    @classmethod
    def from_path(cls, path: Path) -> "DemoData":
        return cls(json.loads(path.read_text(encoding="utf-8")))

    def section(self, name: str) -> dict[str, Any]:
        value = self.payload.get(name, {})
        return value if isinstance(value, dict) else {}


class DemoRequestHandler(BaseHTTPRequestHandler):
    demo_data: DemoData

    def _path(self) -> str:
        return urlparse(self.path).path

    def _read_json_body(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0") or 0)
        if length <= 0:
            return {}

        raw = self.rfile.read(length)
        try:
            payload = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return {}
        return payload if isinstance(payload, dict) else {}

    def _send_json(
        self,
        payload: Any,
        *,
        status: HTTPStatus = HTTPStatus.OK,
        headers: dict[str, str] | None = None,
    ) -> None:
        body = _json_bytes(payload)
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        if headers:
            for key, value in headers.items():
                self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def _send_not_found(self, path: str) -> None:
        self._send_json({"error": "not found", "path": path}, status=HTTPStatus.NOT_FOUND)

    def _provider(self, name: str) -> dict[str, Any]:
        return self.demo_data.section(name)

    def _openai_headers(self, provider_name: str) -> dict[str, str]:
        provider = self._provider(provider_name)
        headers = provider.get("headers", {})
        return {
            "x-ratelimit-limit-requests": str(headers.get("requests_limit", 100)),
            "x-ratelimit-remaining-requests": str(headers.get("requests_remaining", 80)),
            "x-ratelimit-limit-tokens": str(headers.get("tokens_limit", 250000)),
            "x-ratelimit-remaining-tokens": str(headers.get("tokens_remaining", 240000)),
            "x-ratelimit-reset-requests": str(headers.get("reset", "30s")),
        }

    def _openai_usage_payload(self) -> dict[str, Any]:
        usage = self._provider("openai").get("usage", {})
        return {"data": [{"result": [usage]}]}

    def _openai_cost_payload(self) -> dict[str, Any]:
        amount = self._provider("openai").get("cost", {}).get("amount", 0)
        return {"data": [{"result": [{"amount": amount}]}]}

    def _claude_bootstrap_payload(self) -> dict[str, Any]:
        return self.demo_data.section("claude_code").get(
            "bootstrap",
            {
                "account": {
                    "memberships": [
                        {
                            "organization": {
                                "uuid": "org-demo-claude",
                                "subscription": {"type": "max_5x"},
                            }
                        }
                    ]
                }
            },
        )

    def _claude_usage_payload(self) -> dict[str, Any]:
        return self.demo_data.section("claude_code").get(
            "usage",
            {
                "five_hour": {
                    "utilization": 42,
                    "resets_at": "2026-03-29T17:00:00Z",
                },
                "seven_day": {"utilization": 18},
                "extra_usage": {
                    "spent_cents": 750,
                    "monthly_limit_cents": 5000,
                    "resets_at": "2026-04-01T00:00:00Z",
                },
            },
        )

    def _codex_account_payload(self) -> dict[str, Any]:
        return self.demo_data.section("codex_cli").get(
            "account_check",
            {
                "accounts": {
                    "demo-account": {
                        "entitlement": "pro",
                        "remaining_credits": 37.5,
                        "rate_limits": {
                            "message_cap": {
                                "type": "five_hour",
                                "remaining": 210,
                                "limit": 300,
                                "resets_at": "2026-03-29T17:00:00Z",
                            },
                            "weekly_cap": {
                                "type": "week",
                                "remaining": 420,
                                "limit": 500,
                                "resets_at": "2026-04-02T00:00:00Z",
                            },
                            "code_review": {
                                "type": "code_review",
                                "remaining": 18,
                                "limit": 20,
                                "resets_at": "2026-03-31T00:00:00Z",
                            },
                        },
                    }
                }
            },
        )

    def _copilot_billing_payload(self) -> dict[str, Any]:
        return self.demo_data.section("copilot").get(
            "billing",
            {
                "total_seats": 12,
                "seat_breakdown": {"active_this_cycle": 7},
            },
        )

    def _resolve_openai_compatible_provider(self, path: str, payload: dict[str, Any]) -> str | None:
        explicit_paths = {
            "/mock/mistral/chat/completions": "mistral",
            "/mock/deepseek/chat/completions": "deepseek",
            "/mock/groq/chat/completions": "groq",
            "/mock/xai/chat/completions": "xai",
            "/mock/openrouter/chat/completions": "openrouter",
            "/mock/together/chat/completions": "together",
            "/mock/cohere/chat/completions": "cohere",
        }
        if path in explicit_paths:
            return explicit_paths[path]

        if path != "/chat/completions":
            return None

        model = str(payload.get("model", "")).lower()
        if "deepseek" in model:
            return "deepseek"
        if "grok" in model or "xai" in model:
            return "xai"
        if "command" in model or "cohere" in model:
            return "cohere"
        if "mistral" in model:
            return "mistral"
        if "versatile" in model or "groq" in model:
            return "groq"
        if model.startswith(("openai/", "anthropic/", "google/")):
            return "openrouter"
        if "turbo" in model or "mixtral" in model or "gemma" in model:
            return "together"

        return "mistral"

    def do_GET(self) -> None:  # noqa: N802 - stdlib hook name
        path = self._path()

        if path in {"/healthz", "/health"}:
            self._send_json({"status": "ok"})
            return

        if path == "/spend/logs":
            self._send_json(self._provider("litellm").get("spend_logs", []))
            return

        if path == "/models":
            model_ids = [
                *self._provider("cerebras").get("models", []),
                *self._provider("fireworks").get("models", []),
            ]
            self._send_json(
                {
                    "data": [{"id": model_id} for model_id in model_ids]
                }
            )
            return

        if path == "/v1/models":
            self._send_json(
                {"data": [{"id": model_id} for model_id in self._provider("perplexity").get("models", [])]}
            )
            return

        if path in {
            "/organization/usage/completions",
            "/mock/openai/v1/organization/usage/completions",
        }:
            self._send_json(self._openai_usage_payload(), headers=self._openai_headers("openai"))
            return

        if path in {
            "/organization/costs",
            "/mock/openai/v1/organization/costs",
        }:
            self._send_json(self._openai_cost_payload())
            return

        if path == "/openai/v1/dashboard/billing/usage":
            total_usage = self._provider("openai").get("cost", {}).get("amount", 0)
            self._send_json({"total_usage": total_usage, "daily_costs": []})
            return

        if path == "/openai/v1/dashboard/billing/subscription":
            self._send_json({"hard_limit_usd": 120.0, "soft_limit_usd": 100.0})
            return

        if path in {"/user/balance", "/mock/deepseek/user/balance"}:
            balance = self._provider("deepseek").get("balance", 0)
            self._send_json({"is_available": True, "balance_infos": [{"total_balance": str(balance)}]})
            return

        if path in {"/auth/key", "/mock/openrouter/auth/key"}:
            credits = self._provider("openrouter").get("credits", {"usage": 0.0, "limit": 0.0})
            self._send_json({"data": {"label": "demo-key", "usage": credits.get("usage", 0.0), "limit": credits.get("limit", 0.0)}})
            return

        if path == "/claude/api/bootstrap":
            self._send_json(self._claude_bootstrap_payload())
            return

        if re.fullmatch(r"/claude/api/organizations/[^/]+/usage", path):
            self._send_json(self._claude_usage_payload())
            return

        if path == "/chatgpt/backend-api/accounts/check/v4-2023-04-27":
            self._send_json(self._codex_account_payload())
            return

        if re.fullmatch(r"/copilot/orgs/[^/]+/copilot/billing", path):
            self._send_json(self._copilot_billing_payload())
            return

        if path == "/anthropic/v1/workspaces":
            self._send_json({"type": "list", "data": [{"type": "workspace", "id": "wksp_demo", "name": "Demo Workspace"}]})
            return

        if path == "/google/v1/projects/demo/usage":
            self._send_json({"usage": 2.50})
            return

        google_veo_model = re.fullmatch(r"(?:/mock/googleveo/v1beta)?/models/(?P<model>[^/]+)", path)
        if google_veo_model:
            payload = self._provider("googleveo")
            headers = {
                "x-ratelimit-limit-requests": str(payload.get("headers", {}).get("requests_limit", 30)),
                "x-ratelimit-remaining-requests": str(payload.get("headers", {}).get("requests_remaining", 24)),
            }
            self._send_json(
                {
                    "name": google_veo_model.group("model"),
                    "displayName": "Veo demo model",
                    "usage": payload.get("usage", {}),
                    "metadata": {
                        "video_duration_seconds": payload.get("usage", {}).get("video_duration_seconds", 0),
                    },
                },
                headers=headers,
            )
            return

        self._send_not_found(path)

    def do_POST(self) -> None:  # noqa: N802 - stdlib hook name
        path = self._path()
        payload = self._read_json_body()

        if path in {
            "/v1/messages/count_tokens",
            "/mock/anthropic/v1/messages/count_tokens",
        }:
            headers = self._provider("anthropic").get("headers", {})
            self._send_json(
                {},
                headers={
                    "anthropic-ratelimit-requests-limit": str(headers.get("requests_limit", 120)),
                    "anthropic-ratelimit-requests-remaining": str(headers.get("requests_remaining", 84)),
                    "anthropic-ratelimit-input-tokens-limit": str(headers.get("input_tokens_limit", 240000)),
                    "anthropic-ratelimit-input-tokens-remaining": str(headers.get("input_tokens_remaining", 180000)),
                    "anthropic-ratelimit-output-tokens-limit": str(headers.get("output_tokens_limit", 120000)),
                    "anthropic-ratelimit-output-tokens-remaining": str(headers.get("output_tokens_remaining", 96000)),
                    "anthropic-ratelimit-requests-reset": str(headers.get("reset", "2026-03-16T18:30:00Z")),
                },
            )
            return

        google_model = re.fullmatch(r"(?:/mock/google/v1beta)?/models/(?P<model>[^:]+):countTokens", path)
        if google_model:
            total_tokens = self._provider("google").get("total_tokens", 12)
            self._send_json({"model": google_model.group("model"), "totalTokens": total_tokens})
            return

        provider_name = self._resolve_openai_compatible_provider(path, payload)
        if provider_name:
            provider = self._provider(provider_name)
            self._send_json({"usage": provider.get("usage", {})}, headers=self._openai_headers(provider_name))
            return

        azure_match = re.fullmatch(
            r"(?:/mock/azure)?/openai/deployments/(?P<deployment>[^/]+)/chat/completions",
            path,
        )
        if azure_match:
            provider = self._provider("azure")
            headers = provider.get("headers", {})
            self._send_json(
                {
                    "id": f"chatcmpl-{azure_match.group('deployment')}",
                    "object": "chat.completion",
                    "usage": provider.get("usage", {}),
                    "cost": provider.get("cost", {}),
                },
                headers={
                    "x-ratelimit-limit-requests": str(headers.get("requests_limit", 80)),
                    "x-ratelimit-remaining-requests": str(headers.get("requests_remaining", 67)),
                    "x-ratelimit-limit-tokens": str(headers.get("tokens_limit", 300000)),
                    "x-ratelimit-remaining-tokens": str(headers.get("tokens_remaining", 289800)),
                    "x-ratelimit-reset-requests": "25s",
                },
            )
            return

        self._send_not_found(path)

    def log_message(self, fmt: str, *args: Any) -> None:
        return


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="Host interface to bind")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Port to bind")
    parser.add_argument("--preset", type=Path, default=DEFAULT_PRESET, help="Path to the JSON preset file")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    demo_data = DemoData.from_path(args.preset)
    handler = type("ConfiguredDemoRequestHandler", (DemoRequestHandler,), {"demo_data": demo_data})
    server = ThreadingHTTPServer((args.host, args.port), handler)
    base_url = f"http://{server.server_address[0]}:{server.server_address[1]}"

    print(f"Mock AI usage server running on {base_url}")
    print("Demo mode routes are compatible with PLASMA_AI_MONITOR_DEMO=1.")
    print(f"Health check:         {base_url}/healthz")
    print(f"OpenAI base URL:      {base_url}/mock/openai/v1")
    print(f"Anthropic base URL:   {base_url}/mock/anthropic/v1")
    print(f"Google base URL:      {base_url}/mock/google/v1beta")
    print(f"Google Veo base URL:  {base_url}/mock/googleveo/v1beta")
    print(f"Azure base URL:       {base_url}/mock/azure")
    print("OpenAI-compatible providers can point to /mock/<provider> or rely on demo-mode defaults.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
