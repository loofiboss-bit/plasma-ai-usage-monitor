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
#include <QFile>
#include <QRegularExpression>
#include <QTimeZone>

#include "forecastcontract.h"
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

bool installSqlFixture(const QString &fixturePath)
{
    QDir().mkpath(QFileInfo(databasePath()).absolutePath());
    QFile fixture(fixturePath);
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString connectionName = QStringLiteral("fixture_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath());
        ok = db.open();
        const QStringList statements = QString::fromUtf8(fixture.readAll()).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &statement : statements) {
            if (!ok) break;
            if (statement.trimmed().isEmpty()) continue;
            QSqlQuery query(db);
            ok = query.exec(statement.trimmed());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

ForecastContract::Result guardrailForecast(ForecastContract::State state)
{
    ForecastContract::Result result;
    result.kind = ForecastContract::Kind::BudgetOverrun;
    result.state = state;
    result.sourceId = QStringLiteral("provider:openai");
    result.sourceKind = QStringLiteral("provider");
    result.window = QStringLiteral("2026-07");
    result.scope = QStringLiteral("account");
    result.currentValue = 75.0;
    result.projectedValue = 120.0;
    result.limitValue = 100.0;
    result.unit = QStringLiteral("USD");
    result.currency = QStringLiteral("USD");
    result.predictedAt = QDateTime(QDate(2026, 7, 27), QTime(0, 0), QTimeZone::UTC);
    result.periodEnd = QDateTime(QDate(2026, 8, 1), QTime(0, 0), QTimeZone::UTC);
    result.sampleCount = 20;
    result.coveragePercent = 90.0;
    result.evidenceGrade = ForecastContract::EvidenceGrade::Strong;
    result.methodId = QStringLiteral("budget-pacing-v1");
    result.generatedAt = QDateTime(QDate(2026, 7, 24), QTime(12, 0), QTimeZone::UTC);
    return result;
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
    void testObservationSchemaV4AndCurrencyIsolation();
    void testSchemaV5MigrationRollbackAndRecovery();
    void testGuardrailTransitionRestartDeduplication();
    void testGuardrailEventExportAndRetention();
    void testNullableTypedMetricPersistence();
    void testRealLegacyFixtureMigration_data();
    void testRealLegacyFixtureMigration();
    void testSourceMetadataPersistenceAndExports();
    void testGetSummary();
    void testGetDailyCosts();
    void testAsyncHistoryRequest();
    void testPruneOldData();
    void testDisabledRecording();
};

void UsageDatabaseExtendedTest::testRealLegacyFixtureMigration_data()
{
    QTest::addColumn<QString>("fixtureName");
    QTest::addColumn<QString>("provider");
    QTest::addColumn<QString>("source");
    QTest::newRow("v11") << QStringLiteral("v11.sql") << QStringLiteral("OpenAI") << QStringLiteral("billing_api");
    QTest::newRow("v12") << QStringLiteral("v12.sql") << QStringLiteral("OpenRouter") << QStringLiteral("actual_api");
    QTest::newRow("v16") << QStringLiteral("v16.sql") << QStringLiteral("Anthropic") << QStringLiteral("billing_api");
}

void UsageDatabaseExtendedTest::testRealLegacyFixtureMigration()
{
    QFETCH(QString, fixtureName);
    QFETCH(QString, provider);
    QFETCH(QString, source);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    const QString fixture = QFINDTESTDATA(QStringLiteral("fixtures/database/") + fixtureName);
    QVERIFY2(!fixture.isEmpty(), qPrintable(fixtureName));
    QVERIFY(installSqlFixture(fixture));
    {
        UsageDatabase db;
        db.init();
    }
    if (fixtureName != QLatin1String("v16.sql")) {
        QVERIFY(QFileInfo::exists(databasePath() + QStringLiteral(".v13-backup")));
    }
    QVERIFY(QFileInfo::exists(databasePath() + QStringLiteral(".v17-backup")));

    const QString connectionName = QStringLiteral("verify_fixture_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    qint64 observationCount = 0;
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        check.setDatabaseName(databasePath());
        QVERIFY(check.open());
        QSqlQuery query(check);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
        QCOMPARE(query.value(0).toInt(), 7);
        query.prepare(QStringLiteral("SELECT COUNT(*), MIN(source) FROM observations WHERE provider=? AND metric_kind='cost'"));
        query.addBindValue(provider);
        QVERIFY(query.exec() && query.next());
        observationCount = query.value(0).toLongLong();
        QVERIFY(observationCount > 0);
        QCOMPARE(query.value(1).toString(), source);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM metric_source_mapping")) && query.next());
        QVERIFY(query.value(0).toInt() >= 10);
        check.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    // A second initialization must not duplicate migrated history.
    {
        UsageDatabase db;
        db.init();
    }
    const QString secondConnection = QStringLiteral("verify_idempotent_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), secondConnection);
        check.setDatabaseName(databasePath());
        QVERIFY(check.open());
        QSqlQuery query(check);
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM observations WHERE provider=? AND metric_kind='cost'"));
        query.addBindValue(provider);
        QVERIFY(query.exec() && query.next());
        QCOMPARE(query.value(0).toLongLong(), observationCount);
        check.close();
    }
    QSqlDatabase::removeDatabase(secondConnection);
}

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

void UsageDatabaseExtendedTest::testObservationSchemaV4AndCurrencyIsolation()
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
        QCOMPARE(query.value(0).toInt(), 7);

        QVERIFY(query.exec(QStringLiteral("PRAGMA table_info(observations)")));
        bool nullableValue = false;
        bool hasScope = false;
        bool hasWindow = false;
        bool hasServiceTierScope = false;
        bool hasLineItemScope = false;
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("value")) nullableValue = query.value(3).toInt() == 0;
            if (query.value(1).toString() == QLatin1String("scope")) hasScope = true;
            if (query.value(1).toString() == QLatin1String("window")) hasWindow = true;
            if (query.value(1).toString() == QLatin1String("service_tier_scope")) hasServiceTierScope = true;
            if (query.value(1).toString() == QLatin1String("line_item_scope")) hasLineItemScope = true;
        }
        QVERIFY(nullableValue);
        QVERIFY(hasScope);
        QVERIFY(hasWindow);
        QVERIFY(hasServiceTierScope);
        QVERIFY(hasLineItemScope);

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
        QVERIFY(query.exec(QStringLiteral(
            "SELECT normalized_source FROM metric_source_mapping WHERE legacy_source='estimated_from_usage'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("estimated_pricing"));
        check.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

}

void UsageDatabaseExtendedTest::testSchemaV5MigrationRollbackAndRecovery()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    const QString fixture = QFINDTESTDATA(QStringLiteral("fixtures/database/v16.sql"));
    QVERIFY(!fixture.isEmpty());
    QVERIFY(installSqlFixture(fixture));

    const QString conflictConnection
        = QStringLiteral("v5_conflict_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conflictConnection);
        db.setDatabaseName(databasePath());
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE guardrail_events(id INTEGER)")));
        db.close();
    }
    QSqlDatabase::removeDatabase(conflictConnection);

    {
        UsageDatabase db;
        db.init();
    }
    QVERIFY(QFileInfo::exists(databasePath() + QStringLiteral(".v17-backup")));

    const QString rollbackConnection
        = QStringLiteral("v5_rollback_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), rollbackConnection);
        db.setDatabaseName(databasePath());
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
        QCOMPARE(query.value(0).toInt(), 4);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM observations")) && query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM pragma_table_info('guardrail_events')")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("DROP TABLE guardrail_events")));
        db.close();
    }
    QSqlDatabase::removeDatabase(rollbackConnection);

    {
        UsageDatabase db;
        db.init();
    }
    const QString recoveryConnection
        = QStringLiteral("v5_recovery_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), recoveryConnection);
        db.setDatabaseName(databasePath());
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
        QCOMPARE(query.value(0).toInt(), 7);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM observations")) && query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        db.close();
    }
    QSqlDatabase::removeDatabase(recoveryConnection);
}

void UsageDatabaseExtendedTest::testGuardrailTransitionRestartDeduplication()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    ForecastContract::Result forecast = guardrailForecast(ForecastContract::State::Warning);
    const QString stableId = forecast.stableId();

    {
        UsageDatabase db;
        db.init();
        QVERIFY(db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("warning")));
        QVERIFY(!db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("warning")));
        const QVariantMap event = db.lastGuardrailTransition(stableId);
        QCOMPARE(event.value(QStringLiteral("transition")).toString(), QStringLiteral("warning"));
        QCOMPARE(event.value(QStringLiteral("currentValue")).toDouble(), 75.0);
    }

    {
        UsageDatabase db;
        db.init();
        QVERIFY(!db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("warning")));
        forecast.state = ForecastContract::State::Critical;
        forecast.generatedAt = forecast.generatedAt.addSecs(60);
        QVERIFY(db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("critical")));
        forecast.state = ForecastContract::State::Safe;
        forecast.generatedAt = forecast.generatedAt.addSecs(60);
        forecast.predictedAt.reset();
        QVERIFY(db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("recovered")));
        QVERIFY(!db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("recovered")));
        const QVariantMap event = db.lastGuardrailTransition(stableId);
        QCOMPARE(event.value(QStringLiteral("transition")).toString(), QStringLiteral("recovered"));
    }

    const QString connectionName
        = QStringLiteral("v5_dedup_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath());
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM guardrail_events")) && query.next());
        QCOMPARE(query.value(0).toInt(), 3);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM observations")) && query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void UsageDatabaseExtendedTest::testGuardrailEventExportAndRetention()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    UsageDatabase db;
    db.init();
    db.setRetentionDays(1);

    ForecastContract::Result forecast = guardrailForecast(ForecastContract::State::Warning);
    forecast.generatedAt = QDateTime::currentDateTimeUtc().addDays(-5);
    forecast.predictedAt = forecast.generatedAt.addDays(1);
    forecast.periodEnd = forecast.generatedAt.addDays(7);
    QVERIFY(db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("warning")));
    forecast.state = ForecastContract::State::Critical;
    forecast.generatedAt = QDateTime::currentDateTimeUtc();
    forecast.predictedAt = forecast.generatedAt.addSecs(3600);
    forecast.periodEnd = forecast.generatedAt.addDays(7);
    QVERIFY(db.recordGuardrailTransition(forecast.toVariantMap(), QStringLiteral("critical")));

    const QString exportDir = tmp.path() + QStringLiteral("/guardrail-export");
    const QStringList exports = db.exportAllToDirectory(exportDir, { QStringLiteral("json"), QStringLiteral("csv") });
    QCOMPARE(exports.size(), 5);
    const QString jsonPath = exports.filter(QRegularExpression(QStringLiteral("\\.json$"))).value(0);
    QFile jsonFile(jsonPath);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(jsonFile.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 6);
    QCOMPARE(root.value(QStringLiteral("guardrailEvents")).toArray().size(), 2);
    const QString guardrailCsv
        = exports.filter(QRegularExpression(QStringLiteral("guardrail-events-.*\\.csv$"))).value(0);
    QFile csvFile(guardrailCsv);
    QVERIFY(csvFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(csvFile.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts).size(), 3);

    db.pruneOldData();
    const QString connectionName
        = QStringLiteral("v5_retention_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        check.setDatabaseName(databasePath());
        QVERIFY(check.open());
        QSqlQuery query(check);
        QVERIFY(query.exec(QStringLiteral("SELECT transition FROM guardrail_events ORDER BY id")) && query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("critical"));
        QVERIFY(!query.next());
        check.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void UsageDatabaseExtendedTest::testNullableTypedMetricPersistence()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    UsageDatabase db;
    db.init();
    const QDateTime start(QDate(2026, 7, 13), QTime(0, 0), QTimeZone::UTC);
    const QDateTime end = start.addDays(1);
    QVariantMap unavailable{{QStringLiteral("kind"), QStringLiteral("token_remaining")},
                            {QStringLiteral("available"), false},
                            {QStringLiteral("unit"), QStringLiteral("token")},
                            {QStringLiteral("source"), QStringLiteral("published_documentation")},
                            {QStringLiteral("quality"), QStringLiteral("unknown")},
                            {QStringLiteral("scope"), QStringLiteral("project")},
                            {QStringLiteral("window"), QStringLiteral("day")},
                            {QStringLiteral("modelScope"), QStringLiteral("gemini-2.5-pro")},
                            {QStringLiteral("projectScope"), QStringLiteral("workspace-a")},
                            {QStringLiteral("serviceTierScope"), QStringLiteral("priority")},
                            {QStringLiteral("lineItemScope"), QStringLiteral("messages")},
                            {QStringLiteral("periodStart"), start},
                            {QStringLiteral("periodEnd"), end}};
    QVariantMap actualZero = unavailable;
    actualZero[QStringLiteral("kind")] = QStringLiteral("requests");
    actualZero[QStringLiteral("available")] = true;
    actualZero[QStringLiteral("value")] = 0;
    actualZero[QStringLiteral("source")] = QStringLiteral("usage_api");
    actualZero[QStringLiteral("quality")] = QStringLiteral("actual");
    QVERIFY(db.recordProviderMetrics(QStringLiteral("Gemini"), {unavailable, actualZero}));

    const QString connectionName = QStringLiteral("nullable_metrics_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        check.setDatabaseName(databasePath());
        QVERIFY(check.open());
        QSqlQuery query(check);
        QVERIFY(query.exec(QStringLiteral(
            "SELECT metric_kind,value,interval_start_utc,interval_end_utc,scope,window,source,"
            "model_scope,project_scope,service_tier_scope,line_item_scope "
            "FROM observations WHERE provider='Gemini' ORDER BY id")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("token_remaining"));
        QVERIFY(query.value(1).isNull());
        QVERIFY(!query.value(2).isNull());
        QVERIFY(!query.value(3).isNull());
        QCOMPARE(query.value(4).toString(), QStringLiteral("project"));
        QCOMPARE(query.value(5).toString(), QStringLiteral("day"));
        QCOMPARE(query.value(6).toString(), QStringLiteral("published_documentation"));
        QCOMPARE(query.value(7).toString(), QStringLiteral("gemini-2.5-pro"));
        QCOMPARE(query.value(8).toString(), QStringLiteral("workspace-a"));
        QCOMPARE(query.value(9).toString(), QStringLiteral("priority"));
        QCOMPARE(query.value(10).toString(), QStringLiteral("messages"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("requests"));
        QCOMPARE(query.value(1).toDouble(), 0.0);
        QVERIFY(!query.next());
        check.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const QString exportDir = tmp.path() + QStringLiteral("/typed-export");
    const QStringList exports = db.exportAllToDirectory(exportDir, {QStringLiteral("json"), QStringLiteral("csv")});
    QCOMPARE(exports.size(), 5); // combined JSON plus four typed CSV files
    const QString jsonPath = exports.filter(QRegularExpression(QStringLiteral("\\.json$"))).value(0);
    QFile jsonFile(jsonPath);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QJsonObject exportRoot = QJsonDocument::fromJson(jsonFile.readAll()).object();
    const QJsonArray observations = exportRoot.value(QStringLiteral("observations")).toArray();
    QCOMPARE(observations.size(), 2);
    QVERIFY(observations.at(0).toObject().contains(QStringLiteral("value")));
    QVERIFY(observations.at(0).toObject().value(QStringLiteral("value")).isNull());
    QCOMPARE(observations.at(1).toObject().value(QStringLiteral("value")).toDouble(), 0.0);
    for (const QString &path : exports) {
        QFile exported(path);
        QVERIFY(exported.open(QIODevice::ReadOnly));
        const QByteArray contents = exported.readAll();
        QVERIFY(!contents.contains("gemini-2.5-pro"));
        QVERIFY(!contents.contains("workspace-a"));
        QVERIFY(!contents.contains("priority"));
        QVERIFY(!contents.contains("messages"));
    }

    const QString observationsCsv = exports.filter(QRegularExpression(QStringLiteral("observations-.*\\.csv$"))).value(0);
    QFile csvFile(observationsCsv);
    QVERIFY(csvFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList csvLines = QString::fromUtf8(csvFile.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QCOMPARE(csvLines.size(), 3);
    const QStringList nullFields = csvLines.at(1).trimmed().split(QLatin1Char(','), Qt::KeepEmptyParts);
    const QStringList zeroFields = csvLines.at(2).trimmed().split(QLatin1Char(','), Qt::KeepEmptyParts);
    QCOMPARE(nullFields.value(4), QStringLiteral("token_remaining"));
    QVERIFY(nullFields.value(6).isEmpty());
    QCOMPARE(zeroFields.value(4), QStringLiteral("requests"));
    QCOMPARE(zeroFields.value(6), QStringLiteral("0"));
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
    QVERIFY(!first.contains(QStringLiteral("model")));
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
    QVERIFY(!csv.contains(QStringLiteral("model,\"\"line")));
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
