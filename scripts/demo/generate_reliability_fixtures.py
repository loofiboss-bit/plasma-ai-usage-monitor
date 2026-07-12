#!/usr/bin/env python3
"""Create deterministic v12 provider states and observation databases."""

from __future__ import annotations

import argparse
import json
import sqlite3
from datetime import datetime, timedelta, timezone
from pathlib import Path

STATES = [
    "disabled", "missing_key", "loading", "healthy", "stale", "auth_failure",
    "rate_limited", "network_failure", "partial_data", "mixed_currency",
    "catalog_deprecated_model",
]


def create_database(path: Path, rows: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.unlink(missing_ok=True)
    db = sqlite3.connect(path)
    db.executescript("""
        PRAGMA user_version=3;
        PRAGMA journal_mode=WAL;
        CREATE TABLE observations (
          id INTEGER PRIMARY KEY, provider TEXT NOT NULL, observed_at_utc DATETIME NOT NULL,
          interval_start_utc DATETIME, interval_end_utc DATETIME,
          metric_kind TEXT NOT NULL, unit TEXT NOT NULL, value REAL NOT NULL,
          currency TEXT, semantic TEXT NOT NULL, source TEXT NOT NULL,
          data_quality TEXT NOT NULL, model_scope TEXT, project_scope TEXT,
          correlation_id TEXT NOT NULL
        );
        CREATE INDEX idx_observations_provider_time_source_currency
          ON observations(provider, observed_at_utc, source, currency);
    """)
    start = datetime(2026, 1, 1, tzinfo=timezone.utc)
    providers = ["OpenAI", "Anthropic", "Google", "EuropeanFixture"]
    batch = []
    for index in range(rows):
        provider = providers[index % len(providers)]
        observed = start + timedelta(minutes=index)
        currency = "EUR" if provider == "EuropeanFixture" else "USD"
        batch.append((
            provider, observed.isoformat(), "cost", currency, (index % 1000) / 100,
            currency, "interval_total", "billing_api", "complete", "fixture-model",
            f"fixture-{index}",
        ))
    db.executemany("""
        INSERT INTO observations(provider, observed_at_utc, metric_kind, unit, value,
          currency, semantic, source, data_quality, model_scope, correlation_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, batch)
    db.commit()
    db.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path(".demo-output/reliability-v12"))
    parser.add_argument("--rows", type=int, nargs="+", default=[1000, 10000, 100000])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "provider-states.json").write_text(
        json.dumps({"schema": 1, "states": STATES}, indent=2) + "\n", encoding="utf-8")
    for rows in args.rows:
        create_database(args.output / f"observations-{rows}.db", rows)
    print(f"Created deterministic reliability fixtures in {args.output}")


if __name__ == "__main__":
    main()
