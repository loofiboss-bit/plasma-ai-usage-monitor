#include <QtTest>

#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QDir>
#include <QFileInfo>

#include "usagedatabase.h"

namespace {
QString databasePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
}

QStringList snapshotColumns()
{
    const QString connName = QStringLiteral("ext_schema_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QStringList columns;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(databasePath());
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral("PRAGMA table_info(usage_snapshots)"))) {
                while (query.next()) {
                    columns.append(query.value(1).toString());
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return columns;
}

/**
 * Directly update a snapshot timestamp for test purposes.
 */
bool setSnapshotTimestamp(const QString &provider, double cost, const QString &timestamp)
{
    const QString connName = QStringLiteral("ext_test_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(databasePath());
        if (db.open()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "UPDATE usage_snapshots SET timestamp = ? WHERE provider = ? AND ABS(cost - ?) < 0.00001"));
            query.addBindValue(timestamp);
            query.addBindValue(provider);
            query.addBindValue(cost);
            ok = query.exec() && query.numRowsAffected() > 0;
            QSqlQuery observationQuery(db);
            observationQuery.prepare(QStringLiteral(
                "UPDATE observations SET observed_at_utc = ? "
                "WHERE provider = ? AND metric_kind = 'cost' AND ABS(value - ?) < 0.00001"));
            observationQuery.addBindValue(timestamp);
            observationQuery.addBindValue(provider);
            observationQuery.addBindValue(cost);
            ok = ok && observationQuery.exec() && observationQuery.numRowsAffected() > 0;
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok;
}

bool setCostSemantic(const QString &provider, const QString &semantic)
{
    const QString connName = QStringLiteral("semantic_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(databasePath());
        if (db.open()) {
            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "UPDATE observations SET semantic = ? WHERE provider = ? AND metric_kind = 'cost'"));
            query.addBindValue(semantic);
            query.addBindValue(provider);
            ok = query.exec() && query.numRowsAffected() > 0;
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok;
}
} // namespace

class UsageDatabaseExtendedTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRetentionDaysClamping();
    void testGetProviders();
    void testGetToolNames();
    void testExportCsv();
    void testExportCsvRfc4180Quoting();
    void testExportJson();
    void testSourceMetadataSchemaMigration();
    void testObservationSchemaV3AndCurrencyIsolation();
    void testSourceMetadataPersistenceAndExports();
    void testGetSummary();
    void testGetDailyCosts();
    void testAsyncHistoryRequest();
    void testPruneOldData();
    void testDisabledRecording();
};

void UsageDatabaseExtendedTest::testRetentionDaysClamping()
{
    UsageDatabase db;

    db.setRetentionDays(0);
    QCOMPARE(db.retentionDays(), 1);

    db.setRetentionDays(-10);
    QCOMPARE(db.retentionDays(), 1);

    db.setRetentionDays(500);
    QCOMPARE(db.retentionDays(), 365);

    db.setRetentionDays(90);
    QCOMPARE(db.retentionDays(), 90);
}

void UsageDatabaseExtendedTest::testGetProviders()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordSnapshot(QStringLiteral("OpenAI"), 100, 50, 10, 1.0, 1.0, 10.0, 100, 90, 1000, 950);
    db.recordSnapshot(QStringLiteral("Anthropic"), 200, 100, 20, 2.0, 2.0, 20.0, 50, 40, 500, 400);

    // Wait past throttle for different provider
    QStringList providers = db.getProviders();
    QVERIFY(providers.contains(QStringLiteral("OpenAI")));
    QVERIFY(providers.contains(QStringLiteral("Anthropic")));
}

void UsageDatabaseExtendedTest::testObservationSchemaV3AndCurrencyIsolation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    db.recordSnapshot(QStringLiteral("OpenAI"), 100, 20, 2, 1.25, 1.25, 1.25,
                      0, 0, 0, 0, QStringLiteral("gpt"), false,
                      QStringLiteral("billing_api"), QStringLiteral("usage_api"),
                      QStringLiteral("USD"), QStringLiteral("complete"));
    db.recordSnapshot(QStringLiteral("European"), 50, 10, 1, 2.50, 2.50, 2.50,
                      0, 0, 0, 0, QStringLiteral("eu-model"), false,
                      QStringLiteral("billing_api"), QStringLiteral("usage_api"),
                      QStringLiteral("EUR"), QStringLiteral("complete"));

    const QString connectionName = QStringLiteral("observation_schema_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        check.setDatabaseName(databasePath());
        QVERIFY(check.open());
        QSqlQuery query(check);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 3);

        QVERIFY(query.exec(QStringLiteral(
            "SELECT currency, value, semantic, source, data_quality, correlation_id "
            "FROM observations WHERE metric_kind='cost' ORDER BY provider")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("EUR"));
        QCOMPARE(query.value(1).toDouble(), 2.50);
        QCOMPARE(query.value(2).toString(), QStringLiteral("gauge"));
        QCOMPARE(query.value(3).toString(), QStringLiteral("billing_api"));
        QCOMPARE(query.value(4).toString(), QStringLiteral("complete"));
        QVERIFY(!query.value(5).toString().isEmpty());
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("USD"));
        QCOMPARE(query.value(1).toDouble(), 1.25);
        QVERIFY(!query.next());
        check.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void UsageDatabaseExtendedTest::testGetToolNames()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordToolSnapshot(QStringLiteral("Claude Code"), 10, 45, QStringLiteral("5-hour"), QStringLiteral("Pro"), false);
    db.recordToolSnapshot(QStringLiteral("Copilot"), 5, 300, QStringLiteral("monthly"), QStringLiteral("Pro"), false);

    QStringList tools = db.getToolNames();
    QVERIFY(tools.contains(QStringLiteral("Claude Code")));
    QVERIFY(tools.contains(QStringLiteral("Copilot")));
}

void UsageDatabaseExtendedTest::testExportCsv()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordSnapshot(QStringLiteral("OpenAI"), 100, 50, 10, 1.5, 1.5, 15.0, 100, 90, 1000, 950);

    const QDateTime from = QDateTime::currentDateTimeUtc().addSecs(-3600);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);

    QString csv = db.exportCsv(QStringLiteral("OpenAI"), from, to);
    QVERIFY(!csv.isEmpty());

    // Check CSV has header row
    QStringList lines = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVERIFY(lines.size() >= 2); // header + at least 1 data row

    // Header should contain expected column names
    QString header = lines.first();
    QVERIFY(header.contains(QStringLiteral("timestamp")));
    QVERIFY(header.contains(QStringLiteral("cost")));
}

void UsageDatabaseExtendedTest::testExportJson()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordSnapshot(QStringLiteral("OpenAI"), 200, 100, 20, 3.0, 3.0, 30.0, 100, 80, 1000, 800);

    const QDateTime from = QDateTime::currentDateTimeUtc().addSecs(-3600);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);

    QString jsonStr = db.exportJson(QStringLiteral("OpenAI"), from, to);
    QVERIFY(!jsonStr.isEmpty());

    // Parse and validate JSON structure — exportJson wraps data in a root object
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QVERIFY(doc.isObject());

    QJsonObject root = doc.object();
    QCOMPARE(root.value(QStringLiteral("provider")).toString(), QStringLiteral("OpenAI"));
    QVERIFY(root.contains(QStringLiteral("from")));
    QVERIFY(root.contains(QStringLiteral("to")));
    QVERIFY(root.contains(QStringLiteral("snapshots")));

    QJsonArray arr = root.value(QStringLiteral("snapshots")).toArray();
    QVERIFY(arr.size() >= 1);

    QJsonObject first = arr.first().toObject();
    QVERIFY(first.contains(QStringLiteral("timestamp")));
    QVERIFY(first.contains(QStringLiteral("cost")));
    QVERIFY(qAbs(first.value(QStringLiteral("cost")).toDouble() - 3.0) < 0.01);
}

void UsageDatabaseExtendedTest::testExportCsvRfc4180Quoting()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    UsageDatabase db;
    db.init();
    const QString provider = QStringLiteral("Provider, \"quoted\"");
    db.recordSnapshot(provider, 1, 2, 1, 0.1, 0.1, 0.1, 0, 0, 0, 0,
                      QStringLiteral("model,\"line\"\nnext"));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString csv = db.exportCsv(provider, now.addSecs(-60), now.addSecs(60));
    QVERIFY(csv.contains(QStringLiteral("\"Provider, \"\"quoted\"\"\"")));
    QVERIFY(csv.contains(QStringLiteral("\"model,\"\"line\"\"\nnext\"")));
}

void UsageDatabaseExtendedTest::testSourceMetadataSchemaMigration()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    const QString dbPath = databasePath();
    QVERIFY(QDir().mkpath(QFileInfo(dbPath).absolutePath()));

    const QString connName = QStringLiteral("old_schema_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase oldDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        oldDb.setDatabaseName(dbPath);
        QVERIFY(oldDb.open());
        QSqlQuery query(oldDb);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE usage_snapshots ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp DATETIME DEFAULT (datetime('now')),"
            "provider TEXT NOT NULL,"
            "input_tokens INTEGER DEFAULT 0,"
            "output_tokens INTEGER DEFAULT 0,"
            "request_count INTEGER DEFAULT 0,"
            "cost REAL DEFAULT 0.0,"
            "daily_cost REAL DEFAULT 0.0,"
            "monthly_cost REAL DEFAULT 0.0,"
            "rl_requests INTEGER DEFAULT 0,"
            "rl_requests_remaining INTEGER DEFAULT 0,"
            "rl_tokens INTEGER DEFAULT 0,"
            "rl_tokens_remaining INTEGER DEFAULT 0"
            ")"
        )));
        oldDb.close();
    }
    QSqlDatabase::removeDatabase(connName);

    UsageDatabase db;
    db.init();

    const QStringList columns = snapshotColumns();
    QVERIFY(columns.contains(QStringLiteral("cost_source")));
    QVERIFY(columns.contains(QStringLiteral("usage_source")));
    QVERIFY(columns.contains(QStringLiteral("currency")));
    QVERIFY(columns.contains(QStringLiteral("data_quality")));
}

void UsageDatabaseExtendedTest::testSourceMetadataPersistenceAndExports()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordSnapshot(QStringLiteral("OpenAI"),
                      200,
                      100,
                      20,
                      3.0,
                      3.0,
                      30.0,
                      100,
                      80,
                      1000,
                      800,
                      QStringLiteral("gpt-5.4"),
                      false,
                      QStringLiteral("billing_api"),
                      QStringLiteral("actual_api"),
                      QStringLiteral("eur"),
                      QStringLiteral("actual_billing"));

    const QDateTime from = QDateTime::currentDateTimeUtc().addSecs(-3600);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);

    QVariantList snapshots = db.getSnapshots(QStringLiteral("OpenAI"), from, to);
    QCOMPARE(snapshots.size(), 1);
    QVariantMap first = snapshots.first().toMap();
    QCOMPARE(first.value(QStringLiteral("costSource")).toString(), QStringLiteral("billing_api"));
    QCOMPARE(first.value(QStringLiteral("usageSource")).toString(), QStringLiteral("actual_api"));
    QCOMPARE(first.value(QStringLiteral("currency")).toString(), QStringLiteral("EUR"));
    QCOMPARE(first.value(QStringLiteral("dataQuality")).toString(), QStringLiteral("actual_billing"));

    const QString csv = db.exportCsv(QStringLiteral("OpenAI"), from, to);
    QVERIFY(csv.contains(QStringLiteral("cost_source,usage_source,currency,data_quality")));
    QVERIFY(csv.contains(QStringLiteral("billing_api,actual_api,EUR,actual_billing")));

    const QString jsonStr = db.exportJson(QStringLiteral("OpenAI"), from, to);
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QVERIFY(doc.isObject());
    QJsonArray arr = doc.object().value(QStringLiteral("snapshots")).toArray();
    QCOMPARE(arr.size(), 1);
    QJsonObject row = arr.first().toObject();
    QCOMPARE(row.value(QStringLiteral("costSource")).toString(), QStringLiteral("billing_api"));
    QCOMPARE(row.value(QStringLiteral("usageSource")).toString(), QStringLiteral("actual_api"));
    QCOMPARE(row.value(QStringLiteral("currency")).toString(), QStringLiteral("EUR"));
    QCOMPARE(row.value(QStringLiteral("dataQuality")).toString(), QStringLiteral("actual_billing"));
}

void UsageDatabaseExtendedTest::testGetSummary()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordSnapshot(QStringLiteral("TestProv"), 100, 50, 5, 1.0, 1.0, 10.0, 100, 90, 1000, 950);
    // Need to bypass write throttling for second snapshot — use different cost
    db.recordSnapshot(QStringLiteral("TestProv"), 300, 200, 15, 3.0, 3.0, 30.0, 100, 70, 1000, 700);

    const QDateTime from = QDateTime::currentDateTimeUtc().addSecs(-3600);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);

    QVariantMap summary = db.getSummary(QStringLiteral("TestProv"), from, to);
    QVERIFY(!summary.isEmpty());
    QVERIFY(summary.contains(QStringLiteral("snapshotCount")));

    // Should have at least 1 snapshot (throttle may skip the second if same provider within 60s)
    QVERIFY(summary.value(QStringLiteral("snapshotCount")).toInt() >= 1);
}

void UsageDatabaseExtendedTest::testGetDailyCosts()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();

    db.recordSnapshot(QStringLiteral("DailyCostProv"), 100, 50, 10, 5.0, 5.0, 50.0, 0, 0, 0, 0);

    // Backdate the snapshot to yesterday
    QVERIFY(setSnapshotTimestamp(QStringLiteral("DailyCostProv"), 5.0,
                                  QDateTime::currentDateTimeUtc().addDays(-1).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));

    // Insert another for today (different cost to bypass throttle)
    db.recordSnapshot(QStringLiteral("DailyCostProv"), 200, 100, 20, 8.0, 8.0, 80.0, 0, 0, 0, 0);
    QVERIFY(setCostSemantic(QStringLiteral("DailyCostProv"), QStringLiteral("interval_total")));

    const QDateTime from = QDateTime::currentDateTimeUtc().addDays(-2);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);

    QVariantList dailyCosts = db.getDailyCosts(QStringLiteral("DailyCostProv"), from, to);
    QCOMPARE(dailyCosts.size(), 2);
    QCOMPARE(dailyCosts.first().toMap().value(QStringLiteral("currency")).toString(), QStringLiteral("USD"));
}

void UsageDatabaseExtendedTest::testPruneOldData()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    db.setRetentionDays(1);

    db.recordSnapshot(QStringLiteral("PruneProv"), 100, 50, 10, 1.0, 1.0, 10.0, 0, 0, 0, 0);

    // Backdate the snapshot to 5 days ago (beyond 1-day retention)
    QVERIFY(setSnapshotTimestamp(QStringLiteral("PruneProv"), 1.0,
                                  QDateTime::currentDateTimeUtc().addDays(-5).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));

    // Insert a recent one (different cost to bypass throttle)
    db.recordSnapshot(QStringLiteral("PruneProv"), 200, 100, 20, 9.0, 9.0, 90.0, 0, 0, 0, 0);

    db.pruneOldData();

    const QDateTime from = QDateTime::currentDateTimeUtc().addDays(-10);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);
    QVariantList snapshots = db.getSnapshots(QStringLiteral("PruneProv"), from, to);

    // Old snapshot should be pruned, recent one kept
    QCOMPARE(snapshots.size(), 1);
    QVERIFY(qAbs(snapshots.first().toMap().value(QStringLiteral("cost")).toDouble() - 9.0) < 0.01);
}

void UsageDatabaseExtendedTest::testAsyncHistoryRequest()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    db.recordSnapshot(QStringLiteral("AsyncProvider"), 10, 5, 1, 0.5, 0.5, 0.5,
                      0, 0, 0, 0, QStringLiteral("model"), false,
                      QStringLiteral("billing_api"), QStringLiteral("usage_api"));
    QSignalSpy spy(&db, &UsageDatabase::historyReady);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    db.requestHistory(QStringLiteral("request-1"), QStringLiteral("AsyncProvider"),
                      now.addSecs(-60), now.addSecs(60));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("request-1"));
    const QVariantMap payload = spy.first().at(1).toMap();
    QCOMPARE(payload.value(QStringLiteral("snapshots")).toList().size(), 1);
    QVERIFY(payload.contains(QStringLiteral("summary")));
    QVERIFY(payload.contains(QStringLiteral("dailyCosts")));
}

void UsageDatabaseExtendedTest::testDisabledRecording()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

    UsageDatabase db;
    db.init();
    db.setEnabled(false);

    db.recordSnapshot(QStringLiteral("DisabledProv"), 100, 50, 10, 1.0, 1.0, 10.0, 0, 0, 0, 0);

    const QDateTime from = QDateTime::currentDateTimeUtc().addSecs(-3600);
    const QDateTime to = QDateTime::currentDateTimeUtc().addSecs(3600);
    QVariantList snapshots = db.getSnapshots(QStringLiteral("DisabledProv"), from, to);

    QCOMPARE(snapshots.size(), 0);
}

QTEST_MAIN(UsageDatabaseExtendedTest)
#include "test_usagedatabase_extended.moc"
