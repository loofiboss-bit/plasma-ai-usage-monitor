#!/usr/bin/env python3
"""Regression tests for actionable versus expected catalog drift."""

from __future__ import annotations

import json
from datetime import date, timedelta
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import check_catalog_drift as drift


SCRIPT = Path(__file__).with_name("check_catalog_drift.py")


def catalog(*, source_reviewed_at: str, lifecycle_status: str = "active", pricing_unknown: bool = False) -> dict:
    if pricing_unknown:
        pricing = {
            "status": "unknown",
            "currency": "USD",
            "unit": "1M_tokens",
            "precision": "unavailable",
        }
    else:
        pricing = {
            "currency": "USD",
            "unit": "1M_tokens",
            "input": 1.0,
            "output": 2.0,
            "precision": "official_exact",
        }

    lifecycle = {"status": lifecycle_status}
    if lifecycle_status in {"deprecated", "retired"}:
        lifecycle["replacementId"] = "replacement-model"

    return {
        "schemaVersion": 7,
        "catalogVersion": "test",
        "lastReviewed": date.today().isoformat(),
        "runtimeScraping": False,
        "sequence": 1,
        "hardExpiresAt": (date.today() + timedelta(days=30)).isoformat() + "T00:00:00Z",
        "freshnessSloDays": 30,
        "verificationState": "packaged",
        "estimatesAllowed": True,
        "providers": [
            {
                "key": "test",
                "label": "Test provider",
                "reviewExpiresAt": (date.today() + timedelta(days=30)).isoformat(),
                "models": [
                    {
                        "id": "test-model",
                        "sourceRefs": [
                            {
                                "label": "Test source",
                                "url": "https://example.com/catalog",
                                "reviewedAt": source_reviewed_at,
                            }
                        ],
                        "pricing": pricing,
                        "lifecycle": lifecycle,
                    }
                ],
            }
        ],
    }


class CatalogDriftTests(unittest.TestCase):
    def test_expected_lifecycle_and_unknown_pricing_are_not_actionable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            report = drift.audit(
                self._write_catalog(
                    Path(directory),
                    catalog(
                        source_reviewed_at=date.today().isoformat(),
                        lifecycle_status="retired",
                        pricing_unknown=True,
                    ),
                ),
                network=False,
            )

        self.assertEqual(report["status"], "expected_review")
        self.assertEqual(report["stats"]["actionableReviewItems"], 0)
        self.assertEqual(report["stats"]["expectedReviewItems"], 2)
        self.assertEqual(report["actionableReviewItems"], [])
        self.assertTrue(all(item["actionable"] is False for item in report["reviewItems"]))

    def test_stale_source_is_actionable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            report = drift.audit(
                self._write_catalog(Path(directory), catalog(source_reviewed_at="2000-01-01")),
                network=False,
            )

        self.assertEqual(report["status"], "review_required")
        self.assertEqual(report["stats"]["actionableReviewItems"], 1)
        self.assertEqual(report["actionableReviewItems"][0]["category"], "stale_source")

    def test_transient_network_failures_are_warnings(self) -> None:
        self.assertFalse(drift.network_review_is_actionable("TimeoutError"))
        self.assertFalse(drift.network_review_is_actionable("HTTP 503"))
        self.assertTrue(drift.network_review_is_actionable("HTTP 404"))

    def test_strict_modes_have_separate_exit_contracts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "catalog.json"
            path.write_text(json.dumps(catalog(
                source_reviewed_at=date.today().isoformat(),
                lifecycle_status="retired",
                pricing_unknown=True,
            )), encoding="utf-8")

            strict_actionable = subprocess.run(
                [sys.executable, str(SCRIPT), "--catalog", str(path), "--strict-actionable-review"],
                check=False,
            )
            strict_all_reviews = subprocess.run(
                [sys.executable, str(SCRIPT), "--catalog", str(path), "--strict-review"],
                check=False,
            )

        self.assertEqual(strict_actionable.returncode, 0)
        self.assertEqual(strict_all_reviews.returncode, 2)

    @staticmethod
    def _write_catalog(directory: Path, payload: dict) -> Path:
        path = directory / "catalog.json"
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path


if __name__ == "__main__":
    unittest.main(verbosity=2)
