#!/usr/bin/env python3
"""Create isolated schema-v4 history fixtures for v16 release media."""

from __future__ import annotations

import argparse
import sqlite3
from datetime import datetime, timedelta, timezone
from pathlib import Path


SCENARIOS = ("retained", "gap", "analyst-sufficient", "analyst-insufficient")


def timestamp(value: datetime) -> str:
    return value.astimezone(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")


def create_schema(connection: sqlite3.Connection) -> None:
    connection.executescript(
        """
        PRAGMA user_version = 4;
        CREATE TABLE observations (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          provider TEXT NOT NULL,
          observed_at_utc DATETIME NOT NULL,
          interval_start_utc DATETIME,
          interval_end_utc DATETIME,
          metric_kind TEXT NOT NULL,
          unit TEXT NOT NULL,
          value REAL NULL,
          currency TEXT,
          semantic TEXT NOT NULL,
          source TEXT NOT NULL,
          data_quality TEXT NOT NULL DEFAULT 'unknown',
          scope TEXT NOT NULL DEFAULT 'api_key',
          window TEXT NOT NULL DEFAULT 'current',
          model_scope TEXT DEFAULT '',
          project_scope TEXT DEFAULT '',
          reset_at_utc DATETIME,
          correlation_id TEXT NOT NULL
        );
        CREATE INDEX idx_observations_provider_time_source_currency
          ON observations(provider, observed_at_utc, source, currency);
        CREATE TABLE usage_snapshots (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          timestamp DATETIME,
          provider TEXT NOT NULL,
          model TEXT DEFAULT '',
          input_tokens INTEGER DEFAULT 0,
          output_tokens INTEGER DEFAULT 0,
          request_count INTEGER DEFAULT 0,
          cost REAL DEFAULT 0.0,
          is_estimated_cost INTEGER DEFAULT 0,
          daily_cost REAL DEFAULT 0.0,
          monthly_cost REAL DEFAULT 0.0,
          rl_requests INTEGER DEFAULT 0,
          rl_requests_remaining INTEGER DEFAULT 0,
          rl_tokens INTEGER DEFAULT 0,
          rl_tokens_remaining INTEGER DEFAULT 0,
          cost_source TEXT NOT NULL DEFAULT 'unknown',
          usage_source TEXT NOT NULL DEFAULT 'unknown',
          currency TEXT DEFAULT 'USD',
          data_quality TEXT DEFAULT 'unknown'
        );
        CREATE TABLE rate_limit_events (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          timestamp DATETIME,
          provider TEXT NOT NULL,
          event_type TEXT NOT NULL,
          percent_used INTEGER DEFAULT 0
        );
        CREATE TABLE subscription_tool_usage (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          timestamp DATETIME,
          tool_name TEXT NOT NULL,
          usage_count INTEGER DEFAULT 0,
          usage_limit INTEGER DEFAULT 0,
          period_type TEXT NOT NULL,
          plan_tier TEXT DEFAULT '',
          limit_reached BOOLEAN DEFAULT 0
        );
        """
    )


def insert_observation(
    connection: sqlite3.Connection,
    observed_at: datetime,
    kind: str,
    unit: str,
    value: float,
    semantic: str = "interval_total",
    model: str = "gpt-5",
) -> None:
    connection.execute(
        """
        INSERT INTO observations(
          provider, observed_at_utc, interval_start_utc, interval_end_utc,
          metric_kind, unit, value, currency, semantic, source, data_quality,
          scope, window, model_scope, correlation_id
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            "OpenAI",
            timestamp(observed_at),
            timestamp(observed_at - timedelta(hours=1)),
            timestamp(observed_at),
            kind,
            unit,
            value,
            "USD" if kind == "cost" else None,
            semantic,
            "billing_api" if kind == "cost" else "usage_api",
            "complete",
            "organization",
            "day" if kind == "cost" else "current",
            model,
            f"v16-media-{kind}-{observed_at.timestamp():.0f}",
        ),
    )


def seed_history(connection: sqlite3.Connection, with_gap: bool) -> None:
    end = datetime.now(timezone.utc).replace(minute=0, second=0, microsecond=0)
    for offset in range(72, -1, -1):
        if with_gap and 30 <= offset <= 38:
            continue
        observed_at = end - timedelta(hours=offset)
        value = 0.14 + ((72 - offset) % 9) * 0.025
        insert_observation(connection, observed_at, "cost", "USD", value)


def seed_analyst(connection: sqlite3.Connection, days: int) -> None:
    today = datetime.now(timezone.utc).replace(
        hour=12, minute=0, second=0, microsecond=0
    )
    for offset in range(days - 1, -1, -1):
        observed_at = today - timedelta(days=offset)
        day_index = days - offset
        cost = 1.25 + (day_index % 5) * 0.42
        input_tokens = 9000 + day_index * 430
        output_tokens = 2800 + day_index * 170
        insert_observation(connection, observed_at, "cost", "USD", cost)
        insert_observation(
            connection,
            observed_at,
            "input_tokens",
            "token",
            input_tokens,
            "interval_total",
        )
        insert_observation(
            connection,
            observed_at,
            "output_tokens",
            "token",
            output_tokens,
            "interval_total",
        )
        connection.execute(
            """
            INSERT INTO usage_snapshots(
              timestamp, provider, model, input_tokens, output_tokens,
              request_count, cost, daily_cost, cost_source, usage_source,
              currency, data_quality
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                timestamp(observed_at),
                "OpenAI",
                "gpt-5",
                input_tokens,
                output_tokens,
                24 + day_index,
                cost,
                cost,
                "billing_api",
                "usage_api",
                "USD",
                "complete",
            ),
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.unlink(missing_ok=True)
    connection = sqlite3.connect(args.output)
    try:
        create_schema(connection)
        if args.scenario in {"retained", "gap"}:
            seed_history(connection, with_gap=args.scenario == "gap")
        else:
            seed_analyst(
                connection,
                days=21 if args.scenario == "analyst-sufficient" else 2,
            )
        connection.commit()
    finally:
        connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
