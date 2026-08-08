#include <QtTest>

#include "scopebreakdownquery.h"

namespace {
QVariantMap metric(double value, const QString &scope, const QString &model = QString(),
    const QString &project = QString(), const QString &source = QStringLiteral("billing_api"),
    const QString &quality = QStringLiteral("actual"))
{
    const QDateTime start(QDate(2026, 7, 1), QTime(0, 0), QTimeZone::UTC);
    return {
        { QStringLiteral("kind"), QStringLiteral("cost") },
        { QStringLiteral("available"), true },
        { QStringLiteral("value"), value },
        { QStringLiteral("unit"), QStringLiteral("USD") },
        { QStringLiteral("currency"), QStringLiteral("USD") },
        { QStringLiteral("source"), source },
        { QStringLiteral("quality"), quality },
        { QStringLiteral("scope"), scope },
        { QStringLiteral("window"), QStringLiteral("month") },
        { QStringLiteral("periodStart"), start },
        { QStringLiteral("periodEnd"), start.addMonths(1) },
        { QStringLiteral("modelScope"), model },
        { QStringLiteral("projectScope"), project },
    };
}
} // namespace

class ScopeBreakdownQueryTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reconcilesAggregateExactlyOnce();
    void keepsCompatibilityClassesSeparate();
    void exposesOnlyReportedDimensions();
    void handlesDuplicateAndAmbiguousRows();
    void createsMinorUnitUnattributedRow();
    void scopedOverAggregateIsUnavailable();
};

void ScopeBreakdownQueryTest::reconcilesAggregateExactlyOnce()
{
    const QVariantMap result = ScopeBreakdownQuery::run({
        metric(10.0, QStringLiteral("organization")),
        metric(6.0, QStringLiteral("organization_scoped"), QStringLiteral("gpt-a"), QStringLiteral("project-a")),
        metric(4.0, QStringLiteral("organization_scoped"), QStringLiteral("gpt-b"), QStringLiteral("project-b")),
    });
    QCOMPARE(result.value(QStringLiteral("aggregateRows")).toList().size(), 1);
    QCOMPARE(result.value(QStringLiteral("scopedRows")).toList().size(), 2);
    const QVariantList reconciliations = result.value(QStringLiteral("reconciliations")).toList();
    QCOMPARE(reconciliations.size(), 1);
    const QVariantMap reconciliation = reconciliations.first().toMap();
    QCOMPARE(reconciliation.value(QStringLiteral("aggregateValue")).toDouble(), 10.0);
    QCOMPARE(reconciliation.value(QStringLiteral("scopedValue")).toDouble(), 10.0);
    QVERIFY(reconciliation.value(QStringLiteral("reconciled")).toBool());
    QCOMPARE(reconciliation.value(QStringLiteral("status")).toString(), QStringLiteral("exact"));
}

void ScopeBreakdownQueryTest::keepsCompatibilityClassesSeparate()
{
    QVariantMap estimated = metric(3.0, QStringLiteral("organization_scoped"), QString(), QStringLiteral("project-e"),
        QStringLiteral("estimated_pricing"), QStringLiteral("estimated"));
    estimated.insert(QStringLiteral("currency"), QStringLiteral("EUR"));
    estimated.insert(QStringLiteral("unit"), QStringLiteral("EUR"));
    const QVariantMap result = ScopeBreakdownQuery::run({
        metric(3.0, QStringLiteral("organization")),
        estimated,
    });
    QVERIFY(result.value(QStringLiteral("reconciliations")).toList().isEmpty());
    const QVariantMap scoped = result.value(QStringLiteral("scopedRows")).toList().first().toMap();
    QCOMPARE(scoped.value(QStringLiteral("valueClass")).toString(), QStringLiteral("estimated"));
    QCOMPARE(scoped.value(QStringLiteral("currency")).toString(), QStringLiteral("EUR"));
}

void ScopeBreakdownQueryTest::exposesOnlyReportedDimensions()
{
    const QVariantMap aggregate = ScopeBreakdownQuery::annotateMetric(metric(1.0, QStringLiteral("organization")));
    QVERIFY(!aggregate.value(QStringLiteral("modelScopeAvailable")).toBool());
    QVERIFY(!aggregate.value(QStringLiteral("projectScopeAvailable")).toBool());
    QVERIFY(!aggregate.value(QStringLiteral("serviceTierAvailable")).toBool());
    QCOMPARE(aggregate.value(QStringLiteral("aggregationLevel")).toString(), QStringLiteral("aggregate"));

    const QVariantMap tier
        = ScopeBreakdownQuery::annotateMetric(metric(1.0, QStringLiteral("organization_scoped:service_tier:priority"),
            QStringLiteral("claude"), QStringLiteral("deleted_workspace")));
    QCOMPARE(tier.value(QStringLiteral("serviceTierScope")).toString(), QStringLiteral("priority"));
    QVERIFY(tier.value(QStringLiteral("serviceTierAvailable")).toBool());
    QCOMPARE(tier.value(QStringLiteral("projectDisplayKind")).toString(), QStringLiteral("deleted"));

    const QVariantMap lineItem
        = ScopeBreakdownQuery::annotateMetric(metric(1.0, QStringLiteral("organization_scoped:line_item:Assistants")));
    QCOMPARE(lineItem.value(QStringLiteral("lineItemScope")).toString(), QStringLiteral("Assistants"));
    QVERIFY(lineItem.value(QStringLiteral("lineItemAvailable")).toBool());

    QVariantMap explicitDimensions
        = metric(1.0, QStringLiteral("organization_scoped"), QStringLiteral("claude"),
            QStringLiteral("workspace-a"));
    explicitDimensions.insert(QStringLiteral("serviceTierScope"), QStringLiteral("standard"));
    explicitDimensions.insert(QStringLiteral("lineItemScope"), QStringLiteral("Messages"));
    const QVariantMap annotated = ScopeBreakdownQuery::annotateMetric(explicitDimensions);
    QCOMPARE(annotated.value(QStringLiteral("serviceTierScope")).toString(), QStringLiteral("standard"));
    QCOMPARE(annotated.value(QStringLiteral("lineItemScope")).toString(), QStringLiteral("Messages"));
    QCOMPARE(annotated.value(QStringLiteral("serviceTierDisplaySuffix")).toString(), QStringLiteral("standard"));
    QCOMPARE(annotated.value(QStringLiteral("lineItemDisplaySuffix")).toString(), QStringLiteral("Messages"));
}

void ScopeBreakdownQueryTest::handlesDuplicateAndAmbiguousRows()
{
    const QVariantMap result = ScopeBreakdownQuery::run({
        metric(5.0, QStringLiteral("organization")),
        metric(5.0, QStringLiteral("account")),
        metric(4.0, QStringLiteral("organization_scoped"), QStringLiteral("gpt-a"), QStringLiteral("project-a")),
        metric(3.0, QStringLiteral("organization_scoped"), QStringLiteral("gpt-a"), QStringLiteral("project-a")),
        metric(3.0, QStringLiteral("organization_scoped"), QStringLiteral("gpt-b"), QStringLiteral("project-b")),
    });
    QCOMPARE(result.value(QStringLiteral("scopedRows")).toList().size(), 2);
    const QVariantMap reconciliation = result.value(QStringLiteral("reconciliations")).toList().first().toMap();
    QCOMPARE(reconciliation.value(QStringLiteral("aggregateValue")).toDouble(), 10.0);
    QCOMPARE(reconciliation.value(QStringLiteral("scopedValue")).toDouble(), 10.0);
    QCOMPARE(reconciliation.value(QStringLiteral("status")).toString(), QStringLiteral("ambiguous_aggregate"));
    QVERIFY(!reconciliation.value(QStringLiteral("reconciled")).toBool());
}

void ScopeBreakdownQueryTest::createsMinorUnitUnattributedRow()
{
    const QVariantMap result = ScopeBreakdownQuery::run({
        metric(10.004, QStringLiteral("organization")),
        metric(6.0, QStringLiteral("organization_scoped"), QString(), QStringLiteral("project-a")),
        metric(3.99, QStringLiteral("organization_scoped"), QString(), QStringLiteral("project-b")),
    });
    const QVariantMap reconciliation = result.value(QStringLiteral("reconciliations")).toList().first().toMap();
    QCOMPARE(reconciliation.value(QStringLiteral("status")).toString(), QStringLiteral("unattributed"));
    QCOMPARE(reconciliation.value(QStringLiteral("unattributedMinor")).toLongLong(), 1);
    QVERIFY(reconciliation.value(QStringLiteral("reconciled")).toBool());
    const QVariantList scoped = result.value(QStringLiteral("scopedRows")).toList();
    QCOMPARE(scoped.size(), 3);
    const QVariantMap unattributed = scoped.last().toMap();
    QVERIFY(unattributed.value(QStringLiteral("isUnattributed")).toBool());
    QCOMPARE(unattributed.value(QStringLiteral("displayLabel")).toString(), QStringLiteral("Unattributed"));
    QCOMPARE(unattributed.value(QStringLiteral("valueMinor")).toLongLong(), 1);

    const QVariantMap roundedExact = ScopeBreakdownQuery::run({
        metric(10.004, QStringLiteral("organization")),
        metric(10.0, QStringLiteral("organization_scoped"), QString(), QStringLiteral("project-a")),
    });
    QCOMPARE(roundedExact.value(QStringLiteral("reconciliations")).toList().first().toMap()
                 .value(QStringLiteral("status")).toString(),
        QStringLiteral("exact"));
}

void ScopeBreakdownQueryTest::scopedOverAggregateIsUnavailable()
{
    const QVariantMap result = ScopeBreakdownQuery::run({
        metric(10.0, QStringLiteral("organization")),
        metric(10.01, QStringLiteral("organization_scoped"), QString(), QStringLiteral("project-a")),
    });
    const QVariantMap reconciliation = result.value(QStringLiteral("reconciliations")).toList().first().toMap();
    QCOMPARE(reconciliation.value(QStringLiteral("status")).toString(), QStringLiteral("mismatch"));
    QCOMPARE(reconciliation.value(QStringLiteral("reasonKey")).toString(), QStringLiteral("scope-mismatch"));
    QVERIFY(!reconciliation.value(QStringLiteral("available")).toBool());
    QCOMPARE(result.value(QStringLiteral("scopedRows")).toList().size(), 1);
}

QTEST_MAIN(ScopeBreakdownQueryTest)
#include "test_scopebreakdownquery.moc"
