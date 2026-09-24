#include <QtTest>

#include "dailystatemodel.h"
#include "providerbackend.h"
#include "sourcedetailmodel.h"
#include "sourcereadinessmodel.h"
#include "subscriptiontoolbackend.h"

class DailyProvider final : public ProviderBackend {
  Q_OBJECT

public:
  QString name() const override { return QStringLiteral("Daily provider"); }
  QString iconName() const override { return QStringLiteral("network-server"); }
  void refreshImpl() override {}

  void makeReady() {
    setApiKey(QStringLiteral("test-key"));
    setConnected(true);
  }

  void addMetric(MetricKind kind, const QVariant &value, const QString &unit,
                 const QString &currency, const QString &window,
                 MetricSource source, const QDateTime &resetAt = QDateTime()) {
    setProviderMetric(kind, value, unit, currency, QStringLiteral("account"),
                      window, source, QStringLiteral("actual"), resetAt);
  }

  void addScopedCost(double value, const QString &model,
                     const QString &project) {
    setProviderMetric(
        MetricKind::Cost, value, QStringLiteral("USD"),
        QStringLiteral("USD"), QStringLiteral("organization_scoped"),
        QStringLiteral("day"), MetricSource::BillingApi,
        QStringLiteral("actual"), {}, {}, {}, model, project);
  }

  void addQuota(double remaining, const QDateTime &resetAt = QDateTime()) {
    addMetric(MetricKind::RequestLimit, 100.0, QStringLiteral("request"),
              QString(), QStringLiteral("rolling"),
              MetricSource::ResponseHeaders, resetAt);
    addMetric(MetricKind::RequestRemaining, remaining,
              QStringLiteral("request"), QString(), QStringLiteral("rolling"),
              MetricSource::ResponseHeaders, resetAt);
  }

  void addTokenQuota(double remaining, const QDateTime &resetAt = QDateTime()) {
    addMetric(MetricKind::TokenLimit, 200.0, QStringLiteral("token"), QString(),
              QStringLiteral("daily"), MetricSource::ResponseHeaders, resetAt);
    addMetric(MetricKind::TokenRemaining, remaining, QStringLiteral("token"),
              QString(), QStringLiteral("daily"), MetricSource::ResponseHeaders,
              resetAt);
  }

  void setDailySpend(double spent, double budget) {
    setDailyBudget(budget);
    setDailyCost(spent);
    addMetric(MetricKind::Cost, spent, QStringLiteral("USD"),
              QStringLiteral("USD"), QStringLiteral("day"),
              MetricSource::BillingApi);
  }

  void setMonthlySpend(double spent, double budget) {
    setMonthlyBudget(budget);
    setMonthlyCost(spent);
  }

  void makeStale() {
    updateLastRefreshed(QDateTime::currentDateTimeUtc().addDays(-2));
  }

  void failAuthentication() {
    setErrorDetails(QStringLiteral("redacted"),
                    ProviderErrorKind::Authentication);
  }
  void failRateLimited(const QDateTime &retryAt) {
    setErrorDetails(QStringLiteral("redacted"), ProviderErrorKind::RateLimit,
                    429, retryAt);
  }
};

class DailyTool final : public SubscriptionToolBackend {
  Q_OBJECT

public:
  QString toolName() const override { return QStringLiteral("Daily tool"); }
  QString iconName() const override {
    return QStringLiteral("applications-development");
  }
  QString toolColor() const override { return QStringLiteral("#000000"); }
  QString periodLabel() const override {
    return QStringLiteral("5-hour window");
  }
  QString secondaryPeriodLabel() const override {
    return QStringLiteral("Weekly window");
  }
  bool hasSecondaryLimit() const override { return secondaryUsageLimit() > 0; }
  void checkToolInstalled() override {}
  void detectActivity() override {}
  QStringList availablePlans() const override {
    return {QStringLiteral("pro")};
  }
  int defaultLimitForPlan(const QString &) const override { return 100; }

  void install() { setInstalled(true); }
  void recordActivity() { setLastActivity(QDateTime::currentDateTimeUtc()); }
  void recordActivityAt(const QDateTime &time) { setLastActivity(time); }
  void setCount(int count) { setUsageCount(count); }
  void sync(const QVariantList &windows) {
    setSyncedQuotaWindows(windows);
    setLastSyncTime(QDateTime::currentDateTimeUtc());
  }

protected:
  UsagePeriod primaryPeriodType() const override { return FiveHour; }
};

class DailyStateModelTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void liveClockAcceptsObservationAfterLastTick();
  void actualQuotaWinsOverLocalTarget();
  void localConfiguredLimitWithoutActivityIsNotReported();
  void staleLocalActivityIsLastKnownOnly();
  void providerQuotaAgesWithPresentationClock();
  void quotaClockAndIndependentReset();
  void enabledSourcesAppearExactlyOnce();
  void toolOnlySummaryIsComplete();
  void unavailableAndAvailableZeroStayDistinct();
  void expiredCostRemainsLastKnownButDoesNotAggregate();
  void retryAfterIsPresentInSourceDetail();
  void providerToolMixedCurrencyAndFeesAggregateSeparately();
  void rangePricedToolPreservesRange();
  void staleBalanceAndConnectivityRemainDistinct();
  void priorityRules_data();
  void priorityRules();
  void budgetPriorityRequiresTypedCost();
  void monthlyBudgetUsesBackendBillingValue();
  void priorityTiesAreDeterministic();
  void panelAggregatesUseTypedDailyMetrics();
  void scopedRowsNeverDoubleDailyTotals();
  void documentedAndLocalLimitsAreNotLiveQuota();
  void quotaWindowsPreserveSourceAndReset();
  void staleSnapshotsPrioritizeFreshnessOverCachedQuota();
  void sourceDetailPreservesTypedMetricsAndConcreteAction();
  void normalSourcesSortByReportingQuality();
};

void DailyStateModelTest::liveClockAcceptsObservationAfterLastTick() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  readiness.registerLocalTool("codex-cli", &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool("codex-cli", &tool);
  daily.resetPresentationTime();
  QTest::qWait(20);
  tool.sync({QVariantMap{{"kind", "session"}, {"source", "browser_sync"},
                        {"percentRemaining", 80.0}}});
  QCOMPARE(daily.summary().value("lowestActualRemainingQuota").toMap().value("percentRemaining").toDouble(), 80.0);
  QCOMPARE(readiness.source("codex-cli").value("readinessStateKey").toString(), QStringLiteral("reporting_actual"));
}

void DailyStateModelTest::actualQuotaWinsOverLocalTarget() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  tool.setUsageLimit(1);
  tool.incrementUsage();
  tool.sync({QVariantMap{{"kind", "session"},
                         {"source", "browser_sync"},
                         {"percentRemaining", 80.0}}});
  readiness.registerLocalTool("codex-cli", &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool("codex-cli", &tool);
  daily.setPresentationTime(QDateTime::currentDateTimeUtc().addSecs(1));
  QCOMPARE(daily.source("codex-cli").value("percentRemaining").toDouble(),
           80.0);
  QCOMPARE(daily.summary()
               .value("lowestActualRemainingQuota")
               .toMap()
               .value("percentRemaining")
               .toDouble(),
           80.0);
}

void DailyStateModelTest::localConfiguredLimitWithoutActivityIsNotReported() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  tool.setUsageLimit(100);
  readiness.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool(QStringLiteral("codex-cli"), &tool);

  const QVariantMap row = daily.source(QStringLiteral("codex-cli"));
  QCOMPARE(row.value(QStringLiteral("readinessState")).toString(),
           QStringLiteral("waiting_for_activity"));
  QCOMPARE(row.value(QStringLiteral("qualityClass")).toString(),
           QStringLiteral("unavailable"));
  QVERIFY(!row.value(QStringLiteral("hasUsefulData")).toBool());
  QVERIFY(row.value(QStringLiteral("quotaWindows")).toList().isEmpty());
  QCOMPARE(daily.summary().value(QStringLiteral("reportingUsefulSourceCount")).toInt(),
           0);
}

void DailyStateModelTest::staleLocalActivityIsLastKnownOnly() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  tool.setUsageLimit(100);
  tool.setCount(25);
  const QDateTime observed = QDateTime::currentDateTimeUtc();
  tool.recordActivityAt(observed);
  readiness.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.setPresentationTime(observed.addSecs(901));

  const QVariantMap row = daily.source(QStringLiteral("codex-cli"));
  QCOMPARE(row.value(QStringLiteral("readinessState")).toString(),
           QStringLiteral("degraded"));
  QCOMPARE(row.value(QStringLiteral("qualityClass")).toString(),
           QStringLiteral("unavailable"));
  QVERIFY(!row.value(QStringLiteral("hasEstimatedData")).toBool());
  QVERIFY(row.value(QStringLiteral("quotaWindows")).toList().isEmpty());
  const QVariantList lastKnown =
      row.value(QStringLiteral("lastKnownQuotaWindows")).toList();
  QVERIFY(!lastKnown.isEmpty());
  QCOMPARE(lastKnown.first().toMap()
               .value(QStringLiteral("freshnessState")).toString(),
           QStringLiteral("stale"));
  QCOMPARE(lastKnown.first().toMap()
               .value(QStringLiteral("percentUsed")).toDouble(),
           25.0);
}

void DailyStateModelTest::providerQuotaAgesWithPresentationClock() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  const auto start = QDateTime::currentDateTimeUtc();
  provider.addQuota(0, start.addSecs(3600));
  readiness.registerProviderBackend("openai", &provider);
  readiness.setSourceEnabled("openai", true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend("openai", &provider);
  daily.setPresentationTime(start.addSecs(1));
  QCOMPARE(daily.summary()
               .value("lowestActualRemainingQuota")
               .toMap()
               .value("percentRemaining")
               .toDouble(),
           0.0);
  daily.setPresentationTime(start.addSecs(901));
  QVERIFY(
      daily.summary().value("lowestActualRemainingQuota").toMap().isEmpty());
  QVERIFY(daily.summary().value("nearestActualReset").toMap().isEmpty());
  QVERIFY(!daily.source("openai")
               .value("lastKnownQuotaWindows")
               .toList()
               .isEmpty());
}

void DailyStateModelTest::quotaClockAndIndependentReset() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  const auto start = QDateTime::currentDateTimeUtc();
  tool.sync({QVariantMap{{"kind", "weekly"},
                         {"window", "weekly"},
                         {"source", "antigravity_local"},
                         {"percentRemaining", 10.0},
                         {"resetAt", start.addDays(5)}},
             QVariantMap{{"kind", "session"},
                         {"window", "five-hour"},
                         {"source", "antigravity_local"},
                         {"percentRemaining", 80.0},
                         {"resetAt", start.addSecs(3600)}}});
  readiness.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.setPresentationTime(start.addSecs(1));
  QCOMPARE(daily.summary()
               .value("lowestActualRemainingQuota")
               .toMap()
               .value("percentRemaining")
               .toDouble(),
           10.0);
  QCOMPARE(daily.summary()
               .value("nearestActualReset")
               .toMap()
               .value("resetAt")
               .toDateTime(),
           start.addSecs(3600));
  QVERIFY(tool.hasFreshQuota(start.addSecs(1)));
  const auto observation = tool.lastQuotaObservation();
  tool.recordActivity();
  tool.sync({});
  QCOMPARE(tool.lastQuotaObservation(), observation);
  daily.setPresentationTime(start.addSecs(901));
  QVERIFY(
      daily.summary().value("lowestActualRemainingQuota").toMap().isEmpty());
  QVERIFY(daily.summary().value("nearestActualReset").toMap().isEmpty());
  QVERIFY(!daily.source("codex-cli")
               .value("lastKnownQuotaWindows")
               .toList()
               .isEmpty());
}

void DailyStateModelTest::enabledSourcesAppearExactlyOnce() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  QCOMPARE(daily.summary().value(QStringLiteral("enabledSourceCount")).toInt(),
           0);
  DailyProvider provider;
  DailyTool tool;
  provider.makeReady();
  provider.addMetric(ProviderBackend::MetricKind::Requests, 3,
                     QStringLiteral("request"), QString(),
                     QStringLiteral("day"),
                     ProviderBackend::MetricSource::UsageApi);
  tool.setEnabled(true);
  tool.install();
  tool.recordActivity();

  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  readiness.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);
  daily.registerLocalTool(QStringLiteral("codex-cli"), &tool);

  QCOMPARE(daily.rowCount(), 2);
  const QStringList ids = daily.prioritizedSourceIds();
  QCOMPARE(QSet<QString>(ids.cbegin(), ids.cend()).size(), 2);
  QCOMPARE(daily.summary().value(QStringLiteral("enabledSourceCount")).toInt(),
           2);
  QCOMPARE(daily.summary()
               .value(QStringLiteral("reportingUsefulSourceCount"))
               .toInt(),
           2);
  QCOMPARE(daily.summary().value(QStringLiteral("actualSourceCount")).toInt(),
           1);
  QCOMPARE(
      daily.summary().value(QStringLiteral("estimatedSourceCount")).toInt(), 1);
}

void DailyStateModelTest::toolOnlySummaryIsComplete() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  tool.sync({QVariantMap{
      {QStringLiteral("kind"), QStringLiteral("rolling_5h")},
      {QStringLiteral("window"), QStringLiteral("5h")},
      {QStringLiteral("percentUsed"), 25.0},
      {QStringLiteral("percentRemaining"), 75.0},
      {QStringLiteral("source"), QStringLiteral("browser_sync")},
      {QStringLiteral("precision"), QStringLiteral("browser_sync_actual")},
      {QStringLiteral("resetAt"),
       QDateTime::currentDateTimeUtc().addSecs(1800)}}});
  readiness.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool(QStringLiteral("codex-cli"), &tool);

  const QVariantMap summary = daily.summary();
  QCOMPARE(summary.value(QStringLiteral("enabledSourceCount")).toInt(), 1);
  QCOMPARE(summary.value(QStringLiteral("reportingUsefulSourceCount")).toInt(),
           1);
  QCOMPARE(summary.value(QStringLiteral("actualSourceCount")).toInt(), 1);
  QCOMPARE(summary.value(QStringLiteral("lowestRemainingQuota"))
               .toMap()
               .value(QStringLiteral("stableId"))
               .toString(),
           QStringLiteral("codex-cli"));
  QVERIFY(summary.value(QStringLiteral("nearestReset"))
              .toMap()
              .value(QStringLiteral("resetAt"))
              .toDateTime()
              .isValid());
}

void DailyStateModelTest::unavailableAndAvailableZeroStayDistinct() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  QVariantMap row = daily.source(QStringLiteral("openai"));
  QVERIFY(!row.value(QStringLiteral("primaryMetricAvailable")).toBool());
  QVERIFY(!row.value(QStringLiteral("costAvailable")).toBool());
  QVERIFY(!row.value(QStringLiteral("primaryMetricValue")).isValid());

  provider.addMetric(ProviderBackend::MetricKind::Cost, 0.0,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("current"),
                     ProviderBackend::MetricSource::BillingApi);
  row = daily.source(QStringLiteral("openai"));
  QVERIFY(row.value(QStringLiteral("primaryMetricAvailable")).toBool());
  QCOMPARE(row.value(QStringLiteral("primaryMetricValue")).toDouble(), 0.0);
  QVERIFY(row.value(QStringLiteral("costAvailable")).toBool());
  QCOMPARE(row.value(QStringLiteral("costValue")).toDouble(), 0.0);
}

void DailyStateModelTest::expiredCostRemainsLastKnownButDoesNotAggregate() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  const QDateTime observed = QDateTime::currentDateTimeUtc();
  provider.addMetric(ProviderBackend::MetricKind::Cost, 7.25,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("month"),
                     ProviderBackend::MetricSource::BillingApi);
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);
  daily.setPresentationTime(observed.addSecs(901));

  const QVariantMap row = daily.source(QStringLiteral("openai"));
  QVERIFY(!row.value(QStringLiteral("costAvailable")).toBool());
  QVERIFY(!row.value(QStringLiteral("hasActualData")).toBool());
  QVERIFY(daily.summary().value(QStringLiteral("actualSpendTotals")).toMap().isEmpty());
  bool foundLastKnown = false;
  for (const QVariant &entry : row.value(QStringLiteral("detailMetrics")).toList()) {
    const QVariantMap metric = entry.toMap();
    if (metric.value(QStringLiteral("kind")) != QLatin1String("cost"))
      continue;
    QVERIFY(!metric.value(QStringLiteral("available")).toBool());
    QVERIFY(metric.value(QStringLiteral("lastKnownAvailable")).toBool());
    QCOMPARE(metric.value(QStringLiteral("lastKnownValue")).toDouble(), 7.25);
    QCOMPARE(metric.value(QStringLiteral("freshnessState")).toString(),
             QStringLiteral("stale"));
    foundLastKnown = true;
  }
  QVERIFY(foundLastKnown);
}

void DailyStateModelTest::retryAfterIsPresentInSourceDetail() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  const QDateTime retryAt = QDateTime::currentDateTimeUtc().addSecs(1800);
  provider.failRateLimited(retryAt);
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  SourceDetailModel detail;
  detail.registerDailyState(&daily);
  detail.setSourceId(QStringLiteral("openai"));
  QCOMPARE(detail.source().value(QStringLiteral("retryAfter")).toDateTime(),
           retryAt);
  QCOMPARE(detail.actionLabel(), QStringLiteral("Refresh"));
}

void DailyStateModelTest::
    providerToolMixedCurrencyAndFeesAggregateSeparately() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider usd;
  DailyProvider eur;
  DailyProvider estimate;
  DailyTool tool;
  usd.makeReady();
  eur.makeReady();
  estimate.makeReady();
  usd.addMetric(ProviderBackend::MetricKind::Cost, 10.0, QStringLiteral("USD"),
                QStringLiteral("USD"), QStringLiteral("current"),
                ProviderBackend::MetricSource::BillingApi);
  eur.addMetric(ProviderBackend::MetricKind::Cost, 4.0, QStringLiteral("EUR"),
                QStringLiteral("EUR"), QStringLiteral("current"),
                ProviderBackend::MetricSource::BillingApi);
  estimate.addMetric(ProviderBackend::MetricKind::Cost, 2.5,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("current"),
                     ProviderBackend::MetricSource::EstimatedPricing);
  tool.setEnabled(true);
  tool.install();
  tool.setPlanTier(QStringLiteral("pro"));
  tool.recordActivity();

  readiness.registerProviderBackend(QStringLiteral("openai"), &usd);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  readiness.registerProviderBackend(QStringLiteral("anthropic"), &eur);
  readiness.setSourceEnabled(QStringLiteral("anthropic"), true);
  readiness.registerProviderBackend(QStringLiteral("google"), &estimate);
  readiness.setSourceEnabled(QStringLiteral("google"), true);
  readiness.registerLocalTool(QStringLiteral("claude-code"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &usd);
  daily.registerProviderBackend(QStringLiteral("anthropic"), &eur);
  daily.registerProviderBackend(QStringLiteral("google"), &estimate);
  daily.registerLocalTool(QStringLiteral("claude-code"), &tool);

  const QVariantMap summary = daily.summary();
  const QVariantMap actual =
      summary.value(QStringLiteral("actualSpendTotals")).toMap();
  QCOMPARE(actual.size(), 2);
  QCOMPARE(actual.value(QStringLiteral("USD")).toDouble(), 10.0);
  QCOMPARE(actual.value(QStringLiteral("EUR")).toDouble(), 4.0);
  const QVariantMap fixedFees =
      summary.value(QStringLiteral("fixedSubscriptionFees")).toMap();
  QCOMPARE(fixedFees.value(QStringLiteral("USD")).toDouble(), 20.0);
  QCOMPARE(summary.value(QStringLiteral("estimatedSpendTotals"))
               .toMap()
               .value(QStringLiteral("USD"))
               .toDouble(),
           2.5);
}

void DailyStateModelTest::rangePricedToolPreservesRange() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyTool tool;
  tool.setEnabled(true);
  tool.install();
  tool.setPlanTier(QStringLiteral("ai_pro"));

  readiness.registerLocalTool(QStringLiteral("jetbrains-ai"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerLocalTool(QStringLiteral("jetbrains-ai"), &tool);

  const QVariantMap fixedFees =
      daily.summary().value(QStringLiteral("fixedSubscriptionFees")).toMap();
  QVERIFY(fixedFees.isEmpty());
  const auto ranges = daily.summary()
                          .value(QStringLiteral("fixedSubscriptionFeeRanges"))
                          .toList();
  QCOMPARE(ranges.size(), 1);
  QCOMPARE(ranges.first().toMap().value(QStringLiteral("rangeMin")).toDouble(),
           10.0);
  QVERIFY(ranges.first().toMap().value(QStringLiteral("rangeMax")).toDouble() >
          10.0);
}

void DailyStateModelTest::staleBalanceAndConnectivityRemainDistinct() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider stale;
  DailyProvider balance;
  DailyProvider connectivity;
  stale.makeReady();
  stale.addMetric(ProviderBackend::MetricKind::Requests, 2,
                  QStringLiteral("request"), QString(), QStringLiteral("day"),
                  ProviderBackend::MetricSource::UsageApi);
  stale.makeStale();
  balance.makeReady();
  balance.addMetric(ProviderBackend::MetricKind::CreditBalance, 12.0,
                    QStringLiteral("USD"), QStringLiteral("USD"),
                    QStringLiteral("current"),
                    ProviderBackend::MetricSource::BillingApi);
  connectivity.makeReady();

  const QList<QPair<QString, DailyProvider *>> providers{
      {QStringLiteral("openai"), &stale},
      {QStringLiteral("deepseek"), &balance},
      {QStringLiteral("anthropic"), &connectivity}};
  for (const auto &[id, provider] : providers) {
    readiness.registerProviderBackend(id, provider);
    readiness.setSourceEnabled(id, true);
  }
  daily.registerReadinessModel(&readiness);
  for (const auto &[id, provider] : providers)
    daily.registerProviderBackend(id, provider);

  QCOMPARE(daily.source(QStringLiteral("openai"))
               .value(QStringLiteral("freshnessState"))
               .toString(),
           QStringLiteral("stale"));
  QCOMPARE(daily.source(QStringLiteral("openai"))
               .value(QStringLiteral("attentionReasonKey"))
               .toString(),
           QStringLiteral("stale_data"));
  QVERIFY(daily.source(QStringLiteral("deepseek"))
              .value(QStringLiteral("hasBalance"))
              .toBool());
  QCOMPARE(daily.source(QStringLiteral("deepseek"))
               .value(QStringLiteral("qualityClass"))
               .toString(),
           QStringLiteral("balance"));
  QVERIFY(daily.source(QStringLiteral("anthropic"))
              .value(QStringLiteral("connectivityOnly"))
              .toBool());
  QCOMPARE(daily.summary().value(QStringLiteral("staleSourceCount")).toInt(),
           1);
  QCOMPARE(daily.summary().value(QStringLiteral("balanceSourceCount")).toInt(),
           1);
  QCOMPARE(daily.summary()
               .value(QStringLiteral("connectivityOnlySourceCount"))
               .toInt(),
           1);
}

void DailyStateModelTest::priorityRules_data() {
  QTest::addColumn<double>("remaining");
  QTest::addColumn<QString>("severity");
  QTest::addColumn<QString>("reason");
  QTest::newRow("exhausted")
      << 0.0 << QStringLiteral("critical") << QStringLiteral("quota_exhausted");
  QTest::newRow("critical")
      << 3.0 << QStringLiteral("critical") << QStringLiteral("quota_critical");
  QTest::newRow("warning") << 15.0 << QStringLiteral("warning")
                           << QStringLiteral("quota_warning");
  QTest::newRow("normal") << 40.0 << QStringLiteral("none")
                          << QStringLiteral("none");
}

void DailyStateModelTest::priorityRules() {
  QFETCH(double, remaining);
  QFETCH(QString, severity);
  QFETCH(QString, reason);
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.addQuota(remaining, QDateTime::currentDateTimeUtc().addSecs(3600));
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  const QVariantMap row = daily.source(QStringLiteral("openai"));
  QCOMPARE(row.value(QStringLiteral("attentionSeverity")).toString(), severity);
  QCOMPARE(row.value(QStringLiteral("attentionReasonKey")).toString(), reason);
  QCOMPARE(row.value(QStringLiteral("percentRemaining")).toDouble(), remaining);
}

void DailyStateModelTest::budgetPriorityRequiresTypedCost() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.setDailySpend(8.0, 10.0);
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  QVariantMap row = daily.source(QStringLiteral("openai"));
  QVERIFY(row.value(QStringLiteral("budgetAvailable")).toBool());
  QCOMPARE(row.value(QStringLiteral("budgetPercentUsed")).toDouble(), 80.0);
  QCOMPARE(row.value(QStringLiteral("attentionReasonKey")).toString(),
           QStringLiteral("budget_warning"));

  provider.setDailySpend(10.0, 10.0);
  row = daily.source(QStringLiteral("openai"));
  QCOMPARE(row.value(QStringLiteral("attentionSeverity")).toString(),
           QStringLiteral("critical"));
  QCOMPARE(row.value(QStringLiteral("attentionReasonKey")).toString(),
           QStringLiteral("budget_critical"));
}

void DailyStateModelTest::monthlyBudgetUsesBackendBillingValue() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.setMonthlySpend(80.0, 100.0);
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  const QVariantMap row = daily.source(QStringLiteral("openai"));
  QVERIFY(row.value(QStringLiteral("budgetAvailable")).toBool());
  QCOMPARE(row.value(QStringLiteral("budgetPercentUsed")).toDouble(), 80.0);
  QCOMPARE(row.value(QStringLiteral("attentionReasonKey")).toString(),
           QStringLiteral("budget_warning"));
}

void DailyStateModelTest::priorityTiesAreDeterministic() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider openai;
  DailyProvider anthropic;
  const QDateTime reset = QDateTime::currentDateTimeUtc().addSecs(3600);
  openai.makeReady();
  anthropic.makeReady();
  openai.addQuota(3.0, reset);
  anthropic.addQuota(3.0, reset);
  readiness.registerProviderBackend(QStringLiteral("openai"), &openai);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  readiness.registerProviderBackend(QStringLiteral("anthropic"), &anthropic);
  readiness.setSourceEnabled(QStringLiteral("anthropic"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &openai);
  daily.registerProviderBackend(QStringLiteral("anthropic"), &anthropic);

  const QStringList first = daily.prioritizedSourceIds();
  daily.refresh();
  QCOMPARE(daily.prioritizedSourceIds(), first);
  QCOMPARE(first.first(), QStringLiteral("openai"));

  anthropic.failAuthentication();
  QCOMPARE(daily.prioritizedSourceIds().first(), QStringLiteral("anthropic"));
  QCOMPARE(
      daily.summary().value(QStringLiteral("mostUrgentSourceId")).toString(),
      QStringLiteral("anthropic"));
}

void DailyStateModelTest::panelAggregatesUseTypedDailyMetrics() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.addMetric(ProviderBackend::MetricKind::Cost, 0.0,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("current"),
                     ProviderBackend::MetricSource::BillingApi);
  provider.addMetric(ProviderBackend::MetricKind::Cost, 1.5,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("day"),
                     ProviderBackend::MetricSource::BillingApi);
  provider.addQuota(0.0, QDateTime::currentDateTimeUtc().addSecs(3600));
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  const QVariantMap summary = daily.summary();
  QCOMPARE(summary.value(QStringLiteral("providerActualSpendTotals"))
               .toMap()
               .value(QStringLiteral("USD"))
               .toDouble(),
           0.0);
  QCOMPARE(summary.value(QStringLiteral("providerDailyActualSpendTotals"))
               .toMap()
               .value(QStringLiteral("USD"))
               .toDouble(),
           1.5);
  const QVariantMap requests =
      summary.value(QStringLiteral("remainingRequests")).toMap();
  QCOMPARE(requests.value(QStringLiteral("stableId")).toString(),
           QStringLiteral("openai"));
  QCOMPARE(requests.value(QStringLiteral("value")).toDouble(), 0.0);
  QCOMPARE(summary.value(QStringLiteral("lowestActualRemainingQuota"))
               .toMap()
               .value(QStringLiteral("stableId"))
               .toString(),
           QStringLiteral("openai"));
}

void DailyStateModelTest::scopedRowsNeverDoubleDailyTotals() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.addMetric(ProviderBackend::MetricKind::Cost, 10.0,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("day"),
                     ProviderBackend::MetricSource::BillingApi);
  provider.addScopedCost(6.0, QStringLiteral("gpt-a"),
                         QStringLiteral("project-a"));
  provider.addScopedCost(4.0, QStringLiteral("gpt-b"),
                         QStringLiteral("project-b"));
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  const QVariantMap source = daily.source(QStringLiteral("openai"));
  QCOMPARE(source.value(QStringLiteral("costValue")).toDouble(), 10.0);
  QCOMPARE(daily.summary()
               .value(QStringLiteral("providerDailyActualSpendTotals"))
               .toMap()
               .value(QStringLiteral("USD"))
               .toDouble(),
           10.0);

  SourceDetailModel detail;
  detail.registerDailyState(&daily);
  detail.setSourceId(QStringLiteral("openai"));
  QCOMPARE(detail.scopeBreakdown()
               .value(QStringLiteral("scopedRows"))
               .toList()
               .size(),
           2);
  QCOMPARE(detail.scopeBreakdown()
               .value(QStringLiteral("reconciliations"))
               .toList()
               .first()
               .toMap()
               .value(QStringLiteral("status"))
               .toString(),
           QStringLiteral("exact"));
  bool foundProject = false;
  for (int row = 0; row < detail.rowCount(); ++row) {
    const QModelIndex index = detail.index(row, 0);
    if (detail.data(index, SourceDetailModel::ProjectScopeRole).toString()
        == QLatin1String("project-a")) {
      foundProject = true;
      QVERIFY(
          detail.data(index, SourceDetailModel::ProjectScopeAvailableRole)
              .toBool());
      QCOMPARE(
          detail.data(index, SourceDetailModel::ModelScopeRole).toString(),
          QStringLiteral("gpt-a"));
      QCOMPARE(
          detail.data(index, SourceDetailModel::AggregationLevelRole)
              .toString(),
          QStringLiteral("scoped"));
    }
  }
  QVERIFY(foundProject);
}

void DailyStateModelTest::documentedAndLocalLimitsAreNotLiveQuota() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  DailyTool tool;
  provider.makeReady();
  provider.addMetric(ProviderBackend::MetricKind::RequestLimit, 100.0,
                     QStringLiteral("request"), QString(),
                     QStringLiteral("rolling"),
                     ProviderBackend::MetricSource::PublishedDocumentation);
  provider.addMetric(ProviderBackend::MetricKind::RequestRemaining, 10.0,
                     QStringLiteral("request"), QString(),
                     QStringLiteral("rolling"),
                     ProviderBackend::MetricSource::ResponseHeaders,
                     QDateTime::currentDateTimeUtc().addSecs(3600));
  tool.setEnabled(true);
  tool.install();
  tool.setUsageLimit(100);
  tool.recordActivity();
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  readiness.registerLocalTool(QStringLiteral("codex-cli"), &tool);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);
  daily.registerLocalTool(QStringLiteral("codex-cli"), &tool);

  const QVariantMap summary = daily.summary();
  QVERIFY(summary.value(QStringLiteral("lowestActualRemainingQuota"))
              .toMap()
              .isEmpty());
  QVERIFY(
      summary.value(QStringLiteral("nearestActualReset")).toMap().isEmpty());
  QVERIFY(summary.value(QStringLiteral("remainingRequests")).toMap().isEmpty());
  QVERIFY(
      !summary.value(QStringLiteral("lowestRemainingQuota")).toMap().isEmpty());
  const QVariantList windows = daily.source(QStringLiteral("openai"))
                                   .value(QStringLiteral("quotaWindows"))
                                   .toList();
  QCOMPARE(windows.size(), 1);
  QCOMPARE(
      windows.first().toMap().value(QStringLiteral("sourceClass")).toString(),
      QStringLiteral("configured_limit"));
}

void DailyStateModelTest::quotaWindowsPreserveSourceAndReset() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  const QDateTime requestReset = QDateTime::currentDateTimeUtc().addSecs(1800);
  const QDateTime tokenReset = QDateTime::currentDateTimeUtc().addSecs(3600);
  provider.makeReady();
  provider.addQuota(4.0, requestReset);
  provider.addTokenQuota(20.0, tokenReset);
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  const QVariantList windows = daily.source(QStringLiteral("openai"))
                                   .value(QStringLiteral("quotaWindows"))
                                   .toList();
  QCOMPARE(windows.size(), 2);
  for (const QVariant &entry : windows) {
    const QVariantMap window = entry.toMap();
    QCOMPARE(window.value(QStringLiteral("sourceClass")).toString(),
             QStringLiteral("actual"));
    QCOMPARE(window.value(QStringLiteral("sourceKey")).toString(),
             QStringLiteral("response_headers"));
    QVERIFY(window.value(QStringLiteral("resetAt")).toDateTime().isValid());
  }
}

void DailyStateModelTest::staleSnapshotsPrioritizeFreshnessOverCachedQuota() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.addQuota(0.0, QDateTime::currentDateTimeUtc().addSecs(3600));
  provider.makeStale();
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);

  const QVariantMap row = daily.source(QStringLiteral("openai"));
  QCOMPARE(row.value(QStringLiteral("freshnessState")).toString(),
           QStringLiteral("stale"));
  QCOMPARE(row.value(QStringLiteral("attentionSeverity")).toString(),
           QStringLiteral("warning"));
  QCOMPARE(row.value(QStringLiteral("attentionReasonKey")).toString(),
           QStringLiteral("stale_data"));
  QVERIFY(!row.value(QStringLiteral("quotaWindows")).toList().isEmpty());
}

void DailyStateModelTest::sourceDetailPreservesTypedMetricsAndConcreteAction() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider provider;
  provider.makeReady();
  provider.addMetric(ProviderBackend::MetricKind::Cost, 0.0,
                     QStringLiteral("USD"), QStringLiteral("USD"),
                     QStringLiteral("day"),
                     ProviderBackend::MetricSource::BillingApi);
  provider.addQuota(4.0, QDateTime::currentDateTimeUtc().addSecs(3600));
  readiness.registerProviderBackend(QStringLiteral("openai"), &provider);
  readiness.setSourceEnabled(QStringLiteral("openai"), true);
  daily.registerReadinessModel(&readiness);
  daily.registerProviderBackend(QStringLiteral("openai"), &provider);
  daily.setHistoryIdentity(QStringLiteral("openai"), QStringLiteral("OpenAI"));

  SourceDetailModel detail;
  detail.registerDailyState(&daily);
  detail.setSourceId(QStringLiteral("openai"));

  QCOMPARE(detail.source().value(QStringLiteral("stableId")).toString(),
           QStringLiteral("openai"));
  QCOMPARE(detail.actionLabel(), QStringLiteral("Review quota"));
  QCOMPARE(detail.historyId(), QStringLiteral("provider:OpenAI"));
  QCOMPARE(detail.historyMetric(), QStringLiteral("cost"));
  QVERIFY(detail.rowCount() >= 3);
  QCOMPARE(detail.coverage().value(QStringLiteral("actualMetricCount")).toInt(),
           3);
  QCOMPARE(
      detail.coverage().value(QStringLiteral("estimatedMetricCount")).toInt(),
      0);
  bool foundAvailableZero = false;
  for (int row = 0; row < detail.rowCount(); ++row) {
    const QModelIndex index = detail.index(row, 0);
    if (detail.data(index, SourceDetailModel::KindRole).toString() ==
        QLatin1String("cost")) {
      QVERIFY(detail.data(index, SourceDetailModel::AvailableRole).toBool());
      QCOMPARE(detail.data(index, SourceDetailModel::ValueRole).toDouble(),
               0.0);
      foundAvailableZero = true;
    }
  }
  QVERIFY(foundAvailableZero);
}

void DailyStateModelTest::normalSourcesSortByReportingQuality() {
  SourceReadinessModel readiness;
  DailyStateModel daily;
  DailyProvider connection;
  DailyProvider estimate;
  DailyProvider actual;
  connection.makeReady();
  estimate.makeReady();
  estimate.addMetric(ProviderBackend::MetricKind::Requests, 2,
                     QStringLiteral("request"), QString(),
                     QStringLiteral("day"),
                     ProviderBackend::MetricSource::EstimatedPricing);
  actual.makeReady();
  actual.addMetric(ProviderBackend::MetricKind::Requests, 2,
                   QStringLiteral("request"), QString(), QStringLiteral("day"),
                   ProviderBackend::MetricSource::UsageApi);

  const QList<QPair<QString, DailyProvider *>> providers{
      {QStringLiteral("anthropic"), &connection},
      {QStringLiteral("google"), &estimate},
      {QStringLiteral("openai"), &actual}};
  for (const auto &[id, provider] : providers) {
    readiness.registerProviderBackend(id, provider);
    readiness.setSourceEnabled(id, true);
  }
  daily.registerReadinessModel(&readiness);
  for (const auto &[id, provider] : providers)
    daily.registerProviderBackend(id, provider);

  QCOMPARE(daily.prioritizedSourceIds(),
           QStringList({QStringLiteral("openai"), QStringLiteral("google"),
                        QStringLiteral("anthropic")}));
}

QTEST_MAIN(DailyStateModelTest)
#include "test_dailystatemodel.moc"
