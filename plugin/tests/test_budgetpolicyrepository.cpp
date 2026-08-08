#include "budgetpolicymodel.h"
#include "budgetpolicyrepository.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class BudgetPolicyRepositoryTest : public QObject {
  Q_OBJECT

private:
  static void seedV5(const QString &path) {
    const QString name =
        QStringLiteral("seed_%1").arg(reinterpret_cast<quintptr>(&path));
    {
      QSqlDatabase db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
      db.setDatabaseName(path);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(QStringLiteral(
          "CREATE TABLE preserved(id INTEGER PRIMARY KEY, value TEXT)")));
      QVERIFY(query.exec(
          QStringLiteral("INSERT INTO preserved(value) VALUES('v5-data')")));
      QVERIFY(query.exec(QStringLiteral(
          "CREATE TABLE observations(id INTEGER PRIMARY KEY, model_scope TEXT "
          "DEFAULT '', project_scope TEXT DEFAULT '')")));
      QVERIFY(query.exec(
          QStringLiteral("INSERT INTO observations(model_scope,project_scope) "
                         "VALUES('model-local','project-local')")));
      QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=5")));
      db.close();
    }
    QSqlDatabase::removeDatabase(name);
  }

  static QVariantMap
  validPolicy(const QString &source = QStringLiteral("openai")) {
    return {
        {QStringLiteral("sourceId"), source},
        {QStringLiteral("sourceKind"), QStringLiteral("provider")},
        {QStringLiteral("scopeMode"), QStringLiteral("aggregate")},
        {QStringLiteral("valueClass"), QStringLiteral("actual")},
        {QStringLiteral("limitMinor"), 12500},
        {QStringLiteral("currency"), QStringLiteral("USD")},
        {QStringLiteral("periodType"), QStringLiteral("calendar_month")},
        {QStringLiteral("timeZoneId"), QStringLiteral("Europe/Stockholm")},
        {QStringLiteral("warningPercent"), 80},
        {QStringLiteral("criticalPercent"), 90},
    };
  }

private Q_SLOTS:
  void crudDuplicateValidationAndIsolation();
  void legacyMigrationIsIdempotentAndDeletionIsPermanent();
  void replaceIsAtomic();
  void injectedMigrationFailureRollsBack();
};

void BudgetPolicyRepositoryTest::crudDuplicateValidationAndIsolation() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);

  BudgetPolicyRepository first;
  first.setDatabasePath(path);
  first.setOwnerId(QStringLiteral("applet:1"));
  QVERIFY2(first.init(), qPrintable(first.errorString()));
  QVERIFY(QFileInfo::exists(path + QStringLiteral(".v18-backup")));
  QCOMPARE(first.policies().size(), 0);
  {
    const QString verifyName = QStringLiteral("verify_scoped_columns");
    QSqlDatabase verify =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyName);
    verify.setDatabaseName(path);
    QVERIFY(verify.open());
    QSqlQuery query(verify);
    QVERIFY(query.exec(QStringLiteral(
        "SELECT model_scope,project_scope,service_tier_scope,line_item_scope "
        "FROM observations")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("model-local"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("project-local"));
    QVERIFY(query.value(2).toString().isEmpty());
    QVERIFY(query.value(3).toString().isEmpty());
    verify.close();
    verify = {};
    QSqlDatabase::removeDatabase(verifyName);
  }

  const QVariantMap created = first.createPolicy(validPolicy());
  QVERIFY2(created.value(QStringLiteral("ok")).toBool(),
           qPrintable(created.value(QStringLiteral("error")).toString()));
  const QString id = created.value(QStringLiteral("policy"))
                         .toMap()
                         .value(QStringLiteral("policyId"))
                         .toString();
  QVERIFY(!id.isEmpty());
  QCOMPARE(first.policies().size(), 1);

  const QVariantMap updated =
      first.updatePolicy(id, {{QStringLiteral("limitMinor"), 15000}});
  QVERIFY(updated.value(QStringLiteral("ok")).toBool());
  QCOMPARE(first.policies()
               .first()
               .toMap()
               .value(QStringLiteral("limitMinor"))
               .toLongLong(),
           15000);
  QVERIFY(first.setPolicyEnabled(id, false));
  QVERIFY(!first.policies()
               .first()
               .toMap()
               .value(QStringLiteral("enabled"))
               .toBool());
  QVERIFY(first.snoozePolicy(id, QDateTime::currentDateTimeUtc().addDays(1)));

  QVERIFY(first.duplicatePolicy(id).value(QStringLiteral("ok")).toBool());
  QCOMPARE(first.policies().size(), 2);

  QVariantMap invalid = validPolicy();
  invalid[QStringLiteral("warningPercent")] = 95;
  invalid[QStringLiteral("criticalPercent")] = 90;
  QVERIFY(!first.createPolicy(invalid).value(QStringLiteral("ok")).toBool());
  invalid = validPolicy();
  invalid[QStringLiteral("scopeMode")] = QStringLiteral("scoped");
  QVERIFY(!first.createPolicy(invalid).value(QStringLiteral("ok")).toBool());

  BudgetPolicyRepository second;
  second.setDatabasePath(path);
  second.setOwnerId(QStringLiteral("applet:2"));
  QVERIFY(second.init());
  QCOMPARE(second.policies().size(), 0);
  QVERIFY(second.createPolicy(validPolicy(QStringLiteral("anthropic")))
              .value(QStringLiteral("ok"))
              .toBool());
  QCOMPARE(second.policies().size(), 1);
  QCOMPARE(first.policies().size(), 2);

  BudgetPolicyModel model;
  model.setRepository(&first);
  QCOMPARE(model.rowCount(), 2);
  QCOMPARE(
      model.data(model.index(0), BudgetPolicyModel::SourceIdRole).toString(),
      QStringLiteral("openai"));

  QVERIFY(first.deletePolicy(id));
  QCOMPARE(first.policies().size(), 1);
}

void BudgetPolicyRepositoryTest::
    legacyMigrationIsIdempotentAndDeletionIsPermanent() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  BudgetPolicyRepository repository;
  repository.setDatabasePath(path);
  repository.setOwnerId(QStringLiteral("applet:legacy"));
  QVERIFY(repository.init());

  QVariantMap daily = validPolicy();
  daily[QStringLiteral("legacyKey")] = QStringLiteral("openaiDailyBudget");
  daily[QStringLiteral("periodType")] = QStringLiteral("calendar_day");
  daily[QStringLiteral("limitMinor")] = 500;
  daily[QStringLiteral("warningPercent")] = 85;
  QVariantMap zero = validPolicy();
  zero[QStringLiteral("legacyKey")] = QStringLiteral("openaiMonthlyBudget");
  zero[QStringLiteral("limitMinor")] = 0;

  QVERIFY(repository.migrateLegacyBudgets({daily, zero}));
  QCOMPARE(repository.policies().size(), 1);
  const QVariantMap migrated = repository.policies().first().toMap();
  QCOMPARE(migrated.value(QStringLiteral("timeZoneId")).toString(),
           QStringLiteral("UTC"));
  QCOMPARE(migrated.value(QStringLiteral("criticalPercent")).toInt(), 95);
  const QString id = migrated.value(QStringLiteral("policyId")).toString();
  QVERIFY(repository.migrateLegacyBudgets({daily, zero}));
  QCOMPARE(repository.policies().size(), 1);
  QVERIFY(repository.deletePolicy(id));
  QVERIFY(repository.migrateLegacyBudgets({daily}));
  QCOMPARE(repository.policies().size(), 0);
}

void BudgetPolicyRepositoryTest::replaceIsAtomic() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  BudgetPolicyRepository repository;
  repository.setDatabasePath(path);
  repository.setOwnerId(QStringLiteral("applet:replace"));
  QVERIFY(repository.init());
  QVERIFY(repository.createPolicy(validPolicy())
              .value(QStringLiteral("ok"))
              .toBool());
  const QVariantList before = repository.exportPolicies();

  QVariantMap invalid = validPolicy(QStringLiteral("anthropic"));
  invalid[QStringLiteral("currency")] = QStringLiteral("US");
  QVERIFY(!repository.replacePolicies(
      {validPolicy(QStringLiteral("google")), invalid}));
  QCOMPARE(repository.exportPolicies(), before);

  QVERIFY(
      repository.replacePolicies({validPolicy(QStringLiteral("google")),
                                  validPolicy(QStringLiteral("anthropic"))}));
  QCOMPARE(repository.policies().size(), 2);
}

void BudgetPolicyRepositoryTest::injectedMigrationFailureRollsBack() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  qputenv("PLASMA_AI_MONITOR_INJECT_SCHEMA_V6_FAILURE", "1");
  {
    BudgetPolicyRepository repository;
    repository.setDatabasePath(path);
    repository.setOwnerId(QStringLiteral("applet:failure"));
    QVERIFY(!repository.init());
  }
  qunsetenv("PLASMA_AI_MONITOR_INJECT_SCHEMA_V6_FAILURE");

  const QString name = QStringLiteral("verify_failure");
  {
    QSqlDatabase db =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
    QCOMPARE(query.value(0).toInt(), 5);
    QVERIFY(query.exec(QStringLiteral("SELECT value FROM preserved")) &&
            query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("v5-data"));
    QVERIFY(
        query.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE "
                                  "type='table' AND name='budget_policies'")) &&
        query.next());
    QCOMPARE(query.value(0).toInt(), 0);
    QVERIFY(!query.exec(QStringLiteral(
        "SELECT service_tier_scope,line_item_scope FROM observations")));
    QVERIFY(query.exec(QStringLiteral(
                "SELECT model_scope,project_scope FROM observations")) &&
            query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("model-local"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("project-local"));
    db.close();
  }
  QSqlDatabase::removeDatabase(name);

  BudgetPolicyRepository recovered;
  recovered.setDatabasePath(path);
  recovered.setOwnerId(QStringLiteral("applet:failure"));
  QVERIFY2(recovered.init(), qPrintable(recovered.errorString()));
}

QTEST_MAIN(BudgetPolicyRepositoryTest)
#include "test_budgetpolicyrepository.moc"
