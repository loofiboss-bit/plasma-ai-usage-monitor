#include <QtTest>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>

#include "usagedatabase.h"

namespace {
QString dbFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
}

bool updateSnapshotData(const QString &provider, double dailyCost, qint64 input, qint64 output, const QString &timestamp)
{
    const QString connName = QStringLiteral("analyst_test_conn_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbFilePath());
        if (db.open()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "UPDATE usage_snapshots SET timestamp = ? "
                "WHERE provider = ? AND daily_cost = ? AND input_tokens = ? AND output_tokens = ?"
            ));
            query.addBindValue(timestamp);
            query.addBindValue(provider);
            query.addBindValue(dailyCost);
            query.addBindValue(input);
            query.addBindValue(output);
            ok = query.exec() && query.numRowsAffected() > 0;
            QSqlQuery observation(db);
            observation.prepare(QStringLiteral(
                "UPDATE observations SET observed_at_utc=?, value=?, semantic='interval_total' "
                "WHERE id=(SELECT MAX(id) FROM observations WHERE provider=? AND metric_kind='cost')"));
            observation.addBindValue(timestamp);
            observation.addBindValue(dailyCost);
            observation.addBindValue(provider);
            ok = ok && observation.exec() && observation.numRowsAffected() > 0;
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok;
}

QVariantMap metric(const QString &kind, const QString &unit, const QVariant &value, const QDateTime &observedAt,
                   const QString &currency = {}, const QString &quality = QStringLiteral("actual"),
                   const QString &source = QStringLiteral("billing_api"),
                   const QString &semantic = QStringLiteral("interval_total"))
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("value"), value},
        {QStringLiteral("available"), value.isValid()},
        {QStringLiteral("currency"), currency},
        {QStringLiteral("quality"), quality},
        {QStringLiteral("source"), source},
        {QStringLiteral("semantic"), semantic},
        {QStringLiteral("scope"), QStringLiteral("api_key")},
        {QStringLiteral("window"), QStringLiteral("calendar_day")},
        {QStringLiteral("observedAt"), observedAt},
    };
}

QVariantMap kpi(const QVariantMap &snapshot, const QString &name)
{
    return snapshot.value(QStringLiteral("kpis")).toMap().value(name).toMap();
}

bool updateLatestToolTimestamp(const QString &toolName, const QDateTime &timestamp)
{
    const QString connectionName =
        QStringLiteral("analyst_tool_test_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbFilePath());
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral("UPDATE subscription_tool_usage SET timestamp=? "
                                         "WHERE id=(SELECT MAX(id) FROM subscription_tool_usage "
                                         "WHERE tool_name=?)"));
            query.addBindValue(timestamp.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
            query.addBindValue(toolName);
            ok = query.exec() && query.numRowsAffected() == 1;
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}
} // namespace

class UsageDatabaseAnalystTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testYearlyActivity();
    void testEfficiencySeries();
    void testEmptyEfficiencySeries();
    void testAnalystOverview();
    void testAnalystSnapshotNoData();
    void testTypedDailyWindowIsAnalyzable();
    void testAvailableZeroNeedsEnoughCoverage();
    void testCompleteComparisonSeparatesActualAndEstimated();
    void testMixedCurrenciesPreserveActivity();
    void testToolActivityWithoutProviderCost();
    void testExactRangeIsolation();
    void testAsyncRequestSupersession();
    void testHundredThousandObservations();
};

void UsageDatabaseAnalystTest::testYearlyActivity()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    QDateTime now = QDateTime::currentDateTimeUtc();
    QString today = now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QString yesterday = now.addDays(-1).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QString longAgo = now.addDays(-400).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    // Seed data
    db.recordSnapshot("openai", 100, 200, 1, 0.01, 1.5, 30.0, 0, 0, 0, 0); // Today
    QVERIFY(updateSnapshotData("openai", 1.5, 100, 200, today));

    db.recordSnapshot("anthropic", 50, 150, 1, 0.02, 2.5, 40.0, 0, 0, 0, 0); // Yesterday
    QVERIFY(updateSnapshotData("anthropic", 2.5, 50, 150, yesterday));

    db.recordSnapshot("openai", 1000, 2000, 1, 0.1, 10.0, 100.0, 0, 0, 0, 0); // Too old
    QVERIFY(updateSnapshotData("openai", 10.0, 1000, 2000, longAgo));

    // Test Mode 0: Cost
    QVariantMap costActivity = db.getYearlyActivity(0);
    QVERIFY(costActivity.contains("maxIntensity"));
    QVERIFY(costActivity.contains("days"));
    QVariantList costDays = costActivity["days"].toList();
    
    // We expect daily_cost to be aggregated per day.
    // Today: 1.5
    // Yesterday: 2.5
    // Total maxIntensity should be 2.5 (if multiple snapshots per day, they sum, but here 1 per day)
    QCOMPARE(costActivity["maxIntensity"].toDouble(), 2.5);
    
    bool foundToday = false;
    bool foundYesterday = false;
    for (const QVariant &v : costDays) {
        QVariantMap day = v.toMap();
        if (day["date"].toString() == now.toString("yyyy-MM-dd")) {
            QCOMPARE(day["value"].toDouble(), 1.5);
            foundToday = true;
        } else if (day["date"].toString() == now.addDays(-1).toString("yyyy-MM-dd")) {
            QCOMPARE(day["value"].toDouble(), 2.5);
            foundYesterday = true;
        }
    }
    QVERIFY(foundToday);
    QVERIFY(foundYesterday);

    // Test Mode 1: Tokens
    QVariantMap tokenActivity = db.getYearlyActivity(1);
    // Today: 100+200 = 300
    // Yesterday: 50+150 = 200
    QCOMPARE(tokenActivity["maxIntensity"].toDouble(), 300.0);
}

void UsageDatabaseAnalystTest::testEfficiencySeries()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    QDateTime now = QDateTime::currentDateTimeUtc();
    
    // Day 0: 100 in, 200 out -> efficiency 2.0
    db.recordSnapshot("p1", 100, 200, 1, 0.1, 0, 0, 0, 0, 0, 0);
    QVERIFY(updateSnapshotData("p1", 0, 100, 200, now.toString("yyyy-MM-dd HH:mm:ss")));

    // Day 1: 100 in, 50 out -> efficiency 0.5
    db.recordSnapshot("p1", 100, 50, 1, 0.2, 0, 0, 0, 0, 0, 0);
    QVERIFY(updateSnapshotData("p1", 0, 100, 50, now.addDays(-1).toString("yyyy-MM-dd HH:mm:ss")));

    // Day 2: output without input is incompatible and must be omitted.
    db.recordSnapshot("p1", 0, 50, 1, 0.3, 0, 0, 0, 0, 0, 0);
    QVERIFY(updateSnapshotData("p1", 0, 0, 50, now.addDays(-2).toString("yyyy-MM-dd HH:mm:ss")));

    // Day 3: explicit zero output with positive input is a real ratio of zero.
    db.recordSnapshot("p1", 50, 0, 1, 0.4, 0, 0, 0, 0, 0, 0);
    QVERIFY(updateSnapshotData("p1", 0, 50, 0, now.addDays(-3).toString("yyyy-MM-dd HH:mm:ss")));

    QVariantList series = db.getEfficiencySeries(7);
    QCOMPARE(series.size(), 3);

    // Check values (order might depend on implementation, usually ASC date or DESC date)
    // Let's assume we sort by date.
    
    QMap<QString, double> values;
    for (const QVariant &v : series) {
        QVariantMap m = v.toMap();
        values[m["date"].toString()] = m["value"].toDouble();
    }

    QCOMPARE(values[now.toString("yyyy-MM-dd")], 2.0);
    QCOMPARE(values[now.addDays(-1).toString("yyyy-MM-dd")], 0.5);
    QVERIFY(!values.contains(now.addDays(-2).toString("yyyy-MM-dd")));
    QCOMPARE(values[now.addDays(-3).toString("yyyy-MM-dd")], 0.0);
}

void UsageDatabaseAnalystTest::testEmptyEfficiencySeries()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    QVERIFY(db.getEfficiencySeries(30).isEmpty());
}

void UsageDatabaseAnalystTest::testAnalystOverview()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (int dayOffset = 0; dayOffset < 14; ++dayOffset) {
        const bool recentWeek = dayOffset < 7;
        double openAiDaily = recentWeek ? 3.0 : 1.0;
        if (dayOffset == 2) {
            openAiDaily = 8.0; // intentional anomaly
        }

        const double openAiMarkerCost = openAiDaily + (dayOffset * 0.001);
        db.recordSnapshot(QStringLiteral("OpenAI"),
                          100 + dayOffset,
                          180 + dayOffset,
                          5,
                          openAiMarkerCost,
                          openAiDaily,
                          48.0,
                          100,
                          80,
                          1000,
                          700,
                          QStringLiteral("gpt-5.4"),
                          false,
                          QStringLiteral("billing_api"),
                          QStringLiteral("actual_api"),
                          QStringLiteral("USD"),
                          QStringLiteral("actual_billing"));
        QVERIFY(updateSnapshotData(QStringLiteral("OpenAI"),
                                   openAiDaily,
                                   100 + dayOffset,
                                   180 + dayOffset,
                                   now.addDays(-dayOffset).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

        const double anthropicMarkerCost = 0.6 + (dayOffset * 0.001);
        db.recordSnapshot(QStringLiteral("Anthropic"),
                          70 + dayOffset,
                          90 + dayOffset,
                          3,
                          anthropicMarkerCost,
                          0.6,
                          0.0,
                          50,
                          45,
                          500,
                          420,
                          QStringLiteral("claude-3-5-sonnet"),
                          true,
                          QStringLiteral("estimated_from_usage"),
                          QStringLiteral("actual_api"),
                          QStringLiteral("USD"),
                          QStringLiteral("estimated"));
        QVERIFY(updateSnapshotData(QStringLiteral("Anthropic"),
                                   0.6,
                                   70 + dayOffset,
                                   90 + dayOffset,
                                   now.addDays(-dayOffset).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }

    db.recordSnapshot(QStringLiteral("ProbeOnly"),
                      0,
                      0,
                      0,
                      99.0,
                      99.0,
                      99.0,
                      0,
                      0,
                      0,
                      0,
                      QStringLiteral("probe-model"),
                      false,
                      QStringLiteral("connectivity_probe"),
                      QStringLiteral("connectivity_probe"),
                      QStringLiteral("USD"),
                      QStringLiteral("probe_only"));
    QVERIFY(updateSnapshotData(QStringLiteral("ProbeOnly"),
                               99.0,
                               0,
                               0,
                               now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

    const QVariantMap overview = db.getAnalystOverview(30);
    QVERIFY(overview.contains(QStringLiteral("averageDailyCost")));
    QVERIFY(overview.contains(QStringLiteral("weekOverWeekPercent")));
    QVERIFY(overview.contains(QStringLiteral("topDrivers")));
    QVERIFY(overview.contains(QStringLiteral("topModels")));

    QVERIFY(overview.value(QStringLiteral("averageDailyCost")).toDouble() > 0.0);
    QVERIFY(overview.value(QStringLiteral("averageDailyCost")).toDouble() < 20.0);
    QVERIFY(overview.value(QStringLiteral("weekOverWeekPercent")).toDouble() > 0.0);
    QVERIFY(overview.value(QStringLiteral("anomalyCount")).toInt() >= 1);
    QVERIFY(overview.value(QStringLiteral("hasEstimatedData")).toBool());
    QVERIFY(overview.value(QStringLiteral("hasProbeOnlyData")).toBool());

    const QVariantList drivers = overview.value(QStringLiteral("topDrivers")).toList();
    QVERIFY(!drivers.isEmpty());
    QCOMPARE(drivers.first().toMap().value(QStringLiteral("provider")).toString(), QStringLiteral("OpenAI"));
    for (const QVariant &driverValue : drivers) {
        const QVariantMap driver = driverValue.toMap();
        QVERIFY(driver.value(QStringLiteral("provider")).toString() != QStringLiteral("ProbeOnly"));
        QVERIFY(driver.contains(QStringLiteral("costSource")));
        QVERIFY(driver.contains(QStringLiteral("usageSource")));
        QVERIFY(driver.contains(QStringLiteral("dataQuality")));
    }

    const QVariantList models = overview.value(QStringLiteral("topModels")).toList();
    QVERIFY(!models.isEmpty());
    QCOMPARE(models.first().toMap().value(QStringLiteral("model")).toString(), QStringLiteral("gpt-5.4"));
}

void UsageDatabaseAnalystTest::testAnalystSnapshotNoData()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime to = QDateTime::fromString(QStringLiteral("2026-07-23T23:59:59Z"), Qt::ISODate);
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Connectivity"),
        {metric(QStringLiteral("requests"), QStringLiteral("request"), 0, to.addSecs(-3600), {},
                QStringLiteral("connectivity_only"), QStringLiteral("connectivity_probe"))}));
    const QVariantMap snapshot = db.getAnalystSnapshot(to.addDays(-6), to);

    QVERIFY(snapshot.value(QStringLiteral("ok")).toBool());
    QCOMPARE(snapshot.value(QStringLiteral("currencyStatus")).toString(), QStringLiteral("none"));
    QVERIFY(snapshot.value(QStringLiteral("spendSeries")).toList().isEmpty());
    QVERIFY(snapshot.value(QStringLiteral("activitySeries")).toList().isEmpty());
    QVERIFY(!kpi(snapshot, QStringLiteral("averageDailySpend")).value(QStringLiteral("available")).toBool());
    QCOMPARE(kpi(snapshot, QStringLiteral("averageDailySpend")).value(QStringLiteral("reasonKey")).toString(),
             QStringLiteral("no_compatible_cost"));
}

void UsageDatabaseAnalystTest::testTypedDailyWindowIsAnalyzable()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime observedAt = QDateTime::fromString(QStringLiteral("2026-07-23T12:00:00Z"), Qt::ISODate);
    QVariantMap dailyMetric =
        metric(QStringLiteral("cost"), QStringLiteral("USD"), 2.5, observedAt, QStringLiteral("USD"));
    dailyMetric.remove(QStringLiteral("semantic"));
    dailyMetric.insert(QStringLiteral("window"), QStringLiteral("day"));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Daily"), {dailyMetric}));

    const QVariantMap snapshot = db.getAnalystSnapshot(observedAt.addDays(-6), observedAt.addSecs(3600));
    QCOMPARE(snapshot.value(QStringLiteral("spendSeries")).toList().size(), 1);
    QCOMPARE(snapshot.value(QStringLiteral("actualSampleCount")).toInt(), 1);
}

void UsageDatabaseAnalystTest::testAvailableZeroNeedsEnoughCoverage()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime observedAt = QDateTime::fromString(QStringLiteral("2026-07-23T12:00:00Z"), Qt::ISODate);
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Zero"), {metric(QStringLiteral("cost"), QStringLiteral("USD"), 0.0,
                                                                     observedAt, QStringLiteral("USD"))}));

    const QVariantMap snapshot = db.getAnalystSnapshot(observedAt.addDays(-6), observedAt.addSecs(3600));
    const QVariantList spend = snapshot.value(QStringLiteral("spendSeries")).toList();
    QCOMPARE(spend.size(), 1);
    const QVariantMap point = spend.constFirst().toMap();
    QVERIFY(point.value(QStringLiteral("actualAvailable")).toBool());
    QCOMPARE(point.value(QStringLiteral("actual")).toDouble(), 0.0);
    const QVariantMap average = kpi(snapshot, QStringLiteral("averageDailySpend"));
    QVERIFY(!average.value(QStringLiteral("available")).toBool());
    QCOMPARE(average.value(QStringLiteral("reasonKey")).toString(), QStringLiteral("insufficient_daily_samples"));
    QVERIFY(!average.value(QStringLiteral("value")).isValid());
}

void UsageDatabaseAnalystTest::testCompleteComparisonSeparatesActualAndEstimated()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime to = QDateTime::fromString(QStringLiteral("2026-07-23T23:59:59Z"), Qt::ISODate);
    for (int offset = 0; offset < 14; ++offset) {
        const QDateTime observedAt = QDateTime(to.date().addDays(-offset), QTime(12, 0), QTimeZone::utc());
        const double actual = offset == 2 ? 20.0 : (offset < 7 ? 4.0 : 2.0);
        QVERIFY(db.recordProviderMetrics(
            QStringLiteral("Actual"),
            {metric(QStringLiteral("cost"), QStringLiteral("USD"), actual, observedAt, QStringLiteral("USD"))}));
        QVERIFY(
            db.recordProviderMetrics(QStringLiteral("Estimated"),
                                     {metric(QStringLiteral("cost"), QStringLiteral("USD"), 0.5, observedAt,
                                             QStringLiteral("USD"), QStringLiteral("estimated"),
                                             QStringLiteral("estimated_pricing"), QStringLiteral("local_estimate"))}));
        const QString ratioProvider = QStringLiteral("Ratio-%1").arg(offset);
        db.recordSnapshot(ratioProvider, 100, 200, 2, 0.0, 0.0, 0.0, 0, 0, 0, 0, {}, false, QStringLiteral("unknown"),
                          QStringLiteral("usage_api"), QStringLiteral("USD"), QStringLiteral("actual"));
        QVERIFY(updateSnapshotData(ratioProvider, 0.0, 100, 200,
                                   observedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }

    const QVariantMap snapshot = db.getAnalystSnapshot(to.addDays(-29), to);
    QCOMPARE(snapshot.value(QStringLiteral("actualSampleCount")).toInt(), 14);
    QCOMPARE(snapshot.value(QStringLiteral("estimatedSampleCount")).toInt(), 14);
    QCOMPARE(snapshot.value(QStringLiteral("spendSeries")).toList().size(), 14);
    QVERIFY(kpi(snapshot, QStringLiteral("averageDailySpend")).value(QStringLiteral("available")).toBool());
    QVERIFY(kpi(snapshot, QStringLiteral("weekOverWeekChange")).value(QStringLiteral("available")).toBool());
    QVERIFY(kpi(snapshot, QStringLiteral("weekOverWeekChange")).value(QStringLiteral("value")).toDouble() > 0.0);
    QVERIFY(kpi(snapshot, QStringLiteral("outputInputRatio")).value(QStringLiteral("available")).toBool());
    QCOMPARE(kpi(snapshot, QStringLiteral("outputInputRatio")).value(QStringLiteral("value")).toDouble(), 2.0);
    QVERIFY(snapshot.value(QStringLiteral("anomaliesAvailable")).toBool());
    QVERIFY(!snapshot.value(QStringLiteral("anomalies")).toList().isEmpty());
    const QVariantList drivers = snapshot.value(QStringLiteral("topDrivers")).toList();
    QCOMPARE(drivers.size(), 2);
    QCOMPARE(drivers.at(0).toMap().value(QStringLiteral("quality")).toString(), QStringLiteral("actual"));
    QCOMPARE(drivers.at(1).toMap().value(QStringLiteral("quality")).toString(), QStringLiteral("estimated"));
}

void UsageDatabaseAnalystTest::testMixedCurrenciesPreserveActivity()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime observedAt = QDateTime::fromString(QStringLiteral("2026-07-23T12:00:00Z"), Qt::ISODate);
    QVERIFY(db.recordProviderMetrics(
        QStringLiteral("Mixed"),
        {
            metric(QStringLiteral("cost"), QStringLiteral("USD"), 1.0, observedAt, QStringLiteral("USD")),
            metric(QStringLiteral("cost"), QStringLiteral("EUR"), 2.0, observedAt.addSecs(1), QStringLiteral("EUR")),
            metric(QStringLiteral("input_tokens"), QStringLiteral("token"), 100.0, observedAt, {},
                   QStringLiteral("actual"), QStringLiteral("usage_api"), QStringLiteral("interval_total")),
        }));

    const QVariantMap snapshot = db.getAnalystSnapshot(observedAt.addDays(-6), observedAt.addSecs(3600));
    QVERIFY(snapshot.value(QStringLiteral("mixedCurrencies")).toBool());
    QVERIFY(snapshot.value(QStringLiteral("spendSeries")).toList().isEmpty());
    QCOMPARE(kpi(snapshot, QStringLiteral("averageDailySpend")).value(QStringLiteral("reasonKey")).toString(),
             QStringLiteral("mixed_currencies"));
    QVERIFY(snapshot.value(QStringLiteral("activityAvailable")).toBool());
    const QVariantMap activity = snapshot.value(QStringLiteral("activitySeries")).toList().constFirst().toMap();
    QVERIFY(activity.value(QStringLiteral("tokensAvailable")).toBool());
    QVERIFY(!activity.value(QStringLiteral("requestsAvailable")).toBool());
    QVERIFY(!activity.value(QStringLiteral("requests")).isValid());
}

void UsageDatabaseAnalystTest::testToolActivityWithoutProviderCost()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime observedAt = QDateTime::fromString(QStringLiteral("2026-07-23T12:00:00Z"), Qt::ISODate);
    db.recordToolSnapshot(QStringLiteral("Codex CLI"), 7, 20, QStringLiteral("daily"), QStringLiteral("Plus"), false);
    QVERIFY(updateLatestToolTimestamp(QStringLiteral("Codex CLI"), observedAt));

    const QVariantMap snapshot = db.getAnalystSnapshot(observedAt.addDays(-6), observedAt.addSecs(3600));
    QVERIFY(snapshot.value(QStringLiteral("activityAvailable")).toBool());
    const QVariantMap activity = snapshot.value(QStringLiteral("activitySeries")).toList().constFirst().toMap();
    QVERIFY(activity.value(QStringLiteral("toolUsageAvailable")).toBool());
    QCOMPARE(activity.value(QStringLiteral("toolUsage")).toDouble(), 7.0);
    QVERIFY(snapshot.value(QStringLiteral("spendSeries")).toList().isEmpty());
}

void UsageDatabaseAnalystTest::testExactRangeIsolation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QDateTime to = QDateTime::fromString(QStringLiteral("2026-07-23T23:59:59Z"), Qt::ISODate);
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Range"), {metric(QStringLiteral("cost"), QStringLiteral("USD"),
                                                                      3.0, to.addDays(-2), QStringLiteral("USD"))}));
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Range"), {metric(QStringLiteral("cost"), QStringLiteral("USD"),
                                                                      99.0, to.addDays(-20), QStringLiteral("USD"))}));

    const QVariantMap sevenDay = db.getAnalystSnapshot(to.addDays(-6), to);
    const QVariantMap thirtyDay = db.getAnalystSnapshot(to.addDays(-29), to);
    QCOMPARE(sevenDay.value(QStringLiteral("spendSeries")).toList().size(), 1);
    QCOMPARE(thirtyDay.value(QStringLiteral("spendSeries")).toList().size(), 2);
}

void UsageDatabaseAnalystTest::testAsyncRequestSupersession()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    QSignalSpy readySpy(&db, &UsageDatabase::analystReady);
    const QDateTime to = QDateTime::currentDateTimeUtc();
    db.requestAnalyst(QStringLiteral("older"), to.addDays(-29), to);
    db.requestAnalyst(QStringLiteral("latest"), to.addDays(-6), to);

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.size(), 1, 10000);
    QCOMPARE(readySpy.constFirst().at(0).toString(), QStringLiteral("latest"));
    QCOMPARE(readySpy.constFirst().at(1).toMap().value(QStringLiteral("requestId")).toString(),
             QStringLiteral("latest"));
    QTest::qWait(100);
    QCOMPARE(readySpy.size(), 1);
}

void UsageDatabaseAnalystTest::testHundredThousandObservations()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    const QString connectionName =
        QStringLiteral("analyst_bulk_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbFilePath());
        QVERIFY(database.open());
        QVERIFY(database.transaction());
        QSqlQuery query(database);
        query.prepare(QStringLiteral("INSERT INTO observations("
                                     "provider,observed_at_utc,metric_kind,unit,value,currency,"
                                     "semantic,source,data_quality,correlation_id"
                                     ") VALUES('Bulk',?,'requests','request',1,NULL,"
                                     "'interval_total','usage_api','actual',?)"));
        const QDateTime start = QDateTime::fromString(QStringLiteral("2026-07-01T00:00:00Z"), Qt::ISODate);
        for (int index = 0; index < 100000; ++index) {
            query.bindValue(0, start.addSecs(index % (23 * 24 * 3600)).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
            query.bindValue(1, QString::number(index));
            QVERIFY(query.exec());
        }
        QVERIFY(database.commit());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const QDateTime to = QDateTime::fromString(QStringLiteral("2026-07-23T23:59:59Z"), Qt::ISODate);
    QElapsedTimer timer;
    timer.start();
    const QVariantMap snapshot = db.getAnalystSnapshot(to.addDays(-29), to);
    QVERIFY(snapshot.value(QStringLiteral("ok")).toBool());
    QVERIFY(snapshot.value(QStringLiteral("activityAvailable")).toBool());
    QVERIFY(snapshot.value(QStringLiteral("activitySeries")).toList().size() <= 30);
    QVERIFY2(timer.elapsed() < 10000, qPrintable(QStringLiteral("Analyst query took %1 ms").arg(timer.elapsed())));
}

QTEST_MAIN(UsageDatabaseAnalystTest)
#include "test_usagedatabase_analyst.moc"
