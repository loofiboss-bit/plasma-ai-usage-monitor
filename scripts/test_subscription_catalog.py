#!/usr/bin/env python3
"""Deterministic evidence contract tests; no wall-clock freshness assumptions."""
import unittest
from datetime import date
import check_subscription_catalog as catalog
from check_subscription_drift import report

class EvidenceTests(unittest.TestCase):
    def evidence(self):
        return {'reviewedAt': '2026-09-05', 'effectiveFrom': '2026-09-01', 'expiresAt': '2026-10-05', 'sourceRefs': [{'label': 'Official', 'url': 'https://example.com/pricing', 'reviewedAt': '2026-09-05'}]}

    def test_inclusive_expiry(self):
        catalog.check_evidence(self.evidence(), 'test', date(2026, 10, 5))
        with self.assertRaises(SystemExit):
            catalog.check_evidence(self.evidence(), 'test', date(2026, 10, 6))

    def test_recent_header_cannot_certify_missing_evidence(self):
        with self.assertRaises(SystemExit):
            catalog.check_evidence({}, 'test', date(2026, 9, 7))

    def test_malformed_range(self):
        for price in ({'rangeMin': -1, 'rangeMax': 20}, {'rangeMin': 21, 'rangeMax': 20}, {'rangeMin': 10}, {'amount': True}, {'amount': float('nan')}):
            with self.subTest(price=price), self.assertRaises(SystemExit):
                catalog.check_price(dict(price, precision='official_range', currency='USD', period='month'), 'test')

    def test_structural_dates_ignore_wall_clock(self):
        catalog.check_evidence(self.evidence(), 'test', None)
        bad = self.evidence(); bad['expiresAt'] = '2026-09-01'
        with self.assertRaises(SystemExit): catalog.check_evidence(bad, 'test', None)

    def test_drift_report_has_actionable_reason_and_reference(self):
        fixture = {'tools': [{'key': 'example', 'plans': [{'id': 'pro', 'price': {'amount': 10, 'evidence': self.evidence()}}]}]}
        text, actionable = report(fixture, date(2026, 10, 6))
        self.assertTrue(actionable)
        self.assertIn('example / pro / Price', text)
        self.assertIn('evidence not current', text)
        self.assertIn('https://example.com/pricing', text)
        self.assertFalse(report(fixture, date(2026, 10, 5))[1])

    def test_manual_review_with_current_evidence_is_not_actionable(self):
        fixture = {
            'tools': [{
                'key': 'example',
                'reviewReason': 'Vendor requires authenticated account verification.',
                'plans': [{
                    'id': 'free',
                    'price': {
                        'amount': 0,
                        'currency': 'USD',
                        'period': 'month',
                        'precision': 'official_exact',
                        'evidence': dict(self.evidence(), needsManualReview=True),
                    },
                }],
            }],
        }
        text, actionable = report(fixture, date(2026, 9, 10))
        self.assertFalse(actionable)
        self.assertIn('example / free / Price', text)
        self.assertIn('Vendor requires authenticated account verification', text)

        text_expired, actionable_expired = report(fixture, date(2026, 10, 6))
        self.assertTrue(actionable_expired)
        self.assertIn('evidence not current', text_expired)

    def test_source_review_cannot_be_renewed_by_entry(self):
        evidence = self.evidence()
        evidence['sourceRefs'][0]['reviewedAt'] = '2026-07-01'
        with self.assertRaises(SystemExit):
            catalog.check_evidence(evidence, 'test', date(2026, 9, 7))

if __name__ == '__main__':
    unittest.main()
