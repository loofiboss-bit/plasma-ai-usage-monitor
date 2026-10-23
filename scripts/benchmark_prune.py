#!/usr/bin/env python3
import time
import sqlite3
import os

def benchmark():
    # Setup temporary database
    db_path = "/tmp/test_prune_benchmark.db"
    if os.path.exists(db_path):
        os.remove(db_path)

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    tables = [
        ("usage_snapshots", "timestamp"),
        ("rate_limit_events", "timestamp"),
        ("subscription_tool_usage", "timestamp"),
        ("observations", "observed_at_utc"),
        ("guardrail_events", "observed_at_utc"),
        ("estimate_provenance", "observed_at_utc"),
        ("budget_policy_events", "created_at_utc")
    ]

    for table, col in tables:
        cursor.execute(f"CREATE TABLE {table} (id INTEGER PRIMARY KEY, {col} TEXT)")
        # Insert 100,000 dummy rows per table
        rows = [("2020-01-01 00:00:00",) for _ in range(10000)]
        cursor.executemany(f"INSERT INTO {table} ({col}) VALUES (?)", rows)
    conn.commit()

    cutoff = "2025-01-01 00:00:00"

    # Measure unoptimized baseline string formatting/preparation in Python/SQLite
    iterations = 1000
    start = time.perf_counter()
    for _ in range(iterations):
        for table, col in tables:
            # Unoptimized: string formatting inside loop every iteration
            query_str = "DELETE FROM %s WHERE datetime(%s) < datetime(?)" % (table, col)
            # simulate prep / string manipulation
            _ = query_str
    unoptimized_time = time.perf_counter() - start

    # Pre-built query strings list
    prebuilt_queries = [
        f"DELETE FROM {table} WHERE datetime({col}) < datetime(?)"
        for table, col in tables
    ]

    start = time.perf_counter()
    for _ in range(iterations):
        for query_str in prebuilt_queries:
            # Optimized: using prebuilt query string directly
            _ = query_str
    optimized_time = time.perf_counter() - start

    print(f"Iterations: {iterations} x {len(tables)} queries")
    print(f"Unoptimized string formatting time: {unoptimized_time:.6f} s")
    print(f"Optimized prebuilt string time:     {optimized_time:.6f} s")
    if unoptimized_time > 0:
        improvement = (unoptimized_time - optimized_time) / unoptimized_time * 100
        print(f"Speedup / Overhead reduction:       {improvement:.2f}%")

    conn.close()
    if os.path.exists(db_path):
        os.remove(db_path)

if __name__ == "__main__":
    benchmark()
