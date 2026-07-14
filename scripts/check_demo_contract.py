#!/usr/bin/env python3
"""Exercise the deterministic demo server against provider runtime contracts."""

from __future__ import annotations

import json
import sys
import threading
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
DEMO_DIR = ROOT / "scripts" / "demo"
sys.path.insert(0, str(DEMO_DIR))

from mock_ai_usage_server import DemoData, DemoRequestHandler  # noqa: E402


class QuietDemoRequestHandler(DemoRequestHandler):
    def log_message(self, _format: str, *_args: object) -> None:
        return


def fetch_json(base_url: str, path: str) -> object:
    with urlopen(base_url + path, timeout=2) as response:  # noqa: S310 - loopback fixture
        if response.status != 200:
            raise AssertionError(f"{path} returned HTTP {response.status}")
        return json.loads(response.read().decode("utf-8"))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    QuietDemoRequestHandler.demo_data = DemoData.from_path(DEMO_DIR / "showcase_preset.json")
    server = ThreadingHTTPServer(("127.0.0.1", 0), QuietDemoRequestHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base_url = f"http://127.0.0.1:{server.server_port}"

    try:
        openrouter = fetch_json(base_url, "/key")
        require(isinstance(openrouter, dict), "OpenRouter fixture must be an object")
        key_data = openrouter.get("data", {})
        required_key_fields = {
            "usage",
            "usage_daily",
            "usage_weekly",
            "usage_monthly",
            "limit_remaining",
            "limit_reset",
        }
        require(required_key_fields <= set(key_data), "OpenRouter /key fixture is missing v13 fields")
        require(key_data["limit_remaining"] == 43.75, "OpenRouter remaining limit drifted")

        proxy_key = fetch_json(base_url, "/mock/openrouter/key")
        require(proxy_key == openrouter, "OpenRouter root and provider-prefixed demo routes differ")

        spend = fetch_json(base_url, "/spend/logs")
        require(isinstance(spend, list) and spend[0]["currency"] == "USD", "LiteLLM spend fixture drifted")

        models = fetch_json(base_url, "/models")
        model_ids = {entry["id"] for entry in models.get("data", [])}
        require("llama-4-scout-17b-16e-instruct" in model_ids, "Cerebras fixture is missing")
        require(
            "accounts/fireworks/models/llama-v3p3-70b-instruct" in model_ids,
            "Fireworks fixture is missing",
        )

        perplexity = fetch_json(base_url, "/v1/models")
        perplexity_ids = {entry["id"] for entry in perplexity.get("data", [])}
        require("perplexity/sonar" in perplexity_ids, "Perplexity fixture is missing")

        try:
            fetch_json(base_url, "/auth/key")
        except HTTPError as error:
            require(error.code == 404, "Retired OpenRouter /auth/key route did not return 404")
        else:
            raise AssertionError("Retired OpenRouter /auth/key route is still accepted")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)

    print("Demo contract check OK: OpenRouter /key plus four v13 launch providers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
