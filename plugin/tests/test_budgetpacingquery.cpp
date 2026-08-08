#include "billingcycleresolver.h"
#include "budgetobservationquery.h"
#include "budgetpacingquery.h"
#include "currencyminorunits.h"

#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace {
const QDateTime Now(QDate(2026, 7, 11), QTime(12, 0), QTimeZone::UTC);

BillingCycleResolver::Cycle monthCycle(int month = 7) {
  return {QDate(2026, month, 1).startOfDay(QTimeZone::UTC),
          QDate(2026, month, 1).addMonths(1).startOfDay(QTimeZone::UTC),
          {}};
}

BudgetPacingQuery::Observation
observation(const QDate &date, qint64 value,
            const QString &currency = QStringLiteral("USD"),
            ForecastContract::ValueClass valueClass =
                ForecastContract::ValueClass::Actual) {
  BudgetPacingQuery::Observation row;
  row.intervalStart = date.startOfDay(QTimeZone::UTC);
  row.intervalEnd = row.intervalStart.addDays(1);
  row.observedAt = row.intervalStart.addSecs(23 * 60 * 60);
  row.valueMinor = value;
  row.currency = currency;
  row.valueClass = valueClass;
  return row;
}

BudgetPacingQuery::Request request() {
  BudgetPacingQuery::Request value;
  value.policyId = QStringLiteral("11111111-1111-4111-8111-111111111111");
  value.sourceId = QStringLiteral("openai");
  value.sourceKind = QStringLiteral("provider");
  value.limitMinor = 100000;
  value.currency = QStringLiteral("USD");
  value.warningPercent = 80;
  value.criticalPercent = 90;
  value.timeZoneId = QStringLiteral("UTC");
  value.cycle = monthCycle();
  value.previousCycle = monthCycle(6);
  value.generatedAt = Now;
  for (int day = 1; day <= 10; ++day)
    value.observations.append(observation(QDate(2026, 7, day), 1000));
  BudgetPacingQuery::Observation today = observation(QDate(2026, 7, 11), 500);
  today.observedAt = Now;
  value.observations.append(today);
  for (int day = 1; day <= 30; ++day)
    value.observations.append(observation(QDate(2026, 6, day), 800));
  return value;
}
} // namespace

class BudgetPacingQueryTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void calendarCycles_data();
  void calendarCycles();
  void anchorsCoverEveryMonthAndLeapYear();
  void dstBoundaries();
  void providerResetRequiresStableAuthenticatedContract();
  void minorUnitTable();
  void pacingFieldsAndRiskRules();
  void unavailablePrecedenceAndCompatibility();
  void scopedAndPreviousPeriodComparison();
  void databaseBoundaryAndUnknownCurrency();
  void providerScopeDimensionsAtDatabaseBoundary();
  void performanceAtOneHundredThousandObservations();
};

void BudgetPacingQueryTest::calendarCycles_data() {
  QTest::addColumn<QString>("period");
  QTest::addColumn<int>("anchor");
  QTest::addColumn<QDateTime>("expectedStart");
  QTest::addColumn<QDateTime>("expectedEnd");
  QTest::newRow("day") << QStringLiteral("calendar_day") << 0
                       << QDate(2026, 7, 11).startOfDay(QTimeZone::UTC)
                       << QDate(2026, 7, 12).startOfDay(QTimeZone::UTC);
  QTest::newRow("week") << QStringLiteral("iso_week") << 0
                        << QDate(2026, 7, 6).startOfDay(QTimeZone::UTC)
                        << QDate(2026, 7, 13).startOfDay(QTimeZone::UTC);
  QTest::newRow("month") << QStringLiteral("calendar_month") << 0
                         << QDate(2026, 7, 1).startOfDay(QTimeZone::UTC)
                         << QDate(2026, 8, 1).startOfDay(QTimeZone::UTC);
  QTest::newRow("anchor-1") << QStringLiteral("anchored_month") << 1
                            << QDate(2026, 7, 1).startOfDay(QTimeZone::UTC)
                            << QDate(2026, 8, 1).startOfDay(QTimeZone::UTC);
  QTest::newRow("anchor-28") << QStringLiteral("anchored_month") << 28
                             << QDate(2026, 6, 28).startOfDay(QTimeZone::UTC)
                             << QDate(2026, 7, 28).startOfDay(QTimeZone::UTC);
}

void BudgetPacingQueryTest::calendarCycles() {
  QFETCH(QString, period);
  QFETCH(int, anchor);
  QFETCH(QDateTime, expectedStart);
  QFETCH(QDateTime, expectedEnd);
  BillingCycleResolver::Request request;
  request.periodType = period;
  request.anchorDay = anchor;
  request.timeZoneId = QStringLiteral("UTC");
  request.generatedAt = Now;
  const auto cycle = BillingCycleResolver::resolve(request);
  QVERIFY(cycle.isValid());
  QCOMPARE(cycle.startUtc, expectedStart);
  QCOMPARE(cycle.endUtc, expectedEnd);
  const auto previous = BillingCycleResolver::previous(request, cycle);
  QVERIFY(previous.isValid());
  QCOMPARE(previous.endUtc, cycle.startUtc);
}

void BudgetPacingQueryTest::anchorsCoverEveryMonthAndLeapYear() {
  for (int year : {2024, 2025, 2026}) {
    for (int month = 1; month <= 12; ++month) {
      BillingCycleResolver::Request calendarMonth;
      calendarMonth.periodType = QStringLiteral("calendar_month");
      calendarMonth.timeZoneId = QStringLiteral("UTC");
      calendarMonth.generatedAt =
          QDate(year, month, 15).startOfDay(QTimeZone::UTC).addSecs(3600);
      const auto calendarCycle = BillingCycleResolver::resolve(calendarMonth);
      QVERIFY(calendarCycle.isValid());
      QCOMPARE(
          calendarCycle.startUtc.date().daysTo(calendarCycle.endUtc.date()),
          QDate(year, month, 1).daysInMonth());
      for (int anchor : {1, 28}) {
        const QDate localDate(
            year, month, qMin(anchor + 1, QDate(year, month, 1).daysInMonth()));
        BillingCycleResolver::Request request;
        request.periodType = QStringLiteral("anchored_month");
        request.anchorDay = anchor;
        request.timeZoneId = QStringLiteral("UTC");
        request.generatedAt =
            localDate.startOfDay(QTimeZone::UTC).addSecs(3600);
        const auto cycle = BillingCycleResolver::resolve(request);
        QVERIFY2(cycle.isValid(), qPrintable(localDate.toString(Qt::ISODate)));
        QCOMPARE(cycle.startUtc.date().day(), anchor);
        QCOMPARE(cycle.endUtc.date().day(), anchor);
        QCOMPARE(cycle.startUtc.date().addMonths(1), cycle.endUtc.date());
      }
    }
  }
}

void BudgetPacingQueryTest::dstBoundaries() {
  struct Row {
    QByteArray zone;
    QDate date;
    int hours;
  };
  const QList<Row> rows = {{"Europe/Stockholm", QDate(2026, 3, 29), 23},
                           {"Europe/Stockholm", QDate(2026, 10, 25), 25},
                           {"America/New_York", QDate(2026, 3, 8), 23},
                           {"America/New_York", QDate(2026, 11, 1), 25}};
  for (const Row &row : rows) {
    const QTimeZone zone(row.zone);
    BillingCycleResolver::Request request;
    request.periodType = QStringLiteral("calendar_day");
    request.timeZoneId = QString::fromUtf8(row.zone);
    request.generatedAt = QDateTime(row.date, QTime(12, 0), zone).toUTC();
    const auto cycle = BillingCycleResolver::resolve(request);
    QVERIFY(cycle.isValid());
    QCOMPARE(cycle.startUtc.secsTo(cycle.endUtc), qint64(row.hours * 60 * 60));
  }
}

void BudgetPacingQueryTest::providerResetRequiresStableAuthenticatedContract() {
  BillingCycleResolver::Request request;
  request.periodType = QStringLiteral("provider_reset");
  request.generatedAt = Now;
  request.providerPeriodStart = Now.addDays(-2);
  request.providerResetAt = Now.addDays(5);
  QCOMPARE(BillingCycleResolver::resolve(request).reasonKey,
           QStringLiteral("unstable-reset"));
  request.providerResetStable = true;
  request.providerResetAuthenticated = true;
  request.catalogSupportsProviderReset = true;
  const auto cycle = BillingCycleResolver::resolve(request);
  QVERIFY(cycle.isValid());
  QCOMPARE(cycle.startUtc, Now.addDays(-2));
  QCOMPARE(cycle.endUtc, Now.addDays(5));
}

void BudgetPacingQueryTest::minorUnitTable() {
  QCOMPARE(CurrencyMinorUnits::digits(QStringLiteral("USD")),
           std::optional<int>(2));
  QCOMPARE(CurrencyMinorUnits::digits(QStringLiteral("JPY")),
           std::optional<int>(0));
  QCOMPARE(CurrencyMinorUnits::digits(QStringLiteral("KWD")),
           std::optional<int>(3));
  QCOMPARE(CurrencyMinorUnits::digits(QStringLiteral("CLF")),
           std::optional<int>(4));
  QVERIFY(!CurrencyMinorUnits::digits(QStringLiteral("XAU")));
  QCOMPARE(CurrencyMinorUnits::fromMajor(12.345, QStringLiteral("USD")),
           std::optional<qint64>(1235));
  QCOMPARE(CurrencyMinorUnits::fromMajor(12.5, QStringLiteral("JPY")),
           std::optional<qint64>(13));
}

void BudgetPacingQueryTest::pacingFieldsAndRiskRules() {
  BudgetPacingQuery::Request value = request();
  ForecastContract::Result result = BudgetPacingQuery::evaluate(value);
  QVERIFY(result.isValid());
  QCOMPARE(result.contractVersion, QStringLiteral("budget-pacing-v2"));
  QCOMPARE(result.state, ForecastContract::State::Safe);
  QCOMPARE(result.spentMinor, std::optional<qint64>(10500));
  QCOMPARE(result.remainingMinor, std::optional<qint64>(89500));
  QCOMPARE(result.remainingDailyAllowanceMinor, std::optional<qint64>(4261));
  QCOMPARE(result.sampleCount, 40);
  QVERIFY(result.safeTodayMinor && *result.safeTodayMinor >= 0);
  QVERIFY(result.projectedPeriodEndMinor &&
          *result.projectedPeriodEndMinor > *result.spentMinor);
  QVERIFY(result.previousPeriodSpentMinor);

  value.limitMinor = 15000;
  result = BudgetPacingQuery::evaluate(value);
  QCOMPARE(result.state, ForecastContract::State::Warning);
  QVERIFY(result.predictedOverrun);

  value.limitMinor = 12000;
  value.criticalPercent = 85;
  result = BudgetPacingQuery::evaluate(value);
  QCOMPARE(result.state, ForecastContract::State::Critical);

  value.limitMinor = 10000;
  result = BudgetPacingQuery::evaluate(value);
  QCOMPARE(result.state, ForecastContract::State::Exceeded);
  QCOMPARE(*result.remainingMinor, 0);
  QCOMPARE(*result.safeTodayMinor, 0);
}

void BudgetPacingQueryTest::unavailablePrecedenceAndCompatibility() {
  BudgetPacingQuery::Request value = request();
  value.observations.clear();
  for (int day = 1; day <= 4; ++day)
    value.observations.append(observation(QDate(2026, 7, day), 1000));
  value.observations.append(observation(QDate(2026, 7, 11), 500));
  value.limitMinor = 1;
  QCOMPARE(BudgetPacingQuery::evaluate(value).reasonKey,
           QStringLiteral("insufficient-samples"));

  value = request();
  value.observations.last().currency = QStringLiteral("EUR");
  QCOMPARE(BudgetPacingQuery::evaluate(value).reasonKey,
           QStringLiteral("mixed-currency"));
  value = request();
  value.observations.last().valueClass =
      ForecastContract::ValueClass::Estimated;
  QCOMPARE(BudgetPacingQuery::evaluate(value).reasonKey,
           QStringLiteral("mixed-value-class"));
  value = request();
  value.observations.last().valueMinor.reset();
  QCOMPARE(BudgetPacingQuery::evaluate(value).reasonKey,
           QStringLiteral("no-data"));
  value = request();
  value.currency = QStringLiteral("XAU");
  QCOMPARE(BudgetPacingQuery::evaluate(value).reasonKey,
           QStringLiteral("unknown-currency"));

  value = request();
  for (auto &row : value.observations)
    row.valueMinor = 0;
  const auto explicitZero = BudgetPacingQuery::evaluate(value);
  QVERIFY(explicitZero.isValid());
  QCOMPARE(explicitZero.spentMinor, std::optional<qint64>(0));
}

void BudgetPacingQueryTest::scopedAndPreviousPeriodComparison() {
  BudgetPacingQuery::Request value = request();
  value.scopeMode = QStringLiteral("scoped");
  value.scopeKind = QStringLiteral("project");
  value.scopeIdentity = QStringLiteral("local-project-id");
  value.scopeLabel = QStringLiteral("Production");
  QCOMPARE(BudgetPacingQuery::evaluate(value).reasonKey,
           QStringLiteral("scope-unavailable"));
  for (auto &row : value.observations) {
    row.scopeKind = value.scopeKind;
    row.scopeIdentity = value.scopeIdentity;
  }
  const auto result = BudgetPacingQuery::evaluate(value);
  QVERIFY(result.isValid());
  QCOMPARE(result.scope, QStringLiteral("project:Production"));
  QCOMPARE(result.previousPeriodSpentMinor, std::optional<qint64>(24000));
  QVERIFY(result.previousPeriodChangePercent);
}

void BudgetPacingQueryTest::databaseBoundaryAndUnknownCurrency() {
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("observations.db"));
  const QString connectionName = QStringLiteral("budget_observations_%1")
                                     .arg(QUuid::createUuid().toString());
  QSqlDatabase database =
      QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
  database.setDatabaseName(path);
  QVERIFY(database.open());
  QSqlQuery query(database);
  QVERIFY(query.exec(QStringLiteral(
      "CREATE TABLE observations(id INTEGER PRIMARY KEY,provider TEXT,"
      "observed_at_utc TEXT,interval_start_utc TEXT,interval_end_utc TEXT,"
      "metric_kind TEXT,unit TEXT,value REAL,currency TEXT,semantic "
      "TEXT,source TEXT,"
      "data_quality TEXT,scope TEXT,window TEXT,model_scope TEXT,project_scope "
      "TEXT,service_tier_scope TEXT,line_item_scope TEXT)")));
  query.prepare(QStringLiteral(
      "INSERT INTO observations(provider,observed_at_utc,interval_start_utc,"
      "interval_end_utc,metric_kind,unit,value,currency,semantic,source,data_"
      "quality,"
      "scope,window,model_scope,project_scope) "
      "VALUES('openai',?,?,?,'cost','USD',?,?,'gauge',"
      "'billing_api','actual','organization','day','','')"));
  for (int day = 1; day <= 11; ++day) {
    const QDateTime start = QDate(2026, 7, day).startOfDay(QTimeZone::UTC);
    query.bindValue(0, start.addSecs(12 * 60 * 60)
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.bindValue(1, start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.bindValue(
        2, start.addDays(1).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.bindValue(3, 1.25);
    query.bindValue(4, QStringLiteral("USD"));
    QVERIFY(query.exec());
  }
  QVariantMap policy{
      {QStringLiteral("policyId"),
       QStringLiteral("11111111-1111-4111-8111-111111111111")},
      {QStringLiteral("sourceId"), QStringLiteral("openai")},
      {QStringLiteral("sourceKind"), QStringLiteral("provider")},
      {QStringLiteral("scopeMode"), QStringLiteral("aggregate")},
      {QStringLiteral("valueClass"), QStringLiteral("actual")},
      {QStringLiteral("limitMinor"), 10000},
      {QStringLiteral("currency"), QStringLiteral("USD")},
      {QStringLiteral("periodType"), QStringLiteral("calendar_month")},
      {QStringLiteral("timeZoneId"), QStringLiteral("UTC")},
      {QStringLiteral("warningPercent"), 80},
      {QStringLiteral("criticalPercent"), 90}};
  auto pacingRequest =
      BudgetObservationQuery::requestFromPolicy(policy, Now, database);
  QCOMPARE(pacingRequest.periodType, QStringLiteral("calendar_month"));
  const auto result = BudgetPacingQuery::evaluate(pacingRequest);
  QVERIFY2(result.isValid(), qPrintable(result.reasonKey));
  QCOMPARE(result.window, QStringLiteral("calendar_month"));
  QCOMPARE(result.spentMinor, std::optional<qint64>(1375));

  policy[QStringLiteral("currency")] = QStringLiteral("XAU");
  pacingRequest =
      BudgetObservationQuery::requestFromPolicy(policy, Now, database);
  QCOMPARE(BudgetPacingQuery::evaluate(pacingRequest).reasonKey,
           QStringLiteral("unknown-currency"));
  database.close();
  database = {};
  QSqlDatabase::removeDatabase(connectionName);
}

void BudgetPacingQueryTest::providerScopeDimensionsAtDatabaseBoundary() {
  QTemporaryDir directory;
  const QString connectionName =
      QStringLiteral("budget_scope_dimensions_%1")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  QSqlDatabase database =
      QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
  database.setDatabaseName(
      directory.filePath(QStringLiteral("scope-observations.db")));
  QVERIFY(database.open());
  QSqlQuery query(database);
  QVERIFY(query.exec(QStringLiteral(
      "CREATE TABLE observations(id INTEGER PRIMARY KEY,provider TEXT,"
      "observed_at_utc TEXT,interval_start_utc TEXT,interval_end_utc TEXT,"
      "metric_kind TEXT,unit TEXT,value REAL,currency TEXT,semantic TEXT,"
      "source TEXT,data_quality TEXT,scope TEXT,window TEXT,model_scope TEXT,"
      "project_scope TEXT,service_tier_scope TEXT,line_item_scope TEXT)")));

  const auto insertRows = [&query](const QString &provider,
                                   const QString &scope, const QString &model,
                                   const QString &project,
                                   const QString &serviceTier,
                                   const QString &lineItem) {
    query.prepare(QStringLiteral(
        "INSERT INTO observations(provider,observed_at_utc,interval_start_utc,"
        "interval_end_utc,metric_kind,unit,value,currency,semantic,source,"
        "data_quality,scope,window,model_scope,project_scope,"
        "service_tier_scope,line_item_scope) VALUES(?,?,?,?,'cost','USD',"
        "0.5,'USD','gauge','billing_api','actual',?,'day',?,?,?,?)"));
    for (int day = 1; day <= 11; ++day) {
      const QDateTime start = QDate(2026, 7, day).startOfDay(QTimeZone::UTC);
      query.bindValue(0, provider);
      query.bindValue(1, start.addSecs(12 * 60 * 60)
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
      query.bindValue(2, start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
      query.bindValue(
          3, start.addDays(1).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
      query.bindValue(4, scope);
      query.bindValue(5, model);
      query.bindValue(6, project);
      query.bindValue(7, serviceTier);
      query.bindValue(8, lineItem);
      QVERIFY(query.exec());
    }
  };
  insertRows(QStringLiteral("openai"),
             QStringLiteral("organization_scoped:line_item:Responses"),
             QStringLiteral("must-not-become-openai-model-cost"),
             QStringLiteral("project-a"), {}, QStringLiteral("Responses"));
  insertRows(QStringLiteral("anthropic"), QStringLiteral("organization_scoped"),
             QStringLiteral("claude-sonnet"), QStringLiteral("workspace-a"),
             QStringLiteral("standard"), QStringLiteral("Messages"));
  insertRows(QStringLiteral("litellm"), QStringLiteral("organization"), {}, {},
             {}, {});

  const auto policy = [](const QString &source, const QString &scopeKind,
                         const QString &scopeIdentity,
                         const QStringList &supportedScopes) {
    return QVariantMap{
        {QStringLiteral("policyId"), QUuid::createUuid().toString()},
        {QStringLiteral("sourceId"), source},
        {QStringLiteral("sourceKind"), QStringLiteral("provider")},
        {QStringLiteral("provider"), source},
        {QStringLiteral("scopeMode"), QStringLiteral("scoped")},
        {QStringLiteral("scopeKind"), scopeKind},
        {QStringLiteral("scopeIdentity"), scopeIdentity},
        {QStringLiteral("scopeLabel"), QStringLiteral("Local label")},
        {QStringLiteral("catalogSupportedScopes"), supportedScopes},
        {QStringLiteral("valueClass"), QStringLiteral("actual")},
        {QStringLiteral("limitMinor"), 10000},
        {QStringLiteral("currency"), QStringLiteral("USD")},
        {QStringLiteral("periodType"), QStringLiteral("calendar_month")},
        {QStringLiteral("timeZoneId"), QStringLiteral("UTC")},
        {QStringLiteral("warningPercent"), 80},
        {QStringLiteral("criticalPercent"), 90},
    };
  };
  const QStringList openAiScopes{QStringLiteral("aggregate"),
                                 QStringLiteral("project"),
                                 QStringLiteral("line_item")};
  for (const auto &[kind, identity] : QList<QPair<QString, QString>>{
           {QStringLiteral("project"), QStringLiteral("project-a")},
           {QStringLiteral("line_item"), QStringLiteral("Responses")}}) {
    const auto result =
        BudgetPacingQuery::evaluate(BudgetObservationQuery::requestFromPolicy(
            policy(QStringLiteral("openai"), kind, identity, openAiScopes), Now,
            database));
    QVERIFY2(result.isValid(), qPrintable(result.reasonKey));
    QCOMPARE(result.spentMinor, std::optional<qint64>(550));
  }
  QCOMPARE(BudgetPacingQuery::evaluate(
               BudgetObservationQuery::requestFromPolicy(
                   policy(QStringLiteral("openai"), QStringLiteral("model"),
                          QStringLiteral("must-not-become-"
                                         "openai-model-cost"),
                          openAiScopes),
                   Now, database))
               .reasonKey,
           QStringLiteral("scope-unavailable"));

  const QStringList anthropicScopes{
      QStringLiteral("aggregate"), QStringLiteral("workspace"),
      QStringLiteral("model"), QStringLiteral("service_tier"),
      QStringLiteral("line_item")};
  const QList<QPair<QString, QString>> anthropicDimensions{
      {QStringLiteral("workspace"), QStringLiteral("workspace-a")},
      {QStringLiteral("model"), QStringLiteral("claude-sonnet")},
      {QStringLiteral("service_tier"), QStringLiteral("standard")},
      {QStringLiteral("line_item"), QStringLiteral("Messages")}};
  for (const auto &[kind, identity] : anthropicDimensions) {
    const auto result =
        BudgetPacingQuery::evaluate(BudgetObservationQuery::requestFromPolicy(
            policy(QStringLiteral("anthropic"), kind, identity,
                   anthropicScopes),
            Now, database));
    QVERIFY2(result.isValid(), qPrintable(result.reasonKey));
    QCOMPARE(result.spentMinor, std::optional<qint64>(550));
  }

  const auto liteLlmScoped =
      policy(QStringLiteral("litellm"), QStringLiteral("model"),
             QStringLiteral("gateway-model"), {QStringLiteral("aggregate")});
  QCOMPARE(
      BudgetPacingQuery::evaluate(BudgetObservationQuery::requestFromPolicy(
                                      liteLlmScoped, Now, database))
          .reasonKey,
      QStringLiteral("scope-unavailable"));

  database.close();
  database = {};
  QSqlDatabase::removeDatabase(connectionName);
}

void BudgetPacingQueryTest::performanceAtOneHundredThousandObservations() {
  BudgetPacingQuery::Request value = request();
  value.observations.clear();
  value.observations.reserve(100000);
  for (int index = 0; index < 100000; ++index) {
    auto row = observation(QDate(2026, 7, index % 10 + 1), 1000);
    row.observedAt = row.observedAt.addMSecs(index);
    value.observations.append(row);
  }
  value.observations.append(observation(QDate(2026, 7, 11), 500));
  QElapsedTimer timer;
  timer.start();
  const auto result = BudgetPacingQuery::evaluate(value);
  const qint64 elapsed = timer.elapsed();
  QVERIFY(result.isValid());
  QVERIFY2(
      elapsed <= 750,
      qPrintable(QStringLiteral("100k pacing query took %1 ms").arg(elapsed)));
}

QTEST_GUILESS_MAIN(BudgetPacingQueryTest)
#include "test_budgetpacingquery.moc"
