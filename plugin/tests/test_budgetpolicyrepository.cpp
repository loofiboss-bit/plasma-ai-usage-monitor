#include "budgetpolicymodel.h"
#include "budgetpolicyrepository.h"
#include "budgetpolicyschema.h"

#include <QFile>
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

  static void promoteDeliveryFixtureToV7(const QString &path) {
    const auto name = QStringLiteral("promote_delivery_fixture");
    {
      auto database =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
      database.setDatabaseName(path);
      QVERIFY(database.open());
      QSqlQuery query(database);
      QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=7")));
      database.close();
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

  static QVariantMap forecast(const QString &policyId, const QString &state,
                              const QDateTime &start,
                              const QDateTime &generatedAt = {}) {
    return {
        {QStringLiteral("contractVersion"), QStringLiteral("budget-pacing-v2")},
        {QStringLiteral("policyId"), policyId},
        {QStringLiteral("sourceId"), QStringLiteral("openai")},
        {QStringLiteral("kind"), QStringLiteral("budget_overrun")},
        {QStringLiteral("state"), state},
        {QStringLiteral("periodStart"), start},
        {QStringLiteral("periodEnd"), start.addMonths(1)},
        {QStringLiteral("generatedAt"),
         generatedAt.isValid() ? generatedAt : start.addDays(10)},
    };
  }

private Q_SLOTS:
  void deliveryMigrationRollsBackAndRetriesAreBounded();
  void repeatedUpgradeRefreshesRollbackSnapshot();
  void channelCooldownAndUnacceptedTimestampAreIndependent();
  void channelDeliverySurvivesRestartAndIsolatesPolicies();
  void crudDuplicateValidationAndIsolation();
  void legacyMigrationIsIdempotentAndDeletionIsPermanent();
  void replaceIsAtomic();
  void injectedMigrationFailureRollsBack();
  void currencyEditingAndNextPeriodBoundary();
  void transitionsPersistBeforeDeliveryAndDeduplicate();
  void transitionEdgeMatrix();
  void recoveryUnavailableResetAndRestartRules();
  void dndCooldownSnoozeAndFailedDelivery();
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

void BudgetPolicyRepositoryTest::currencyEditingAndNextPeriodBoundary() {
  BudgetPolicyRepository repository;
  repository.setOwnerId(QStringLiteral("applet:editing"));

  const QVariantMap usd = repository.parseMajorAmount(QStringLiteral("12.34"),
                                                      QStringLiteral("USD"));
  QVERIFY(usd.value(QStringLiteral("ok")).toBool());
  QCOMPARE(usd.value(QStringLiteral("minor")).toLongLong(), 1234);
  const QVariantMap large = repository.parseMajorAmount(
      QStringLiteral("123456789012.34"), QStringLiteral("USD"));
  QVERIFY(large.value(QStringLiteral("ok")).toBool());
  QCOMPARE(large.value(QStringLiteral("minor")).toLongLong(),
           qint64(12345678901234));

  const QString formattedUsd =
      repository.formatMinorAmount(1234, QStringLiteral("USD"));
  QCOMPARE(repository.parseMajorAmount(formattedUsd, QStringLiteral("USD"))
               .value(QStringLiteral("minor"))
               .toLongLong(),
           1234);
  const QString formattedJpy =
      repository.formatMinorAmount(1234, QStringLiteral("JPY"));
  QCOMPARE(repository.parseMajorAmount(formattedJpy, QStringLiteral("JPY"))
               .value(QStringLiteral("minor"))
               .toLongLong(),
           1234);
  QVERIFY(repository.formatMinorAmount(100, QStringLiteral("XAU")).isEmpty());
  QCOMPARE(
      repository.parseMajorAmount(QStringLiteral("1"), QStringLiteral("XAU"))
          .value(QStringLiteral("error"))
          .toString(),
      QStringLiteral("unknown-currency"));

  QVariantMap daily = validPolicy();
  daily[QStringLiteral("periodType")] = QStringLiteral("calendar_day");
  daily[QStringLiteral("timeZoneId")] = QStringLiteral("Europe/Stockholm");
  const QDateTime generatedAt = QDateTime::fromString(
      QStringLiteral("2026-03-29T00:30:00Z"), Qt::ISODate);
  QCOMPARE(repository.nextPeriodStart(daily, generatedAt),
           QDateTime::fromString(QStringLiteral("2026-03-29T22:00:00Z"),
                                 Qt::ISODate));

  QVariantMap reset = daily;
  reset[QStringLiteral("periodType")] = QStringLiteral("provider_reset");
  QVERIFY(!repository.nextPeriodStart(reset, generatedAt).isValid());
}

void BudgetPolicyRepositoryTest::
    transitionsPersistBeforeDeliveryAndDeduplicate() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  BudgetPolicyRepository repository;
  repository.setDatabasePath(path);
  repository.setOwnerId(QStringLiteral("applet:transitions"));
  QVERIFY(repository.init());
  const QVariantMap created = repository.createPolicy(validPolicy());
  QVERIFY(created.value(QStringLiteral("ok")).toBool());
  const QString policyId = created.value(QStringLiteral("policy"))
                               .toMap()
                               .value(QStringLiteral("policyId"))
                               .toString();
  const QDateTime start(QDate(2026, 8, 1), QTime(0, 0), QTimeZone::utc());

  const QVariantMap warning = repository.prepareTransitions(
      forecast(policyId, QStringLiteral("warning"), start));
  QVERIFY2(warning.value(QStringLiteral("ok")).toBool(),
           qPrintable(warning.value(QStringLiteral("error")).toString()));
  const QVariantList warningEvents =
      warning.value(QStringLiteral("events")).toList();
  QCOMPARE(warningEvents.size(), 1);
  const QVariantMap warningEvent = warningEvents.first().toMap();
  QCOMPARE(warningEvent.value(QStringLiteral("transition")).toString(),
           QStringLiteral("warning"));
  QCOMPARE(warningEvent.value(QStringLiteral("deliveryStatus")).toString(),
           QStringLiteral("pending"));
  QVERIFY(warningEvent.value(QStringLiteral("deliver")).toBool());

  const QString verifyName = QStringLiteral("verify_persist_before_delivery");
  {
    QSqlDatabase verify =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyName);
    verify.setDatabaseName(path);
    QVERIFY(verify.open());
    QSqlQuery query(verify);
    QVERIFY(query.exec(QStringLiteral(
        "SELECT e.delivery_status,s.risk_level FROM budget_policy_events e "
        "JOIN budget_policy_state s ON s.policy_id=e.policy_id")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("pending"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("warning"));
    verify.close();
  }
  QSqlDatabase::removeDatabase(verifyName);

  QVERIFY(repository.markEventDelivered(
      warningEvent.value(QStringLiteral("eventId")).toLongLong()));
  QVERIFY(repository.lastDeliveredAt(policyId).isValid());
  QCOMPARE(repository
               .prepareTransitions(
                   forecast(policyId, QStringLiteral("warning"), start))
               .value(QStringLiteral("events"))
               .toList()
               .size(),
           0);

  for (const QString &risk :
       {QStringLiteral("critical"), QStringLiteral("exceeded")}) {
    const QVariantList events =
        repository.prepareTransitions(forecast(policyId, risk, start))
            .value(QStringLiteral("events"))
            .toList();
    QCOMPARE(events.size(), 1);
    QCOMPARE(
        events.first().toMap().value(QStringLiteral("transition")).toString(),
        risk);
    QVERIFY(repository.markEventDelivered(
        events.first().toMap().value(QStringLiteral("eventId")).toLongLong()));
  }
}

void BudgetPolicyRepositoryTest::transitionEdgeMatrix() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  BudgetPolicyRepository repository;
  repository.setDatabasePath(path);
  repository.setOwnerId(QStringLiteral("applet:edge-matrix"));
  QVERIFY(repository.init());
  const QDateTime start(QDate(2026, 5, 1), QTime(0, 0), QTimeZone::utc());
  const QStringList states = {QStringLiteral("unavailable"),
                              QStringLiteral("safe"), QStringLiteral("warning"),
                              QStringLiteral("critical"),
                              QStringLiteral("exceeded")};
  const auto risky = [](const QString &state) {
    return state == QLatin1String("warning") ||
           state == QLatin1String("critical") ||
           state == QLatin1String("exceeded");
  };

  for (const QString &previousState : states) {
    for (const QString &currentState : states) {
      const QVariantMap created = repository.createPolicy(validPolicy());
      QVERIFY(created.value(QStringLiteral("ok")).toBool());
      const QString policyId = created.value(QStringLiteral("policy"))
                                   .toMap()
                                   .value(QStringLiteral("policyId"))
                                   .toString();
      QVariantList events =
          repository
              .prepareTransitions(forecast(policyId, previousState, start))
              .value(QStringLiteral("events"))
              .toList();
      for (const QVariant &event : events) {
        QVERIFY(repository.markEventDelivered(
            event.toMap().value(QStringLiteral("eventId")).toLongLong()));
      }

      events =
          repository.prepareTransitions(forecast(policyId, currentState, start))
              .value(QStringLiteral("events"))
              .toList();
      QString expected;
      if (risky(currentState) && currentState != previousState)
        expected = currentState;
      else if (currentState == QLatin1String("safe") && risky(previousState))
        expected = QStringLiteral("recovered");
      QCOMPARE(events.size(), expected.isEmpty() ? 0 : 1);
      if (!expected.isEmpty()) {
        QCOMPARE(events.first()
                     .toMap()
                     .value(QStringLiteral("transition"))
                     .toString(),
                 expected);
      }
    }
  }
}

void BudgetPolicyRepositoryTest::recoveryUnavailableResetAndRestartRules() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  const QDateTime firstStart(QDate(2026, 6, 1), QTime(0, 0), QTimeZone::utc());
  QString policyId;
  {
    BudgetPolicyRepository repository;
    repository.setDatabasePath(path);
    repository.setOwnerId(QStringLiteral("applet:recovery"));
    QVERIFY(repository.init());
    const QVariantMap created = repository.createPolicy(validPolicy());
    policyId = created.value(QStringLiteral("policy"))
                   .toMap()
                   .value(QStringLiteral("policyId"))
                   .toString();

    QVariantList events =
        repository
            .prepareTransitions(
                forecast(policyId, QStringLiteral("warning"), firstStart))
            .value(QStringLiteral("events"))
            .toList();
    QVERIFY2(!events.isEmpty(), qPrintable(repository.errorString()));
    QVERIFY(repository.markEventDelivered(
        events.first().toMap().value(QStringLiteral("eventId")).toLongLong()));
    events = repository
                 .prepareTransitions(forecast(
                     policyId, QStringLiteral("unavailable"), firstStart))
                 .value(QStringLiteral("events"))
                 .toList();
    QCOMPARE(events.size(), 0);
    events = repository
                 .prepareTransitions(
                     forecast(policyId, QStringLiteral("safe"), firstStart))
                 .value(QStringLiteral("events"))
                 .toList();
    QCOMPARE(events.size(), 0);

    events = repository
                 .prepareTransitions(
                     forecast(policyId, QStringLiteral("critical"), firstStart))
                 .value(QStringLiteral("events"))
                 .toList();
    QVERIFY(repository.markEventDelivered(
        events.first().toMap().value(QStringLiteral("eventId")).toLongLong()));
    events = repository
                 .prepareTransitions(
                     forecast(policyId, QStringLiteral("safe"), firstStart))
                 .value(QStringLiteral("events"))
                 .toList();
    QCOMPARE(events.size(), 1);
    QCOMPARE(
        events.first().toMap().value(QStringLiteral("transition")).toString(),
        QStringLiteral("recovered"));
    QVERIFY(repository.markEventDelivered(
        events.first().toMap().value(QStringLiteral("eventId")).toLongLong()));

    const QDateTime secondStart = firstStart.addMonths(1);
    events = repository
                 .prepareTransitions(
                     forecast(policyId, QStringLiteral("safe"), secondStart))
                 .value(QStringLiteral("events"))
                 .toList();
    QCOMPARE(events.size(), 1);
    QCOMPARE(
        events.first().toMap().value(QStringLiteral("transition")).toString(),
        QStringLiteral("period_reset"));
    QVERIFY(repository.markEventDelivered(
        events.first().toMap().value(QStringLiteral("eventId")).toLongLong()));
  }

  BudgetPolicyRepository restarted;
  restarted.setDatabasePath(path);
  restarted.setOwnerId(QStringLiteral("applet:recovery"));
  QVERIFY(restarted.init());
  QCOMPARE(restarted
               .prepareTransitions(forecast(policyId, QStringLiteral("safe"),
                                            firstStart.addMonths(1)))
               .value(QStringLiteral("events"))
               .toList()
               .size(),
           0);
}

void BudgetPolicyRepositoryTest::dndCooldownSnoozeAndFailedDelivery() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("usage.db"));
  seedV5(path);
  const QDate today = QDateTime::currentDateTimeUtc().date();
  const QDateTime firstStart(QDate(today.year(), today.month(), 1), QTime(0, 0),
                             QTimeZone::utc());
  QString policyId;
  qint64 pendingId = 0;
  {
    BudgetPolicyRepository repository;
    repository.setDatabasePath(path);
    repository.setOwnerId(QStringLiteral("applet:suppression"));
    QVERIFY(repository.init());
    const QVariantMap created = repository.createPolicy(validPolicy());
    policyId = created.value(QStringLiteral("policy"))
                   .toMap()
                   .value(QStringLiteral("policyId"))
                   .toString();
    QVariantList events =
        repository
            .prepareTransitions(
                forecast(policyId, QStringLiteral("warning"), firstStart),
                QStringLiteral("dnd"))
            .value(QStringLiteral("events"))
            .toList();
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first()
                 .toMap()
                 .value(QStringLiteral("deliveryStatus"))
                 .toString(),
             QStringLiteral("pending"));
    QVERIFY(!events.first().toMap().value(QStringLiteral("deliver")).toBool());
    pendingId =
        events.first().toMap().value(QStringLiteral("eventId")).toLongLong();
  }

  BudgetPolicyRepository restarted;
  restarted.setDatabasePath(path);
  restarted.setOwnerId(QStringLiteral("applet:suppression"));
  QVERIFY(restarted.init());
  QVariantList events =
      restarted
          .prepareTransitions(
              forecast(policyId, QStringLiteral("warning"), firstStart))
          .value(QStringLiteral("events"))
          .toList();
  QCOMPARE(events.size(), 1);
  QCOMPARE(events.first().toMap().value(QStringLiteral("eventId")).toLongLong(),
           pendingId);
  QVERIFY(events.first().toMap().value(QStringLiteral("deliver")).toBool());
  QVERIFY(restarted.markEventDelivered(pendingId));

  events = restarted
               .prepareTransitions(
                   forecast(policyId, QStringLiteral("critical"), firstStart),
                   QStringLiteral("cooldown"))
               .value(QStringLiteral("events"))
               .toList();
  QCOMPARE(events.size(), 1);
  QCOMPARE(
      events.first().toMap().value(QStringLiteral("deliveryStatus")).toString(),
      QStringLiteral("suppressed"));
  QVERIFY(!events.first().toMap().value(QStringLiteral("deliver")).toBool());

  const QDateTime nextStart = firstStart.addMonths(1);
  QVERIFY(restarted.snoozePolicy(policyId, nextStart));
  events =
      restarted
          .prepareTransitions(forecast(policyId, QStringLiteral("exceeded"),
                                       firstStart, firstStart.addDays(20)))
          .value(QStringLiteral("events"))
          .toList();
  QCOMPARE(events.size(), 1);
  QCOMPARE(events.first().toMap().value(QStringLiteral("reasonKey")).toString(),
           QStringLiteral("snoozed"));

  events = restarted
               .prepareTransitions(forecast(policyId, QStringLiteral("warning"),
                                            nextStart, nextStart))
               .value(QStringLiteral("events"))
               .toList();
  QCOMPARE(events.size(), 2);
  for (const QVariant &value : events)
    QVERIFY(value.toMap().value(QStringLiteral("deliver")).toBool());
  const qint64 failedId =
      events.last().toMap().value(QStringLiteral("eventId")).toLongLong();
  QVERIFY(
      restarted.markEventFailed(failedId, QStringLiteral("injected-failure")));
  QVERIFY(!restarted.policies()
               .first()
               .toMap()
               .value(QStringLiteral("snoozedUntilUtc"))
               .toDateTime()
               .isValid());
  QCOMPARE(restarted
               .prepareTransitions(forecast(policyId, QStringLiteral("warning"),
                                            nextStart, nextStart.addSecs(1)))
               .value(QStringLiteral("events"))
               .toList()
               .size(),
           1);
  // Only the still-pending period-reset event is retried; the failed risk
  // event remains terminal and is never duplicated.

  const QVariantMap secondCreated = restarted.createPolicy(validPolicy());
  const QString secondPolicyId = secondCreated.value(QStringLiteral("policy"))
                                     .toMap()
                                     .value(QStringLiteral("policyId"))
                                     .toString();
  events =
      restarted
          .prepareTransitions(
              forecast(secondPolicyId, QStringLiteral("warning"), firstStart),
              QStringLiteral("dnd"))
          .value(QStringLiteral("events"))
          .toList();
  QCOMPARE(events.size(), 1);
  const qint64 staleWarningId =
      events.first().toMap().value(QStringLiteral("eventId")).toLongLong();
  events =
      restarted
          .prepareTransitions(
              forecast(secondPolicyId, QStringLiteral("critical"), firstStart),
              QStringLiteral("dnd"))
          .value(QStringLiteral("events"))
          .toList();
  QCOMPARE(events.size(), 1);
  QCOMPARE(
      events.first().toMap().value(QStringLiteral("transition")).toString(),
      QStringLiteral("critical"));
  events = restarted
               .prepareTransitions(forecast(
                   secondPolicyId, QStringLiteral("critical"), firstStart))
               .value(QStringLiteral("events"))
               .toList();
  QCOMPARE(events.size(), 1);
  QCOMPARE(
      events.first().toMap().value(QStringLiteral("transition")).toString(),
      QStringLiteral("critical"));

  const QString verifyName = QStringLiteral("verify_dnd_supersession");
  {
    QSqlDatabase verify =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyName);
    verify.setDatabaseName(path);
    QVERIFY(verify.open());
    QSqlQuery query(verify);
    query.prepare(QStringLiteral(
        "SELECT delivery_status,reason_key FROM budget_policy_events "
        "WHERE id=?"));
    query.addBindValue(staleWarningId);
    QVERIFY(query.exec() && query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("suppressed"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("superseded"));
    verify.close();
  }
  QSqlDatabase::removeDatabase(verifyName);
}

void BudgetPolicyRepositoryTest::
    channelDeliverySurvivesRestartAndIsolatesPolicies() {
  QTemporaryDir dir;
  const auto path = dir.filePath(QStringLiteral("channels.db"));
  seedV5(path);
  qint64 eventId = 0;
  {
    BudgetPolicyRepository repository;
    repository.setOwnerId(QStringLiteral("applet:channels"));
    repository.setDatabasePath(path);
    QVERIFY(repository.init());
    promoteDeliveryFixtureToV7(path);
    const auto policy = repository.createPolicy(validPolicy());
    QVERIFY(policy.value(QStringLiteral("ok")).toBool());
    const auto now = QDateTime::currentDateTimeUtc();
    auto prepared = repository.prepareTransitions(
        forecast(policy.value(QStringLiteral("policy"))
                     .toMap()
                     .value(QStringLiteral("policyId"))
                     .toString(),
                 QStringLiteral("warning"), now.addDays(-1), now));
    QVERIFY(prepared.value(QStringLiteral("ok")).toBool());
    QCOMPARE(prepared.value(QStringLiteral("events")).toList().size(), 1);
    eventId = prepared.value(QStringLiteral("events"))
                  .toList()
                  .first()
                  .toMap()
                  .value(QStringLiteral("eventId"))
                  .toLongLong();
    const QStringList channels{QStringLiteral("kde"), QStringLiteral("slack"),
                               QStringLiteral("discord")};
    QCOMPARE(repository.prepareChannelDeliveries(eventId, channels, 15).size(),
             3);
    QVERIFY(repository.completeChannelDelivery(eventId, QStringLiteral("kde"),
                                               true, false, {}, 0, 15));
    QVERIFY(repository.completeChannelDelivery(
        eventId, QStringLiteral("slack"), false, true,
        QStringLiteral("rate-limited"), 1200, 15));
    QVERIFY(repository.completeChannelDelivery(
        eventId, QStringLiteral("discord"), false, false,
        QStringLiteral("webhook-rejected"), 0, 15));
    QVERIFY(
        repository.prepareChannelDeliveries(eventId, channels, 15).isEmpty());
    for (const auto &entry : repository.deliveryStatus()) {
      const auto receipt = entry.toMap();
      if (receipt.value(QStringLiteral("channel")) == QStringLiteral("slack")) {
        const auto next =
            receipt.value(QStringLiteral("nextAttemptAt")).toDateTime();
        QVERIFY(next >= now.addSecs(1200));
      }
    }
    const auto second = repository.createPolicy(validPolicy());
    QVERIFY(second.value(QStringLiteral("ok")).toBool());
    prepared = repository.prepareTransitions(
        forecast(second.value(QStringLiteral("policy"))
                     .toMap()
                     .value(QStringLiteral("policyId"))
                     .toString(),
                 QStringLiteral("warning"), now.addDays(-1), now));
    QVERIFY(prepared.value(QStringLiteral("ok")).toBool());
    QCOMPARE(prepared.value(QStringLiteral("events")).toList().size(), 1);
    const auto secondId = prepared.value(QStringLiteral("events"))
                              .toList()
                              .first()
                              .toMap()
                              .value(QStringLiteral("eventId"))
                              .toLongLong();
    QCOMPARE(repository.prepareChannelDeliveries(secondId, channels, 15).size(),
             3);
  }
  BudgetPolicyRepository reopened;
  reopened.setOwnerId(QStringLiteral("applet:channels"));
  reopened.setDatabasePath(path);
  QVERIFY(reopened.init());
  QVERIFY(reopened
              .prepareChannelDeliveries(eventId,
                                        {QStringLiteral("kde"),
                                         QStringLiteral("slack"),
                                         QStringLiteral("discord")},
                                        15)
              .isEmpty());
  const auto statuses = reopened.deliveryStatus();
  bool kdeDelivered = false, slackPending = false;
  for (const auto &value : statuses) {
    const auto row = value.toMap();
    if (row.value(QStringLiteral("eventId")).toLongLong() != eventId)
      continue;
    if (row.value(QStringLiteral("channel")) == QStringLiteral("kde"))
      kdeDelivered =
          row.value(QStringLiteral("status")) == QStringLiteral("delivered");
    if (row.value(QStringLiteral("channel")) == QStringLiteral("slack"))
      slackPending =
          row.value(QStringLiteral("status")) == QStringLiteral("pending");
  }
  QVERIFY(kdeDelivered);
  QVERIFY(slackPending);
  // Disabling Slack suppresses its pending retry without resending KDE.
  QVERIFY(
      reopened
          .prepareChannelDeliveries(
              eventId, {QStringLiteral("kde"), QStringLiteral("discord")}, 15)
          .isEmpty());
}

void BudgetPolicyRepositoryTest::
    deliveryMigrationRollsBackAndRetriesAreBounded() {
  QTemporaryDir dir;
  const auto path = dir.filePath(QStringLiteral("migration.db"));
  seedV5(path);
  BudgetPolicyRepository repository;
  repository.setDatabasePath(path);
  repository.setOwnerId(QStringLiteral("applet:bounded"));
  QVERIFY(repository.init());
  const QString name = QStringLiteral("verify_delivery_migration");
  {
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(path);
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=7")));
    QString error;
    QVERIFY(!BudgetPolicySchema::migrateDeliverySchema(database, &error, true));
    QVERIFY(!database.tables().contains(
        QStringLiteral("budget_policy_deliveries")));
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 7);
    query.finish();
    QVERIFY(BudgetPolicySchema::migrateDeliverySchema(database, &error));
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=9")));
    QVERIFY(!BudgetPolicySchema::migrateDeliverySchema(database, &error));
    {
      BudgetPolicyRepository future;
      future.setDatabasePath(path);
      future.setOwnerId(QStringLiteral("applet:future"));
      QVERIFY(!future.init());
    }
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=8")));

    query.finish();
    const auto policy = repository.createPolicy(validPolicy());
    QVERIFY2(policy.value(QStringLiteral("ok")).toBool(),
             qPrintable(policy.value(QStringLiteral("error")).toString()));
    const auto now = QDateTime::currentDateTimeUtc();
    auto prepared = repository.prepareTransitions(
        forecast(policy.value(QStringLiteral("policy"))
                     .toMap()
                     .value(QStringLiteral("policyId"))
                     .toString(),
                 QStringLiteral("warning"), now.addDays(-1), now));
    QVERIFY2(prepared.value(QStringLiteral("ok")).toBool(),
             qPrintable(prepared.value(QStringLiteral("error")).toString()));
    QCOMPARE(prepared.value(QStringLiteral("events")).toList().size(), 1);
    auto id = prepared.value(QStringLiteral("events"))
                  .toList()
                  .first()
                  .toMap()
                  .value(QStringLiteral("eventId"))
                  .toLongLong();
    for (int attempt = 0; attempt < 3; ++attempt) {
      QCOMPARE(
          repository.prepareChannelDeliveries(id, {QStringLiteral("slack")}, 15)
              .size(),
          1);
      QVERIFY(repository.completeChannelDelivery(
          id, QStringLiteral("slack"), false, true,
          QStringLiteral("server-error"), 0, 15));
      QVERIFY(query.exec(
          QStringLiteral("UPDATE budget_policy_deliveries SET "
                         "next_attempt_at_utc='2000-01-01T00:00:00.000Z'")));
    }
    QVERIFY(
        repository.prepareChannelDeliveries(id, {QStringLiteral("slack")}, 15)
            .isEmpty());
    QCOMPARE(repository.deliveryStatus()
                 .first()
                 .toMap()
                 .value(QStringLiteral("attempts"))
                 .toInt(),
             3);
    QCOMPARE(repository.deliveryStatus()
                 .first()
                 .toMap()
                 .value(QStringLiteral("status"))
                 .toString(),
             QStringLiteral("failed"));
    database.close();
  }
  QSqlDatabase::removeDatabase(name);
}

void BudgetPolicyRepositoryTest::
    channelCooldownAndUnacceptedTimestampAreIndependent() {
  QTemporaryDir dir;
  const auto path = dir.filePath(QStringLiteral("cooldowns.db"));
  seedV5(path);
  BudgetPolicyRepository repository;
  repository.setOwnerId(QStringLiteral("applet:cooldown"));
  repository.setDatabasePath(path);
  QVERIFY(repository.init());
  promoteDeliveryFixtureToV7(path);
  const auto created = repository.createPolicy(validPolicy());
  QVERIFY(created.value(QStringLiteral("ok")).toBool());
  const auto policyId = created.value(QStringLiteral("policy"))
                            .toMap()
                            .value(QStringLiteral("policyId"))
                            .toString();
  const auto now = QDateTime::currentDateTimeUtc();
  auto prepared = repository.prepareTransitions(
      forecast(policyId, QStringLiteral("warning"), now.addDays(-1), now));
  QVERIFY(prepared.value(QStringLiteral("ok")).toBool());
  QCOMPARE(prepared.value(QStringLiteral("events")).toList().size(), 1);
  const auto warningId = prepared.value(QStringLiteral("events"))
                             .toList()
                             .first()
                             .toMap()
                             .value(QStringLiteral("eventId"))
                             .toLongLong();
  QCOMPARE(
      repository
          .prepareChannelDeliveries(warningId, {QStringLiteral("kde")}, 0, 15)
          .size(),
      1);
  QVERIFY(repository.completeChannelDelivery(warningId, QStringLiteral("kde"),
                                             true, false, {}, 0, 15));
  prepared = repository.prepareTransitions(
      forecast(policyId, QStringLiteral("critical"), now.addDays(-1), now));
  QVERIFY(prepared.value(QStringLiteral("ok")).toBool());
  QCOMPARE(prepared.value(QStringLiteral("events")).toList().size(), 1);
  const auto criticalId = prepared.value(QStringLiteral("events"))
                              .toList()
                              .first()
                              .toMap()
                              .value(QStringLiteral("eventId"))
                              .toLongLong();
  auto due = repository.prepareChannelDeliveries(
      criticalId, {QStringLiteral("kde"), QStringLiteral("slack")}, 0, 15);
  QCOMPARE(due.size(), 1);
  QCOMPARE(due.first().toMap().value(QStringLiteral("channel")).toString(),
           QStringLiteral("slack"));
  QVERIFY(repository
              .prepareChannelDeliveries(criticalId, {QStringLiteral("slack")},
                                        0, 15)
              .isEmpty());
  QVERIFY(repository.completeChannelDelivery(
      criticalId, QStringLiteral("slack"), false, false,
      QStringLiteral("webhook-rejected"), 0, 0));
  const auto name = QStringLiteral("verify_no_unaccepted_timestamp");
  {
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(path);
    QVERIFY(database.open());
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT delivery_status,delivered_at_utc FROM "
                                 "budget_policy_events WHERE id=?"));
    query.addBindValue(criticalId);
    QVERIFY(query.exec() && query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("failed"));
    QVERIFY(query.value(1).isNull());
    database.close();
  }
  QSqlDatabase::removeDatabase(name);
}

void BudgetPolicyRepositoryTest::repeatedUpgradeRefreshesRollbackSnapshot() {
  QTemporaryDir dir;
  const auto path = dir.filePath(QStringLiteral("reupgrade.db"));
  const auto backupPath = path + QStringLiteral(".v21-backup");
  seedV5(path);
  {
    BudgetPolicyRepository repository;
    repository.setOwnerId(QStringLiteral("applet:rollback"));
    repository.setDatabasePath(path);
    QVERIFY(repository.init());
  }
  promoteDeliveryFixtureToV7(path);
  const auto name = QStringLiteral("upgrade_rollback_snapshot");
  {
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(path);
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("PRAGMA journal_mode=WAL")));
    query.finish();
    QString error;
    QVERIFY2(BudgetPolicySchema::migrateDeliverySchema(database, &error),
             qPrintable(error));
    database.close();
  }
  QSqlDatabase::removeDatabase(name);
  // Restore the v7 backup rather than opening the migrated database with V21.
  QVERIFY(QFile::remove(path));
  QVERIFY(QFile::copy(backupPath, path));
  {
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(path);
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
    QCOMPARE(query.value(0).toInt(), 7);
    query.finish();
    QVERIFY(query.exec(QStringLiteral("PRAGMA journal_mode=WAL")));
    query.finish();
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO preserved(value) VALUES('post-rollback-history')")));
    QString error;
    QVERIFY2(BudgetPolicySchema::migrateDeliverySchema(database, &error),
             qPrintable(error));
    database.close();
  }
  QSqlDatabase::removeDatabase(name);
  {
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(backupPath);
    database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
    QCOMPARE(query.value(0).toInt(), 7);
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM preserved WHERE "
                                      "value='post-rollback-history'")) &&
            query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(!database.tables().contains(
        QStringLiteral("budget_policy_deliveries")));
    database.close();
  }
  QSqlDatabase::removeDatabase(name);
}

QTEST_MAIN(BudgetPolicyRepositoryTest)
#include "test_budgetpolicyrepository.moc"
