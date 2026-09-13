#!/usr/bin/env python3
"""Report per-entry subscription review work without runtime web scraping."""
import argparse
import contextlib
import io
import json
from datetime import date
from pathlib import Path
from check_subscription_catalog import CATALOG, check_evidence, check_numbers


def report(catalog, today):
    rows = []
    for tool in catalog.get('tools', []):
        for group in ('plans', 'billingModes'):
            for plan in tool.get(group, []):
                entries = ([dict(plan['price'], label='Price')] if 'price' in plan else []) + plan.get('quotaWindows', [])
                for entry in entries:
                    evidence = entry.get('evidence', {})
                    if evidence.get('archived') and evidence.get('needsManualReview'):
                        continue
                    actionable_reason = ''
                    error = io.StringIO()
                    with contextlib.redirect_stderr(error):
                        try:
                            check_evidence(evidence, f"{tool['key']}/{plan['id']}", today)
                            check_numbers(entry, f"{tool['key']}/{plan['id']}")
                        except SystemExit:
                            actionable_reason = error.getvalue().strip()
                    is_actionable = bool(actionable_reason)
                    reason = actionable_reason
                    if not reason and (evidence.get('needsManualReview') or entry.get('precision') == 'needs_manual_review'):
                        reason = tool.get('reviewReason', 'Authenticated account evidence requires review')
                    if reason:
                        refs = evidence.get('sourceRefs') or tool.get('sourceRefs', [])
                        rows.append((tool['key'], plan['id'], entry.get('label', 'Allowance'), reason, refs, is_actionable))
    has_actionable = any(item[5] for item in rows)
    lines = [f'# Subscription evidence review — {today}', '']
    if has_actionable:
        lines += ['Actionable subscription evidence drift requires maintainer attention.', '']
    else:
        lines += ['Public catalog evidence does not establish account entitlement.', '']
    for tool, plan, label, reason, refs, is_actionable in rows:
        lines += [f'## {tool} / {plan} / {label}', '', reason, '']
        lines += [f"- [{ref['label']}]({ref['url']})" for ref in refs]
        lines += ['']
    if not rows:
        lines += ['No actionable subscription evidence drift.']
    return '\n'.join(lines) + '\n', has_actionable


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    parser.add_argument('--as-of', type=date.fromisoformat, default=date.today())
    parser.add_argument('--strict-review', action='store_true',
                        help='return 1 when any maintainer review is needed')
    parser.add_argument('--strict-actionable-review', action='store_true',
                        help='return 1 only when an actionable maintainer review is needed')
    args = parser.parse_args()
    content, actionable = report(json.loads(CATALOG.read_text()), args.as_of)
    if args.output:
        args.output.write_text(content)
    else:
        print(content, end='')
    if args.strict_review:
        has_any_reviews = "No actionable subscription evidence drift." not in content
        raise SystemExit(1 if has_any_reviews else 0)
    raise SystemExit(1 if actionable else 0)

if __name__ == '__main__':
    main()
