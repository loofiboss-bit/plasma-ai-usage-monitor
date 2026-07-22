#include <QtTest>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

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
} // namespace

class UsageDatabaseAnalystTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testYearlyActivity();
    void testEfficiencySeries();
    void testEmptyEfficiencySeries();
    void testAnalystOverview();
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

QTEST_MAIN(UsageDatabaseAnalystTest)
#include "test_usagedatabase_analyst.moc"
