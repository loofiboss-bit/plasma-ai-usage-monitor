#include "budgetpolicyschema.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

class LocalSessionSchemaTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void migrationCreatesPrivacyScopedTablesAndCanRetry() {
    const QString name = QStringLiteral("session_schema_%1")
                             .arg(reinterpret_cast<quintptr>(this));
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral(
        "CREATE TABLE budget_policies(policy_id TEXT PRIMARY KEY,"
        "limit_minor INTEGER NOT NULL)")));
    QVERIFY(q.exec(QStringLiteral(
        "INSERT INTO budget_policies VALUES('preserve-policy',2500)")));
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=8")));
    QString error;
    QVERIFY(!BudgetPolicySchema::migrateSessionSchema(db, &error, true));
    QVERIFY(!db.tables().contains(QStringLiteral("local_sessions")));
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")) && q.next());
    QCOMPARE(q.value(0).toInt(), 8);
    q.finish();
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(db, &error),
             qPrintable(error));
    QCOMPARE(db.tables().contains(QStringLiteral("local_sessions")), true);
    QCOMPARE(db.tables().contains(QStringLiteral("local_usage_events")), true);
    QCOMPARE(db.tables().contains(QStringLiteral("local_usage_conflicts")),
             true);
    QCOMPARE(db.tables().contains(QStringLiteral("local_import_checkpoints")),
             true);
    QCOMPARE(db.tables().contains(QStringLiteral("local_project_aliases")),
             true);
    QCOMPARE(db.tables().contains(QStringLiteral("observed_account_quotas")),
             true);
    QVERIFY(q.exec(QStringLiteral(
        "SELECT limit_minor FROM budget_policies WHERE policy_id="
        "'preserve-policy'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2500);
    q.finish();
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")) && q.next());
    QCOMPARE(q.value(0).toInt(), 9);
    q.finish();
    QVERIFY(q.exec(QStringLiteral("DROP TABLE local_usage_conflicts")));
    QVERIFY(BudgetPolicySchema::migrateSessionSchema(db, &error));
    QVERIFY(db.tables().contains(QStringLiteral("local_usage_conflicts")));
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(name);
  }

  void deliverySchemaAcceptsVersionNineBudgetDatabase() {
    const QString name = QStringLiteral("delivery_schema_v9_%1")
                             .arg(reinterpret_cast<quintptr>(this));
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("CREATE TABLE budget_policy_deliveries(x)")));
    QVERIFY(q.exec(QStringLiteral("CREATE TABLE history_operations(x)")));
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=9")));
    q.finish();
    QString error;
    QVERIFY2(BudgetPolicySchema::migrateDeliverySchema(db, &error),
             qPrintable(error));
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(name);
  }

  void fileBackupIsCompleteAndSurvivesFailedMigration() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.sqlite"));
    const QString name = QStringLiteral("session_schema_file_%1")
                             .arg(reinterpret_cast<quintptr>(this));
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode=WAL")));
    QVERIFY(q.exec(QStringLiteral("PRAGMA wal_autocheckpoint=0")));
    QVERIFY(q.exec(QStringLiteral("CREATE TABLE legacy(value TEXT NOT NULL)")));
    QVERIFY(q.exec(QStringLiteral("INSERT INTO legacy VALUES('preserve-me')")));
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=8")));
    q.finish();

    QString error;
    QVERIFY(!BudgetPolicySchema::migrateSessionSchema(db, &error, true));
    QVERIFY(QFileInfo::exists(path + QStringLiteral(".v24-backup")));
    const QString backupName = QStringLiteral("session_backup_read_%1")
                                   .arg(reinterpret_cast<quintptr>(this));
    auto backup =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), backupName);
    backup.setDatabaseName(path + QStringLiteral(".v24-backup"));
    QVERIFY(backup.open());
    QSqlQuery verify(backup);
    QVERIFY(verify.exec(QStringLiteral("PRAGMA integrity_check")));
    QVERIFY(verify.next());
    QCOMPARE(verify.value(0).toString(), QStringLiteral("ok"));
    QVERIFY(verify.exec(QStringLiteral("SELECT value FROM legacy")));
    QVERIFY(verify.next());
    QCOMPARE(verify.value(0).toString(), QStringLiteral("preserve-me"));
    verify.finish();
    backup.close();
    backup = {};
    QSqlDatabase::removeDatabase(backupName);

    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")) && q.next());
    QCOMPARE(q.value(0).toInt(), 8);
    q.finish();
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(db, &error),
             qPrintable(error));
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(name);
  }
};

QTEST_GUILESS_MAIN(LocalSessionSchemaTest)
#include "test_localsessionschema.moc"
