#!/usr/bin/env python3
"""Create an Ed25519-signed provider catalog feed envelope.

The private key is read only by OpenSSL from a caller-provided PEM path.  It
is never copied into the repository, output envelope, logs, or generated
artifacts.
"""

from __future__ import annotations

import argparse
import base64
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT / "package/contents/catalog/providers-v4.json"


def canonical_json(value: object) -> bytes:
    def normalize_numbers(item: object) -> object:
        if isinstance(item, float) and item.is_integer():
            return int(item)
        if isinstance(item, dict):
            return {key: normalize_numbers(child) for key, child in item.items()}
        if isinstance(item, list):
            return [normalize_numbers(child) for child in item]
        return item

    return json.dumps(
        normalize_numbers(value),
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def require_future(value: str, label: str) -> None:
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.astimezone(timezone.utc) <= datetime.now(timezone.utc):
        raise SystemExit(f"{label} must be a future ISO-8601 timestamp with timezone")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--key", type=Path, required=True, help="Ed25519 PEM private key; keep it outside the repository")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sequence", type=int, required=True)
    parser.add_argument("--expires-at", required=True)
    parser.add_argument("--min-app-version", default="")
    parser.add_argument("--key-id", default="catalog-ed25519-v1")
    args = parser.parse_args()

    if args.sequence < 1:
        raise SystemExit("--sequence must be positive")
    if not args.catalog.is_file() or not args.key.is_file():
        raise SystemExit("catalog and signing key must be existing files")
    require_future(args.expires_at, "--expires-at")
    payload = json.loads(args.catalog.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit("catalog root must be an object")
    if payload.get("schemaVersion") != 7 or payload.get("runtimeScraping") is not False:
        raise SystemExit("catalog must be schema v7 with runtimeScraping=false")
    if payload.get("sequence") != args.sequence:
        raise SystemExit("catalog sequence must match --sequence")
    hard_expiry = payload.get("hardExpiresAt")
    if not isinstance(hard_expiry, str):
        raise SystemExit("catalog hardExpiresAt is missing")
    require_future(hard_expiry, "catalog hardExpiresAt")
    payload["verificationState"] = "remote_verified"

    payload_bytes = canonical_json(payload)
    with tempfile.TemporaryDirectory(prefix="aiusage-catalog-sign-") as directory:
        directory_path = Path(directory)
        payload_path = directory_path / "payload.json"
        signature_path = directory_path / "signature.bin"
        payload_path.write_bytes(payload_bytes)
        subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-rawin", "-inkey", str(args.key), "-in", str(payload_path), "-out", str(signature_path)],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        signature = signature_path.read_bytes()

    envelope = {
        "sequence": args.sequence,
        "expiresAt": args.expires_at,
        "minAppVersion": args.min_app_version,
        "keyId": args.key_id,
        "sha256": hashlib.sha256(payload_bytes).hexdigest(),
        "signature": base64.b64encode(signature).decode("ascii"),
        "payload": payload,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(envelope, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"Signed catalog feed sequence {args.sequence} -> {args.output}")


if __name__ == "__main__":
    main()
