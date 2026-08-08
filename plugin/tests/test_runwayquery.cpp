#include <QtTest>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUuid>

#include "forecastcontract.h"
#include "guardrailmodel.h"
#include "runwayquery.h"

namespace {

const QDateTime Now(QDate(2026, 7, 11), QTime(12, 0), QTimeZone::UTC);

RunwayQuery::QuotaRequest quotaRequest() {
  RunwayQuery::QuotaRequest request;
  request.sourceId = QStringLiteral("openai");
  request.sourceKind = QStringLiteral("provider");
  request.window = QStringLiteral("requests_minute");
  request.scope = QStringLiteral("api_key");
  request.unit = QStringLiteral("request");
  const QDateTime resetAt = Now.addSecs(2 * 60 * 60);
  const QList<double> remaining{100.0, 80.0, 60.0, 40.0};
  for (qsizetype index = 0; index < remaining.size(); ++index) {
    RunwayQuery::QuotaSample sample;
    sample.observedAt = Now.addSecs((-30 + static_cast<int>(index) * 10) * 60);
    sample.resetAt = resetAt;
    sample.remaining = remaining.at(index);
    sample.limit = 1000.0;
    sample.unit = request.unit;
    sample.source = QStringLiteral("response_headers");
    request.samples.append(sample);
  }
  return request;
}

RunwayQuery::BudgetRequest budgetRequest(int completedDays = 10,
                                         int reportedDays = 7) {
  RunwayQuery::BudgetRequest request;
  request.sourceId = QStringLiteral("openai-budget");
  request.sourceKind = QStringLiteral("provider");
  request.scope = QStringLiteral("organization");
  request.budget = 300.0;
  request.budgetCurrency = QStringLiteral("USD");
  for (int dayNumber = 1; dayNumber <= reportedDays; ++dayNumber) {
    RunwayQuery::BudgetDay day;
    day.periodStart = QDate(2026, 7, dayNumber).startOfDay(QTimeZone::UTC);
    day.periodEnd = day.periodStart.addDays(1);
    day.value = 10.0;
    day.currency = QStringLiteral("USD");
    request.days.append(day);
  }
  Q_UNUSED(completedDays)
  return request;
}

QVariantMap quotaMap(const RunwayQuery::QuotaRequest &request) {
  QVariantList samples;
  for (const RunwayQuery::QuotaSample &sample : request.samples) {
    samples.append(QVariantMap{
        {QStringLiteral("observedAt"), sample.observedAt},
        {QStringLiteral("resetAt"), sample.resetAt},
        {QStringLiteral("remaining"),
         sample.remaining ? QVariant(*sample.remaining) : QVariant()},
        {QStringLiteral("limit"),
         sample.limit ? QVariant(*sample.limit) : QVariant()},
        {QStringLiteral("unit"), sample.unit},
        {QStringLiteral("source"), sample.source},
    });
  }
  return {
      {QStringLiteral("sourceId"), request.sourceId},
      {QStringLiteral("sourceKind"), request.sourceKind},
      {QStringLiteral("window"), request.window},
      {QStringLiteral("scope"), request.scope},
      {QStringLiteral("unit"), request.unit},
      {QStringLiteral("samples"), samples},
  };
}

QVariantMap budgetMap(const RunwayQuery::BudgetRequest &request) {
  QVariantList days;
  for (const RunwayQuery::BudgetDay &day : request.days) {
    days.append(QVariantMap{
        {QStringLiteral("periodStart"), day.periodStart},
        {QStringLiteral("periodEnd"), day.periodEnd},
        {QStringLiteral("value"),
         day.value ? QVariant(*day.value) : QVariant()},
        {QStringLiteral("currency"), day.currency},
        {QStringLiteral("valueClass"),
         ForecastContract::valueClassKey(day.valueClass)},
    });
  }
  return {
      {QStringLiteral("sourceId"), request.sourceId},
      {QStringLiteral("sourceKind"), request.sourceKind},
      {QStringLiteral("window"), request.window},
      {QStringLiteral("scope"), request.scope},
      {QStringLiteral("budget"), request.budget},
      {QStringLiteral("budgetCurrency"), request.budgetCurrency},
      {QStringLiteral("valueClass"),
       ForecastContract::valueClassKey(request.valueClass)},
      {QStringLiteral("days"), days},
  };
}

int runwayConnectionCount() {
  int count = 0;
  for (const QString &name : QSqlDatabase::connectionNames()) {
    if (name.startsWith(QLatin1String("aiusagemonitor_runway_"))) {
      ++count;
    }
  }
  return count;
}

} // namespace

class RunwayQueryTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void quotaStableDecline();
  void quotaRiskBoundaries();
  void quotaNoConsumptionIsSafe();
  void quotaUnavailableStates();
  void quotaNumericZeroIsNotMissing();
  void quotaSparseCoverageAndTimeZones();
  void budgetPacingBoundaries();
  void budgetUnavailableStates();
  void budgetActualEstimatedSeparation();
  void databaseQueryAndConnectionCleanup();
  void budgetPolicyDatabaseFacadeAndCancellation();
  void asynchronousSupersessionAndCleanup();
  void cacheHitAndInvalidation();
  void localizedReasonsAreComplete();
  void performanceAtOneHundredThousandObservations();
};

void RunwayQueryTest::quotaStableDecline() {
  const ForecastContract::Result result =
      RunwayQuery::quotaRunway(quotaRequest(), Now);
  QVERIFY(result.isValid());
  QCOMPARE(result.state, ForecastContract::State::Critical);
  QCOMPARE(*result.currentValue, 40.0);
  QCOMPARE(*result.limitValue, 1000.0);
  QVERIFY(result.predictedAt);
  QCOMPARE(result.predictedAt->toSecsSinceEpoch(),
           Now.addSecs(20 * 60).toSecsSinceEpoch());
  QCOMPARE(result.methodId, QStringLiteral("quota-runway-v1"));
  QCOMPARE(result.sampleCount, 4);
  QCOMPARE(result.coveragePercent, 100.0);
}

void RunwayQueryTest::quotaRiskBoundaries() {
  RunwayQuery::QuotaRequest warning = quotaRequest();
  warning.samples[0].remaining = 460.0;
  warning.samples[1].remaining = 440.0;
  warning.samples[2].remaining = 420.0;
  warning.samples[3].remaining = 400.0;
  for (RunwayQuery::QuotaSample &sample : warning.samples) {
    sample.resetAt = Now.addSecs(5 * 60 * 60);
  }
  ForecastContract::Result result = RunwayQuery::quotaRunway(warning, Now);
  QVERIFY(result.isValid());
  QCOMPARE(result.state, ForecastContract::State::Warning);
  QVERIFY(result.predictedAt);
  QVERIFY(*result.predictedAt < result.periodEnd);

  RunwayQuery::QuotaRequest safe = quotaRequest();
  safe.samples[0].remaining = 1000.0;
  safe.samples[1].remaining = 980.0;
  safe.samples[2].remaining = 960.0;
  safe.samples[3].remaining = 940.0;
  result = RunwayQuery::quotaRunway(safe, Now);
  QVERIFY(result.isValid());
  QCOMPARE(result.state, ForecastContract::State::Safe);
  QVERIFY(!result.predictedAt);
  QVERIFY(*result.projectedValue > 0.0);
}

void RunwayQueryTest::quotaNoConsumptionIsSafe() {
  RunwayQuery::QuotaRequest request = quotaRequest();
  for (RunwayQuery::QuotaSample &sample : request.samples) {
    sample.remaining = 500.0;
  }
  const ForecastContract::Result result =
      RunwayQuery::quotaRunway(request, Now);
  QVERIFY(result.isValid());
  QCOMPARE(result.state, ForecastContract::State::Safe);
  QCOMPARE(*result.projectedValue, 500.0);
  QVERIFY(!result.predictedAt);
}

void RunwayQueryTest::quotaUnavailableStates() {
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    request.samples.resize(3);
    QCOMPARE(RunwayQuery::quotaRunway(request, Now).reasonKey,
             QStringLiteral("insufficient_samples"));
  }
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    for (qsizetype index = 0; index < request.samples.size(); ++index) {
      request.samples[index].observedAt = Now.addSecs((-3 + index) * 60);
    }
    QCOMPARE(RunwayQuery::quotaRunway(request, Now).reasonKey,
             QStringLiteral("insufficient_span"));
  }
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    QCOMPARE(RunwayQuery::quotaRunway(request, Now.addSecs(16 * 60)).reasonKey,
             QStringLiteral("stale_data"));
  }
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    request.samples[2].remaining = 90.0;
    QCOMPARE(RunwayQuery::quotaRunway(request, Now).reasonKey,
             QStringLiteral("non_monotonic"));
  }
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    request.samples[1].resetAt = request.samples[1].resetAt.addSecs(60);
    QCOMPARE(RunwayQuery::quotaRunway(request, Now).reasonKey,
             QStringLiteral("reset_detected"));
  }
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    request.samples.last().remaining.reset();
    QCOMPARE(RunwayQuery::quotaRunway(request, Now).reasonKey,
             QStringLiteral("missing_value"));
  }
  {
    RunwayQuery::QuotaRequest request = quotaRequest();
    request.samples.last().source = QStringLiteral("self_tracked");
    QCOMPARE(RunwayQuery::quotaRunway(request, Now).reasonKey,
             QStringLiteral("unsupported_source"));
  }
}

void RunwayQueryTest::quotaNumericZeroIsNotMissing() {
  RunwayQuery::QuotaRequest request = quotaRequest();
  request.samples.last().remaining = 0.0;
  const ForecastContract::Result result =
      RunwayQuery::quotaRunway(request, Now);
  QVERIFY(result.isValid());
  QCOMPARE(result.state, ForecastContract::State::Critical);
  QCOMPARE(*result.currentValue, 0.0);
  QVERIFY(result.predictedAt);
  QCOMPARE(*result.predictedAt, Now);
}

void RunwayQueryTest::quotaSparseCoverageAndTimeZones() {
  RunwayQuery::QuotaRequest request = quotaRequest();
  const QList<int> minutes{-180, -170, -160, 0};
  for (qsizetype index = 0; index < request.samples.size(); ++index) {
    request.samples[index].observedAt =
        Now.addSecs(minutes.at(index) * 60)
            .toTimeZone(QTimeZone("Europe/Stockholm"));
  }
  const ForecastContract::Result result = RunwayQuery::quotaRunway(
      request, Now.toTimeZone(QTimeZone("America/New_York")));
  QVERIFY(result.isValid());
  QVERIFY(result.coveragePercent < 50.0);
  QCOMPARE(result.generatedAt, Now);

  RunwayQuery::QuotaRequest dst = quotaRequest();
  const QDateTime dstNow(QDate(2026, 3, 8), QTime(8, 0), QTimeZone::UTC);
  const QDateTime reset = dstNow.addSecs(3 * 60 * 60);
  for (qsizetype index = 0; index < dst.samples.size(); ++index) {
    dst.samples[index].observedAt =
        dstNow.addSecs((-30 + index * 10) * 60)
            .toTimeZone(QTimeZone("America/New_York"));
    dst.samples[index].resetAt =
        reset.toTimeZone(QTimeZone("Europe/Stockholm"));
  }
  const ForecastContract::Result dstResult =
      RunwayQuery::quotaRunway(dst, dstNow);
  QVERIFY(dstResult.isValid());
  QCOMPARE(dstResult.periodEnd, reset);
}

void RunwayQueryTest::budgetPacingBoundaries() {
  RunwayQuery::BudgetRequest request = budgetRequest();
  ForecastContract::Result result = RunwayQuery::budgetPacing(request, Now);
  QVERIFY(result.isValid());
  QCOMPARE(result.state, ForecastContract::State::Safe);
  QCOMPARE(*result.currentValue, 70.0);
  QCOMPARE(*result.projectedValue, 280.0);
  QCOMPARE(result.coveragePercent, 70.0);
  QCOMPARE(result.sampleCount, 7);
  QCOMPARE(result.periodEnd, QDate(2026, 8, 1).startOfDay(QTimeZone::UTC));

  RunwayQuery::BudgetDay incomplete;
  incomplete.periodStart = QDate(2026, 7, 11).startOfDay(QTimeZone::UTC);
  incomplete.periodEnd = incomplete.periodStart.addDays(1);
  incomplete.value = 10000.0;
  incomplete.currency = QStringLiteral("USD");
  request.days.append(incomplete);
  result = RunwayQuery::budgetPacing(request, Now);
  QCOMPARE(*result.currentValue, 70.0);

  request.budget = 260.0;
  result = RunwayQuery::budgetPacing(request, Now);
  QCOMPARE(result.state, ForecastContract::State::Warning);
  QVERIFY(result.predictedAt);

  request.budget = 200.0;
  result = RunwayQuery::budgetPacing(request, Now);
  QCOMPARE(result.state, ForecastContract::State::Critical);
}

void RunwayQueryTest::budgetUnavailableStates() {
  {
    RunwayQuery::BudgetRequest request = budgetRequest(10, 4);
    QCOMPARE(RunwayQuery::budgetPacing(request, Now).reasonKey,
             QStringLiteral("insufficient_samples"));
  }
  {
    RunwayQuery::BudgetRequest request = budgetRequest(10, 6);
    QCOMPARE(RunwayQuery::budgetPacing(request, Now).reasonKey,
             QStringLiteral("insufficient_coverage"));
  }
  {
    RunwayQuery::BudgetRequest request = budgetRequest();
    request.days.last().currency = QStringLiteral("EUR");
    QCOMPARE(RunwayQuery::budgetPacing(request, Now).reasonKey,
             QStringLiteral("mixed_currency"));
  }
  {
    RunwayQuery::BudgetRequest request = budgetRequest();
    request.budgetCurrency = QStringLiteral("EUR");
    QCOMPARE(RunwayQuery::budgetPacing(request, Now).reasonKey,
             QStringLiteral("currency_mismatch"));
  }
  {
    RunwayQuery::BudgetRequest request = budgetRequest();
    request.days.last().valueClass = ForecastContract::ValueClass::Estimated;
    QCOMPARE(RunwayQuery::budgetPacing(request, Now).reasonKey,
             QStringLiteral("mixed_value_class"));
  }
  {
    RunwayQuery::BudgetRequest request = budgetRequest();
    request.budget = 0.0;
    QCOMPARE(RunwayQuery::budgetPacing(request, Now).reasonKey,
             QStringLiteral("missing_budget"));
  }
}

void RunwayQueryTest::budgetActualEstimatedSeparation() {
  RunwayQuery::BudgetRequest actual = budgetRequest();
  RunwayQuery::BudgetRequest estimated = budgetRequest();
  estimated.valueClass = ForecastContract::ValueClass::Estimated;
  for (RunwayQuery::BudgetDay &day : estimated.days) {
    day.valueClass = ForecastContract::ValueClass::Estimated;
  }
  const ForecastContract::Result actualResult =
      RunwayQuery::budgetPacing(actual, Now);
  const ForecastContract::Result estimatedResult =
      RunwayQuery::budgetPacing(estimated, Now);
  QVERIFY(actualResult.isValid());
  QVERIFY(estimatedResult.isValid());
  QCOMPARE(actualResult.valueClass, ForecastContract::ValueClass::Actual);
  QCOMPARE(estimatedResult.valueClass, ForecastContract::ValueClass::Estimated);
  QVERIFY(actualResult.stableId() != estimatedResult.stableId());

  const ForecastContract::Result strongResult =
      RunwayQuery::budgetPacing(budgetRequest(10, 10), Now);
  QCOMPARE(strongResult.evidenceGrade, ForecastContract::EvidenceGrade::Strong);
}

void RunwayQueryTest::databaseQueryAndConnectionCleanup() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("runway.db"));
  const QString connection =
      QStringLiteral("runway_fixture_%1")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  {
    QSqlDatabase database =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    database.setDatabaseName(path);
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE observations("
        "id INTEGER PRIMARY KEY, provider TEXT, observed_at_utc TEXT, "
        "interval_start_utc TEXT, interval_end_utc TEXT, "
        "metric_kind TEXT, unit TEXT, value REAL, currency TEXT, "
        "semantic TEXT, source TEXT, data_quality TEXT, scope TEXT, "
        "window TEXT, model_scope TEXT, project_scope TEXT, "
        "reset_at_utc TEXT, correlation_id TEXT)")));
    query.prepare(QStringLiteral(
        "INSERT INTO observations(provider,observed_at_utc,metric_kind,"
        "unit,value,semantic,source,data_quality,scope,window,"
        "model_scope,project_scope,reset_at_utc,correlation_id) "
        "VALUES('openai',?,?,?,?,?,'response_headers','actual','api_key',"
        "'minute','','',?,?)"));
    for (int index = 0; index < 4; ++index) {
      const QString correlation = QString::number(index);
      const QString observed =
          Now.addSecs((-30 + index * 10) * 60)
              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
      const QString reset =
          Now.addSecs(2 * 60 * 60)
              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
      query.bindValue(0, observed);
      query.bindValue(1, QStringLiteral("request_remaining"));
      query.bindValue(2, QStringLiteral("request"));
      query.bindValue(3, 100 - index * 20);
      query.bindValue(4, QStringLiteral("gauge"));
      query.bindValue(5, reset);
      query.bindValue(6, correlation);
      QVERIFY(query.exec());
      query.bindValue(0, observed);
      query.bindValue(1, QStringLiteral("request_limit"));
      query.bindValue(2, QStringLiteral("request"));
      query.bindValue(3, 1000);
      query.bindValue(4, QStringLiteral("gauge"));
      query.bindValue(5, reset);
      query.bindValue(6, correlation);
      QVERIFY(query.exec());
    }
    database.close();
    database = QSqlDatabase();
  }
  QSqlDatabase::removeDatabase(connection);

  const QVariantMap descriptor{
      {QStringLiteral("sourceId"), QStringLiteral("openai")},
      {QStringLiteral("sourceKind"), QStringLiteral("provider")},
      {QStringLiteral("provider"), QStringLiteral("openai")},
      {QStringLiteral("remainingKind"), QStringLiteral("request_remaining")},
      {QStringLiteral("limitKind"), QStringLiteral("request_limit")},
      {QStringLiteral("window"), QStringLiteral("minute")},
      {QStringLiteral("scope"), QStringLiteral("api_key")},
      {QStringLiteral("unit"), QStringLiteral("request")},
  };
  const QVariantList results = RunwayQuery::execute(
      {{QStringLiteral("databasePath"), path},
       {QStringLiteral("generatedAt"), Now},
       {QStringLiteral("quotaSources"), QVariantList{descriptor}}});
  QCOMPARE(results.size(), 1);
  const QVariantMap result = results.first().toMap();
  QVERIFY2(result.value(QStringLiteral("state")).toString() ==
               QLatin1String("critical"),
           qPrintable(result.value(QStringLiteral("reasonKey")).toString()));
  QCOMPARE(runwayConnectionCount(), 0);

  GuardrailModel model;
  QSignalSpy completedSpy(&model, &GuardrailModel::completed);
  model.refreshWithQuery(
      {{QStringLiteral("databasePath"), path},
       {QStringLiteral("generatedAt"), Now},
       {QStringLiteral("quotaSources"), QVariantList{descriptor}}});
  QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 3000);
  QTRY_COMPARE_WITH_TIMEOUT(model.pendingWorkerCount(), 0, 3000);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(runwayConnectionCount(), 0);
}

void RunwayQueryTest::budgetPolicyDatabaseFacadeAndCancellation() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("budget-policy.db"));
  const QString connection = QStringLiteral("budget_policy_fixture_%1")
                                 .arg(QUuid::createUuid().toString());
  {
    QSqlDatabase database =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    database.setDatabaseName(path);
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE observations(id INTEGER PRIMARY KEY,provider TEXT,"
        "observed_at_utc TEXT,interval_start_utc TEXT,interval_end_utc TEXT,"
        "metric_kind TEXT,unit TEXT,value REAL,currency TEXT,semantic "
        "TEXT,source TEXT,"
        "data_quality TEXT,scope TEXT,window TEXT,model_scope "
        "TEXT,project_scope TEXT)")));
    query.prepare(QStringLiteral(
        "INSERT INTO observations(provider,observed_at_utc,interval_start_utc,"
        "interval_end_utc,metric_kind,unit,value,currency,semantic,source,data_"
        "quality,"
        "scope,window,model_scope,project_scope) "
        "VALUES('openai',?,?,?,'cost','USD',1.0,"
        "'USD','gauge','billing_api','actual','organization','day','','')"));
    for (int day = 1; day <= 11; ++day) {
      const QDateTime start = QDate(2026, 7, day).startOfDay(QTimeZone::UTC);
      query.bindValue(0, start.addSecs(12 * 60 * 60)
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
      query.bindValue(1, start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
      query.bindValue(
          2, start.addDays(1).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
      QVERIFY(query.exec());
    }
    database.close();
    database = {};
  }
  QSqlDatabase::removeDatabase(connection);
  const QVariantMap policy{
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
      {QStringLiteral("criticalPercent"), 90},
      {QStringLiteral("provider"), QStringLiteral("openai")},
      {QStringLiteral("observationScope"), QStringLiteral("organization")}};
  const QVariantMap request{
      {QStringLiteral("databasePath"), path},
      {QStringLiteral("generatedAt"), Now},
      {QStringLiteral("budgetPolicies"), QVariantList{policy}}};
  const QVariantList results = RunwayQuery::execute(request);
  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first()
               .toMap()
               .value(QStringLiteral("contractVersion"))
               .toString(),
           QStringLiteral("budget-pacing-v2"));
  QCOMPARE(
      results.first().toMap().value(QStringLiteral("spentMinor")).toLongLong(),
      1100);
  QCOMPARE(runwayConnectionCount(), 0);

  std::atomic_bool cancelled = true;
  QVERIFY(RunwayQuery::execute(request, &cancelled).isEmpty());
  QCOMPARE(runwayConnectionCount(), 0);
}

void RunwayQueryTest::asynchronousSupersessionAndCleanup() {
  GuardrailModel model;
  QSignalSpy completedSpy(&model, &GuardrailModel::completed);

  RunwayQuery::QuotaRequest slow = quotaRequest();
  slow.samples.clear();
  slow.samples.reserve(100000);
  const QDateTime resetAt = Now.addSecs(48 * 60 * 60);
  for (int index = 0; index < 100000; ++index) {
    RunwayQuery::QuotaSample sample;
    sample.observedAt = Now.addSecs(-100000 + index);
    sample.resetAt = resetAt;
    sample.remaining = 200000.0 - index;
    sample.limit = 300000.0;
    sample.unit = QStringLiteral("request");
    sample.source = QStringLiteral("response_headers");
    slow.samples.append(sample);
  }
  model.refreshWithQuery(
      {{QStringLiteral("generatedAt"), Now},
       {QStringLiteral("quotaSeries"), QVariantList{quotaMap(slow)}}});

  RunwayQuery::BudgetRequest current = budgetRequest();
  current.sourceId = QStringLiteral("latest-budget");
  model.refreshWithQuery(
      {{QStringLiteral("generatedAt"), Now},
       {QStringLiteral("budgetSeries"), QVariantList{budgetMap(current)}}});

  QTRY_VERIFY_WITH_TIMEOUT(completedSpy.count() >= 1, 5000);
  QCOMPARE(model.generation(), 2);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.forecasts()
               .first()
               .toMap()
               .value(QStringLiteral("sourceId"))
               .toString(),
           QStringLiteral("latest-budget"));
  QCOMPARE(
      model.data(model.index(0, 0), GuardrailModel::SourceIdRole).toString(),
      QStringLiteral("latest-budget"));
  QVERIFY(!model.data(model.index(0, 0), GuardrailModel::EvidenceGradeRole)
               .toString()
               .isEmpty());
  QTRY_COMPARE_WITH_TIMEOUT(model.pendingWorkerCount(), 0, 5000);
  QVERIFY(!model.isBusy());
  QCOMPARE(runwayConnectionCount(), 0);
}

void RunwayQueryTest::cacheHitAndInvalidation() {
  GuardrailModel model;
  QSignalSpy completedSpy(&model, &GuardrailModel::completed);
  RunwayQuery::BudgetRequest request = budgetRequest();
  const QVariantMap query{
      {QStringLiteral("generatedAt"), Now},
      {QStringLiteral("budgetSeries"), QVariantList{budgetMap(request)}},
      {QStringLiteral("observationRevision"), 1},
      {QStringLiteral("policyRevision"), 1},
  };

  model.refreshWithQuery(query);
  QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 3000);
  QTRY_COMPARE_WITH_TIMEOUT(model.pendingWorkerCount(), 0, 3000);

  model.refreshWithQuery(query);
  QCOMPARE(completedSpy.count(), 2);
  QCOMPARE(model.pendingWorkerCount(), 0);

  model.invalidateCache();
  model.refreshWithQuery(query);
  QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 3, 3000);
  QTRY_COMPARE_WITH_TIMEOUT(model.pendingWorkerCount(), 0, 3000);
}

void RunwayQueryTest::localizedReasonsAreComplete() {
  GuardrailModel model;
  for (const QString &reason : ForecastContract::unavailableReasonKeys()) {
    QVERIFY2(!model.localizedReason(reason).isEmpty(), qPrintable(reason));
  }
}

void RunwayQueryTest::performanceAtOneHundredThousandObservations() {
  RunwayQuery::QuotaRequest request = quotaRequest();
  request.samples.clear();
  request.samples.reserve(100000);
  const QDateTime resetAt = Now.addSecs(48 * 60 * 60);
  for (int index = 0; index < 100000; ++index) {
    RunwayQuery::QuotaSample sample;
    sample.observedAt = Now.addSecs(-100000 + index);
    sample.resetAt = resetAt;
    sample.remaining = 200000.0 - index;
    sample.limit = 300000.0;
    sample.unit = QStringLiteral("request");
    sample.source = QStringLiteral("response_headers");
    request.samples.append(sample);
  }
  QElapsedTimer timer;
  timer.start();
  const ForecastContract::Result result =
      RunwayQuery::quotaRunway(request, Now);
  const qint64 elapsed = timer.elapsed();
  QVERIFY(result.isValid());
  QVERIFY2(
      elapsed <= 750,
      qPrintable(QStringLiteral("100k runway query took %1 ms").arg(elapsed)));
  qInfo() << "100k runway query:" << elapsed << "ms";
}

QTEST_GUILESS_MAIN(RunwayQueryTest)

#include "test_runwayquery.moc"
