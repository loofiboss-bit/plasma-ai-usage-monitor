#include <QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

#include "usagedatabase.h"

namespace {
QString databasePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
}

QVariantMap
metric(const QString &kind, const QString &unit, const QVariant &value,
       bool available, const QString &currency = {},
       const QString &quality = QStringLiteral("actual"),
       const QString &source = QStringLiteral("actual_api"),
       const QString &window = QStringLiteral("current"),
       const QDateTime &observedAt = QDateTime::currentDateTimeUtc())
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("value"), value},
        {QStringLiteral("available"), available},
        {QStringLiteral("currency"), currency},
        {QStringLiteral("quality"), quality},
        {QStringLiteral("source"), source},
        {QStringLiteral("scope"), QStringLiteral("api_key")},
        {QStringLiteral("window"), window},
        {QStringLiteral("observedAt"), observedAt},
    };
}

QVariantMap source(const QString &kind, const QString &name,
                   bool historyOnly = false)
{
    return {
        {QStringLiteral("historyId"), kind + QLatin1Char(':') + name},
        {QStringLiteral("sourceKind"), kind},
        {QStringLiteral("dbName"), name},
        {QStringLiteral("displayName"), name},
        {QStringLiteral("historyOnly"), historyOnly},
    };
}

QVariantMap seriesResult(UsageDatabase &db, const QVariantList &sources,
                         const QString &metricKind, const QDateTime &from,
                         const QDateTime &to, int bucketMinutes = 60)
{
    return db.getHistorySeries(sources, from, to, metricKind, bucketMinutes);
}

bool withDatabase(const std::function<bool(QSqlDatabase &)> &operation)
{
    const QString connectionName = QStringLiteral("history_contract_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath());
        if (database.open()) {
            ok = operation(database);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}
} // namespace

class HistoryContractTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void retainedCatalogIncludesProviderToolAndUnknownSource();
    void availableZeroDoesNotBecomeUnavailable();
    void gapsAndSmallSeriesArePreserved();
    void gaugesAreNotSummedAsIntervalTotals();
    void mixedCurrencyComparisonFailsClosed();
    void incompatibleSemanticComparisonFailsClosed();
    void providerRateLimitRequiresCompatiblePair();
    void rollingToolQuotaUsesGaugeSemantics();
    void olderAsyncRequestIsSuperseded();
    void hundredThousandObservationsStayBounded();
};

void HistoryContractTest::
    retainedCatalogIncludesProviderToolAndUnknownSource()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Disabled Provider"),
        {metric(QStringLiteral("cost"), QStringLiteral("USD"), 2.0, true,
                QStringLiteral("USD"))}));
    QVERIFY(
        db.recordProviderMetrics(QStringLiteral("Unknown Retained Source"),
                                 {metric(QStringLiteral("requests"),
                                         QStringLiteral("request"), 4, true)}));
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Connectivity Provider"),
        {metric(QStringLiteral("requests"), QStringLiteral("request"), 0, true,
                {}, QStringLiteral("connectivity_only"),
                QStringLiteral("connectivity_probe"))}));
    db.recordToolSnapshot(QStringLiteral("Disabled Tool"), 3, 10,
                          QStringLiteral("5-hour"), QStringLiteral("Pro"), false);

    const QVariantList catalog = db.getHistoryCatalog();
    QHash<QString, QVariantMap> byId;
    for (const QVariant &entry : catalog) {
        const QVariantMap row = entry.toMap();
        byId.insert(row.value(QStringLiteral("historyId")).toString(), row);
    }

    QVERIFY(byId.contains(QStringLiteral("provider:Disabled Provider")));
    QVERIFY(byId.contains(QStringLiteral("provider:Unknown Retained Source")));
    QVERIFY(byId.contains(QStringLiteral("provider:Connectivity Provider")));
    QVERIFY(byId.contains(QStringLiteral("tool:Disabled Tool")));
    QVERIFY(byId.value(QStringLiteral("provider:Disabled Provider"))
                .value(QStringLiteral("metricKinds"))
                .toStringList()
                .contains(QStringLiteral("cost")));
    QVERIFY(byId.value(QStringLiteral("provider:Connectivity Provider"))
                .value(QStringLiteral("metricKinds"))
                .toStringList()
                .isEmpty());
    const QStringList toolMetrics = byId.value(QStringLiteral("tool:Disabled Tool"))
                                        .value(QStringLiteral("metricKinds"))
                                        .toStringList();
    QVERIFY(toolMetrics.contains(QStringLiteral("usageCount")));
    QVERIFY(toolMetrics.contains(QStringLiteral("remaining")));
}

void HistoryContractTest::availableZeroDoesNotBecomeUnavailable()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Zero Provider"),
        {
            metric(QStringLiteral("cost"), QStringLiteral("USD"), 99.0, false,
                   QStringLiteral("USD"), QStringLiteral("unavailable"),
                   QStringLiteral("actual_api"), QStringLiteral("current"),
                   now.addSecs(-60)),
            metric(QStringLiteral("cost"), QStringLiteral("USD"), 0.0, true,
                   QStringLiteral("USD"), QStringLiteral("actual"),
                   QStringLiteral("actual_api"), QStringLiteral("current"), now),
        }));

    const QVariantMap payload = seriesResult(
        db, {source(QStringLiteral("provider"), QStringLiteral("Zero Provider"))},
        QStringLiteral("cost"), now.addSecs(-3600), now.addSecs(60));
    QVERIFY(payload.value(QStringLiteral("ok")).toBool());
    const QVariantList series = payload.value(QStringLiteral("series")).toList();
    QCOMPARE(series.size(), 1);
    const QVariantList points = series.first().toMap().value(QStringLiteral("points")).toList();
    QCOMPARE(points.size(), 1);
    QCOMPARE(points.first().toMap().value(QStringLiteral("available")).toBool(),
             true);
    QCOMPARE(points.first().toMap().value(QStringLiteral("value")).toDouble(),
             0.0);
}

void HistoryContractTest::gapsAndSmallSeriesArePreserved()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime start = QDateTime::fromString(
        QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Gap Provider"),
        {metric(QStringLiteral("requests"), QStringLiteral("request"), 1, true,
                {}, QStringLiteral("actual"), QStringLiteral("actual_api"),
                QStringLiteral("current"), start)}));
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Gap Provider"),
        {metric(QStringLiteral("requests"), QStringLiteral("request"), 2, true,
                {}, QStringLiteral("actual"), QStringLiteral("actual_api"),
                QStringLiteral("current"), start.addSecs(3 * 3600))}));

    const QVariantMap payload = seriesResult(
        db, {source(QStringLiteral("provider"), QStringLiteral("Gap Provider"))},
        QStringLiteral("requests"), start.addSecs(-1), start.addSecs(4 * 3600),
        60);
    const QVariantMap row = payload.value(QStringLiteral("series")).toList().first().toMap();
    QCOMPARE(row.value(QStringLiteral("sampleCount")).toInt(), 2);
    QCOMPARE(row.value(QStringLiteral("availablePointCount")).toInt(), 2);
    QVERIFY(row.value(QStringLiteral("containsGaps")).toBool());
    const QVariantList points = row.value(QStringLiteral("points")).toList();
    QCOMPARE(points.size(), 3);
    QCOMPARE(points.at(1).toMap().value(QStringLiteral("available")).toBool(),
             false);
    QVERIFY(!points.at(1).toMap().value(QStringLiteral("value")).isValid());
}

void HistoryContractTest::gaugesAreNotSummedAsIntervalTotals()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime start =
        QDateTime::fromString(QStringLiteral("2026-01-01T00:00:00Z"),
                              Qt::ISODate);
    QVariantMap firstGauge = metric(QStringLiteral("requests"),
                                    QStringLiteral("request"), 10, true, {},
                                    QStringLiteral("actual"),
                                    QStringLiteral("actual_api"),
                                    QStringLiteral("current"), start);
    QVariantMap secondGauge = metric(QStringLiteral("requests"),
                                     QStringLiteral("request"), 12, true, {},
                                     QStringLiteral("actual"),
                                     QStringLiteral("actual_api"),
                                     QStringLiteral("current"),
                                     start.addSecs(60));
    QVariantMap firstInterval = metric(QStringLiteral("requests"),
                                       QStringLiteral("request"), 2, true, {},
                                       QStringLiteral("actual"),
                                       QStringLiteral("actual_api"),
                                       QStringLiteral("current"),
                                       start.addSecs(120));
    QVariantMap secondInterval = metric(QStringLiteral("requests"),
                                        QStringLiteral("request"), 3, true, {},
                                        QStringLiteral("actual"),
                                        QStringLiteral("actual_api"),
                                        QStringLiteral("current"),
                                        start.addSecs(180));
    firstGauge.insert(QStringLiteral("semantic"), QStringLiteral("gauge"));
    secondGauge.insert(QStringLiteral("semantic"), QStringLiteral("gauge"));
    firstInterval.insert(QStringLiteral("semantic"),
                         QStringLiteral("interval_total"));
    secondInterval.insert(QStringLiteral("semantic"),
                          QStringLiteral("interval_total"));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Semantic Provider"),
                                     {firstGauge}));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Semantic Provider"),
                                     {secondGauge}));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Semantic Provider"),
                                     {firstInterval}));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Semantic Provider"),
                                     {secondInterval}));

    const QVariantMap payload = seriesResult(
        db,
        {source(QStringLiteral("provider"),
                QStringLiteral("Semantic Provider"))},
        QStringLiteral("requests"), start.addSecs(-1), start.addSecs(3600), 60);
    QVERIFY(payload.value(QStringLiteral("ok")).toBool());
    const QVariantList series = payload.value(QStringLiteral("series")).toList();
    QCOMPARE(series.size(), 2);

    QHash<QString, QVariantMap> bySemantic;
    for (const QVariant &entry : series) {
        const QVariantMap row = entry.toMap();
        bySemantic.insert(row.value(QStringLiteral("semantic")).toString(), row);
    }
    QCOMPARE(bySemantic.value(QStringLiteral("gauge"))
                 .value(QStringLiteral("points"))
                 .toList()
                 .last()
                 .toMap()
                 .value(QStringLiteral("value"))
                 .toDouble(),
             12.0);
    QCOMPARE(bySemantic.value(QStringLiteral("interval_total"))
                 .value(QStringLiteral("points"))
                 .toList()
                 .first()
                 .toMap()
                 .value(QStringLiteral("value"))
                 .toDouble(),
             5.0);
}

void HistoryContractTest::mixedCurrencyComparisonFailsClosed()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("US Provider"),
        {metric(QStringLiteral("cost"), QStringLiteral("USD"), 1.0, true,
                QStringLiteral("USD"))}));
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("EU Provider"),
        {metric(QStringLiteral("cost"), QStringLiteral("EUR"), 1.0, true,
                QStringLiteral("EUR"))}));

    const QVariantMap payload = seriesResult(
        db,
        {source(QStringLiteral("provider"), QStringLiteral("US Provider")),
         source(QStringLiteral("provider"), QStringLiteral("EU Provider"))},
        QStringLiteral("cost"), now.addSecs(-3600), now.addSecs(60));
    QVERIFY(!payload.value(QStringLiteral("ok")).toBool());
    QCOMPARE(payload.value(QStringLiteral("errorKey")).toString(),
             QStringLiteral("mixed_currencies"));
}

void HistoryContractTest::incompatibleSemanticComparisonFailsClosed()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Actual Provider"),
        {metric(QStringLiteral("cost"), QStringLiteral("USD"), 1.0, true,
                QStringLiteral("USD"))}));
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Estimated Provider"),
        {metric(QStringLiteral("cost"), QStringLiteral("USD"), 1.0, true,
                QStringLiteral("USD"), QStringLiteral("estimated"),
                QStringLiteral("estimated_pricing"))}));

    const QVariantMap payload = seriesResult(
        db,
        {source(QStringLiteral("provider"), QStringLiteral("Actual Provider")),
         source(QStringLiteral("provider"),
                QStringLiteral("Estimated Provider"))},
        QStringLiteral("cost"), now.addSecs(-3600), now.addSecs(60));
    QVERIFY(!payload.value(QStringLiteral("ok")).toBool());
    QCOMPARE(payload.value(QStringLiteral("errorKey")).toString(),
             QStringLiteral("incompatible_semantics"));
}

void HistoryContractTest::providerRateLimitRequiresCompatiblePair()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVariantMap limit = metric(QStringLiteral("request_limit"),
                               QStringLiteral("request"), 100, true);
    QVariantMap remaining = metric(QStringLiteral("request_remaining"),
                                   QStringLiteral("request"), 25, true);
    limit.insert(QStringLiteral("resetAt"), now.addSecs(3600));
    remaining.insert(QStringLiteral("resetAt"), now.addSecs(3600));
    limit.insert(QStringLiteral("source"), QStringLiteral("documented_limit"));
    remaining.insert(QStringLiteral("source"),
                     QStringLiteral("response_headers"));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Quota Provider"),
                                     {limit, remaining}));

    QVariantList catalog = db.getHistoryCatalog();
    bool advertised = false;
    for (const QVariant &entry : catalog) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("historyId")).toString()
            != QLatin1String("provider:Quota Provider")) {
            continue;
        }
        advertised = row.value(QStringLiteral("metricKinds"))
                         .toStringList()
                         .contains(QStringLiteral("rateLimitUsed"));
    }
    QVERIFY(!advertised);

    limit.insert(QStringLiteral("source"), QStringLiteral("response_headers"));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Quota Provider"),
                                     {limit, remaining}));

    catalog = db.getHistoryCatalog();
    advertised = false;
    for (const QVariant &entry : catalog) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("historyId")).toString()
            != QLatin1String("provider:Quota Provider")) {
            continue;
        }
        advertised = row.value(QStringLiteral("metricKinds"))
                         .toStringList()
                         .contains(QStringLiteral("rateLimitUsed"));
    }
    QVERIFY(advertised);

    const QVariantMap payload = seriesResult(
        db,
        {source(QStringLiteral("provider"), QStringLiteral("Quota Provider"))},
        QStringLiteral("rateLimitUsed"), now.addSecs(-60), now.addSecs(60));
    QVERIFY(payload.value(QStringLiteral("ok")).toBool());
    const QVariantMap row =
        payload.value(QStringLiteral("series")).toList().first().toMap();
    QCOMPARE(row.value(QStringLiteral("semantic")).toString(),
             QStringLiteral("rolling_gauge"));
    QCOMPARE(row.value(QStringLiteral("unit")).toString(),
             QStringLiteral("percent"));
    QCOMPARE(row.value(QStringLiteral("points"))
                 .toList()
                 .first()
                 .toMap()
                 .value(QStringLiteral("value"))
                 .toDouble(),
             75.0);
    QVERIFY(row.value(QStringLiteral("resetAt")).toDateTime().isValid());
}

void HistoryContractTest::rollingToolQuotaUsesGaugeSemantics()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    db.recordToolSnapshot(QStringLiteral("Codex CLI"), 30, 100,
                          QStringLiteral("5-hour"), QStringLiteral("Pro"), false);
    db.recordToolSnapshot(QStringLiteral("Codex CLI"), 80, 100,
                          QStringLiteral("5-hour"), QStringLiteral("Pro"), false);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QVariantMap payload = seriesResult(
        db, {source(QStringLiteral("tool"), QStringLiteral("Codex CLI"), true)},
        QStringLiteral("remaining"), now.addSecs(-3600), now.addSecs(60));
    QVERIFY(payload.value(QStringLiteral("ok")).toBool());
    const QVariantMap row = payload.value(QStringLiteral("series")).toList().first().toMap();
    QCOMPARE(row.value(QStringLiteral("semantic")).toString(),
             QStringLiteral("rolling_gauge"));
    QCOMPARE(row.value(QStringLiteral("window")).toString(),
             QStringLiteral("5-hour"));
    QVERIFY(row.value(QStringLiteral("historyOnly")).toBool());
    QCOMPARE(row.value(QStringLiteral("availablePointCount")).toInt(), 1);
    const QVariantMap point = row.value(QStringLiteral("points")).toList().first().toMap();
    QCOMPARE(point.value(QStringLiteral("value")).toDouble(), 20.0);
}

void HistoryContractTest::olderAsyncRequestIsSuperseded()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    QVERIFY(
        db.recordProviderMetrics(QStringLiteral("Async Provider"),
                                 {metric(QStringLiteral("requests"),
                                         QStringLiteral("request"), 1, true)}));

    QSignalSpy spy(&db, &UsageDatabase::historySeriesReady);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QVariantList sources{
        source(QStringLiteral("provider"), QStringLiteral("Async Provider"))};
    db.requestHistorySeries(QStringLiteral("old"), sources, now.addDays(-1), now,
                            QStringLiteral("requests"), 60);
    db.requestHistorySeries(QStringLiteral("new"), sources, now.addDays(-1), now,
                            QStringLiteral("requests"), 60);

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("new"));
}

void HistoryContractTest::hundredThousandObservationsStayBounded()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime start = QDateTime::fromString(
        QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);
    QVERIFY(withDatabase([&](QSqlDatabase &database) {
        if (!database.transaction())
            return false;
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO observations("
            "provider,observed_at_utc,metric_kind,unit,value,semantic,source,"
            "data_quality,scope,window,correlation_id) "
            "VALUES('Scale Provider',?,'requests','request',?,'gauge',"
            "'actual_api','actual','api_key','current',?)"));
        for (int i = 0; i < 100000; ++i) {
            query.bindValue(0, start.addSecs(i * 60));
            query.bindValue(1, i % 100);
            query.bindValue(2, QString::number(i));
            if (!query.exec()) {
                database.rollback();
                return false;
            }
        }
        return database.commit();
    }));

    QElapsedTimer timer;
    timer.start();
    const QVariantMap payload = seriesResult(
        db,
        {source(QStringLiteral("provider"), QStringLiteral("Scale Provider"))},
        QStringLiteral("requests"), start.addSecs(-1), start.addSecs(100000 * 60),
        1);
    QVERIFY(payload.value(QStringLiteral("ok")).toBool());
    const QVariantMap row = payload.value(QStringLiteral("series")).toList().first().toMap();
    QCOMPARE(row.value(QStringLiteral("sampleCount")).toInt(), 100000);
    QVERIFY(row.value(QStringLiteral("availablePointCount")).toInt() <= 240);
    // Debug-build guard against unbounded query growth. The public contract
    // keeps this work off the UI thread and caps returned chart points; the
    // release-build latency budget belongs to the dedicated Phase 7 gate.
    QVERIFY2(
        timer.elapsed() < 20000,
        qPrintable(QStringLiteral("100k query took %1 ms").arg(timer.elapsed())));
}

QTEST_MAIN(HistoryContractTest)
#include "test_history_contract.moc"
