#include "usagedatabase.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class HistoryMaintenanceTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void storageAndPruningAreAsync();
  void exportResultsAndEvidence();
  void initializationFailsClosed();
  void partialExportIsExplicit();
};

void HistoryMaintenanceTest::storageAndPruningAreAsync() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  qputenv("XDG_DATA_HOME", root.path().toUtf8());
  UsageDatabase writer;
  QVERIFY(writer.init());
  writer.recordSnapshot("Retained", 10, 5, 1, 1, 1, 1, 0, 0, 0, 0);
  UsageDatabase settings;
  QSignalSpy spy(&settings, &UsageDatabase::maintenanceFinished);
  settings.requestStorageStatus("open");
  QTRY_COMPARE(spy.count(), 1);
  QVariantMap result = spy.takeFirst()[1].toMap();
  QVERIFY(result.value("ok").toBool());
  QCOMPARE(result.value("requestId").toString(), QStringLiteral("open"));
  QVERIFY(result.value("databaseBytes").toLongLong() > 0);
  QVERIFY(result.value("sources").toStringList().contains("Retained"));
  QVERIFY(result.contains("walBytes"));

  const QString connection = QUuid::createUuid().toString();
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(
        root.filePath("plasma-ai-usage-monitor/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec(
        "UPDATE usage_snapshots SET timestamp='2000-01-01 00:00:00'"));
    QVERIFY(query.exec(
        "UPDATE observations SET observed_at_utc='2000-01-01 00:00:00'"));
  }
  QSqlDatabase::removeDatabase(connection);
  settings.requestPrune("prune");
  QTRY_COMPARE(spy.count(), 1);
  result = spy.takeFirst()[1].toMap();
  QVERIFY(result.value("ok").toBool());
  QVERIFY(result.value("rowsDeleted").toLongLong() >= 1);
  QVERIFY(!result.value("sources").toStringList().contains("Retained"));
  QTRY_COMPARE(settings.pendingWorkerCount(), 0);
}

void HistoryMaintenanceTest::exportResultsAndEvidence() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  qputenv("XDG_DATA_HOME", root.path().toUtf8());
  UsageDatabase database;
  QVERIFY(database.init());
  const QVariantMap provenance{
      {"scopeIdentity", "private-project"},
      {"token", "private-token"},
      {"webhookUrl", "https://private-webhook.invalid"}};
  database.recordSnapshot("Export fixture", 10, 5, 1, 1, 1, 1, 0, 0, 0, 0, {},
                          false, "usage_api", "usage_api", "USD", "actual",
                          provenance);
  QVERIFY(database.recordProviderMetrics(
      "Export fixture", {QVariantMap{{"kind", "cost"},
                                     {"unit", "currency"},
                                     {"currency", "USD"},
                                     {"semantic", "interval_total"},
                                     {"available", true},
                                     {"value", 1},
                                     {"projectScope", "private-project"},
                                     {"provenance", provenance}}}));
  const QString connection = QUuid::createUuid().toString();
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(
        root.filePath("plasma-ai-usage-monitor/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec(
        "INSERT INTO "
        "budget_policies(owner_id,policy_id,source_id,source_kind,scope_mode,"
        "scope_kind,scope_identity,scope_label,value_class,limit_minor,"
        "currency,period_type,time_zone_id,warning_percent,critical_percent,"
        "created_at_utc,updated_at_utc) "
        "VALUES('private-owner','private-policy','openai','provider','scoped','"
        "project','private-project','private-label','actual',100,'USD','"
        "calendar_month','UTC',80,95,'2026-10-01','2026-10-01')"));
    QVERIFY(query.exec("INSERT INTO "
                       "budget_policy_events(policy_id,period_start_utc,period_"
                       "end_utc,transition,deduplication_key,delivery_status,"
                       "created_at_utc,reason_key) "
                       "VALUES('private-policy','2026-10-01','2026-11-01','"
                       "warning','private-dedup','pending','2026-10-02','')"));
    QVERIFY(
        query.exec("INSERT INTO "
                   "budget_policy_deliveries(event_id,channel,status,attempts,"
                   "reason_key) VALUES(1,'slack','pending',1,'http-503')"));
  }
  QSqlDatabase::removeDatabase(connection);
  QSignalSpy spy(&database, &UsageDatabase::exportFinished);
  database.requestExportAll("scheduled-test", root.filePath("exports"),
                            {"json", "csv"});
  QTRY_COMPARE(spy.count(), 1);
  const QVariantMap result = spy.takeFirst()[1].toMap();
  QVERIFY2(result.value("ok").toBool(),
           qPrintable(result.value("errorKey").toString()));
  QCOMPARE(result.value("paths").toStringList().size(), 6);
  for (const QString &path : result.value("paths").toStringList()) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    QVERIFY(!data.contains("private-owner"));
    QVERIFY(!data.contains("private-policy"));
    QVERIFY(!data.contains("private-project"));
    QVERIFY(!data.contains("private-label"));
    QVERIFY(!data.contains("private-dedup"));
    QVERIFY(!data.contains("private-token"));
    QVERIFY(!data.contains("private-webhook"));
    QVERIFY(!data.contains("provenance_json"));
    if (path.endsWith(".json")) {
      const auto json = QJsonDocument::fromJson(data).object();
      QCOMPARE(json.value("schemaVersion").toInt(), 7);
      const auto event =
          json.value("budgetPolicyEvents").toArray().at(0).toObject();
      QCOMPARE(event.value("transition").toString(), QStringLiteral("warning"));
      QCOMPARE(event.value("channel").toString(), QStringLiteral("slack"));
      QCOMPARE(event.value("channel_status").toString(),
               QStringLiteral("pending"));
    }
  }
  database.requestStorageStatus("after-export");
  QSignalSpy storage(&database, &UsageDatabase::maintenanceFinished);
  QTRY_COMPARE(storage.count(), 1);
  QCOMPARE(storage.first()[1]
               .toMap()
               .value("lastScheduledExport")
               .toMap()
               .value("status")
               .toString(),
           QStringLiteral("success"));
  QFile obstacle(root.filePath("not-a-directory"));
  QVERIFY(obstacle.open(QIODevice::WriteOnly));
  obstacle.close();
  database.requestExportAll("manual-failed", obstacle.fileName(), {"json"});
  QTRY_COMPARE(spy.count(), 1);
  QCOMPARE(spy.first()[1].toMap().value("status").toString(),
           QStringLiteral("failed"));
  QCOMPARE(spy.first()[1].toMap().value("errorKey").toString(),
           QStringLiteral("export-directory-unwritable"));
}

void HistoryMaintenanceTest::initializationFailsClosed() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  qputenv("XDG_DATA_HOME", root.path().toUtf8());
  QDir().mkpath(root.filePath("plasma-ai-usage-monitor"));
  const QString connection = QUuid::createUuid().toString();
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(
        root.filePath("plasma-ai-usage-monitor/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec("PRAGMA user_version=99"));
    QVERIFY(query.exec("CREATE TABLE sentinel(value TEXT)"));
    QVERIFY(query.exec("INSERT INTO sentinel VALUES('preserved')"));
  }
  QSqlDatabase::removeDatabase(connection);
  UsageDatabase database;
  QVERIFY(!database.init());
  QVERIFY(!database.initialized());
  QCOMPARE(database.errorKey(), QStringLiteral("unsupported-database-schema"));
  database.requestStorageStatus("failure");
  QSignalSpy spy(&database, &UsageDatabase::maintenanceFinished);
  QTRY_COMPARE(spy.count(), 1);
  QVERIFY(!spy.first()[1].toMap().value("ok").toBool());
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(
        root.filePath("plasma-ai-usage-monitor/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec("PRAGMA user_version"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 99);
    QVERIFY(query.exec("SELECT value FROM sentinel"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("preserved"));
  }
  QSqlDatabase::removeDatabase(connection);
}

void HistoryMaintenanceTest::partialExportIsExplicit() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  qputenv("XDG_DATA_HOME", root.path().toUtf8());
  UsageDatabase database;
  QVERIFY(database.init());
  const QString connection = QUuid::createUuid().toString();
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(
        root.filePath("plasma-ai-usage-monitor/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec("DROP TABLE observations"));
  }
  QSqlDatabase::removeDatabase(connection);
  const auto result =
      database.exportAllToDirectory(root.filePath("partial"), {"json", "csv"});
  QCOMPARE(result.value("status").toString(), QStringLiteral("partial"));
  QCOMPARE(result.value("paths").toStringList().size(), 4);
  for (const QString &path : result.value("paths").toStringList())
    QVERIFY(!path.endsWith(".json"));
}

QTEST_GUILESS_MAIN(HistoryMaintenanceTest)
#include "test_historymaintenance.moc"
