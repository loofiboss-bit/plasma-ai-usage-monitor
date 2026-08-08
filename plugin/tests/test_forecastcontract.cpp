#include <QtTest>

#include "forecastcontract.h"

namespace {
ForecastContract::Result availableForecast(ForecastContract::State state) {
  ForecastContract::Result result;
  result.kind = ForecastContract::Kind::QuotaExhaustion;
  result.state = state;
  result.sourceId = QStringLiteral("provider:openai");
  result.sourceKind = QStringLiteral("provider");
  result.window = QStringLiteral("requests:2026-07-29T12:00:00Z");
  result.scope = QStringLiteral("account");
  result.currentValue = 40.0;
  result.projectedValue = 95.0;
  result.limitValue = 100.0;
  result.unit = QStringLiteral("request");
  result.periodEnd =
      QDateTime(QDate(2026, 7, 29), QTime(12, 0), QTimeZone::UTC);
  result.sampleCount = 8;
  result.coveragePercent = 88.0;
  result.evidenceGrade = ForecastContract::EvidenceGrade::Strong;
  result.methodId = QStringLiteral("quota-runway-v1");
  result.generatedAt =
      QDateTime(QDate(2026, 7, 29), QTime(9, 0), QTimeZone::UTC);
  return result;
}
} // namespace

class ForecastContractTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void availableStates_data();
  void availableStates();
  void unavailableReasons_data();
  void unavailableReasons();
  void missingValueDiffersFromZero();
  void valueClassAndCurrencyRoundTrip();
  void budgetPacingV2RoundTrip();
  void invalidContractsFailClosed();
  void stableIdentityAndTransitions();
};

void ForecastContractTest::availableStates_data() {
  QTest::addColumn<int>("state");
  QTest::addColumn<QString>("key");

  QTest::newRow("safe") << int(ForecastContract::State::Safe)
                        << QStringLiteral("safe");
  QTest::newRow("warning") << int(ForecastContract::State::Warning)
                           << QStringLiteral("warning");
  QTest::newRow("critical")
      << int(ForecastContract::State::Critical) << QStringLiteral("critical");
  QTest::newRow("exceeded")
      << int(ForecastContract::State::Exceeded) << QStringLiteral("exceeded");
}

void ForecastContractTest::availableStates() {
  QFETCH(int, state);
  QFETCH(QString, key);
  ForecastContract::Result result =
      availableForecast(ForecastContract::State(state));
  if (result.state != ForecastContract::State::Safe) {
    result.predictedAt = result.generatedAt.addSecs(3600);
  }

  QString diagnostic;
  QVERIFY2(result.isValid(&diagnostic), qPrintable(diagnostic));
  const QVariantMap map = result.toVariantMap();
  QCOMPARE(map.value(QStringLiteral("state")).toString(), key);
  QCOMPARE(map.value(QStringLiteral("currentValue")).toDouble(), 40.0);
  QVERIFY(map.contains(QStringLiteral("currency")));
  QVERIFY(map.value(QStringLiteral("currency")).isNull());

  const auto roundTrip = ForecastContract::fromVariantMap(map, &diagnostic);
  QVERIFY2(roundTrip.has_value(), qPrintable(diagnostic));
  QCOMPARE(roundTrip->state, result.state);
  QCOMPARE(roundTrip->stableId(), result.stableId());
}

void ForecastContractTest::unavailableReasons_data() {
  QTest::addColumn<QString>("reason");
  for (const QString &reason : ForecastContract::unavailableReasonKeys()) {
    QTest::newRow(qPrintable(reason)) << reason;
  }
}

void ForecastContractTest::unavailableReasons() {
  QFETCH(QString, reason);
  ForecastContract::Result result =
      availableForecast(ForecastContract::State::Unavailable);
  result.currentValue.reset();
  result.projectedValue.reset();
  result.limitValue.reset();
  result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
  result.reasonKey = reason;

  QString diagnostic;
  QVERIFY2(result.isValid(&diagnostic), qPrintable(diagnostic));
  const auto roundTrip =
      ForecastContract::fromVariantMap(result.toVariantMap(), &diagnostic);
  QVERIFY2(roundTrip.has_value(), qPrintable(diagnostic));
  QCOMPARE(roundTrip->reasonKey, reason);
  QVERIFY(!roundTrip->currentValue.has_value());
}

void ForecastContractTest::missingValueDiffersFromZero() {
  ForecastContract::Result result =
      availableForecast(ForecastContract::State::Safe);
  result.currentValue = 0.0;
  const QVariantMap map = result.toVariantMap();
  QVERIFY(map.value(QStringLiteral("currentValue")).isValid());
  QVERIFY(!map.value(QStringLiteral("currentValue")).isNull());
  QCOMPARE(map.value(QStringLiteral("currentValue")).toDouble(), 0.0);

  result.state = ForecastContract::State::Unavailable;
  result.currentValue.reset();
  result.projectedValue.reset();
  result.limitValue.reset();
  result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
  result.reasonKey = QStringLiteral("missing_value");
  const QVariantMap unavailable = result.toVariantMap();
  QVERIFY(unavailable.contains(QStringLiteral("currentValue")));
  QVERIFY(unavailable.value(QStringLiteral("currentValue")).isNull());
}

void ForecastContractTest::valueClassAndCurrencyRoundTrip() {
  ForecastContract::Result result =
      availableForecast(ForecastContract::State::Warning);
  result.kind = ForecastContract::Kind::BudgetOverrun;
  result.window = QStringLiteral("2026-07");
  result.unit = QStringLiteral("USD");
  result.currency = QStringLiteral("USD");
  result.valueClass = ForecastContract::ValueClass::Estimated;
  result.methodId = QStringLiteral("budget-pacing-v1");
  result.predictedAt = result.generatedAt.addDays(3);

  const auto parsed = ForecastContract::fromVariantMap(result.toVariantMap());
  QVERIFY(parsed.has_value());
  QCOMPARE(parsed->kind, ForecastContract::Kind::BudgetOverrun);
  QCOMPARE(parsed->currency, std::optional<QString>(QStringLiteral("USD")));
  QCOMPARE(parsed->valueClass, ForecastContract::ValueClass::Estimated);
}

void ForecastContractTest::budgetPacingV2RoundTrip() {
  ForecastContract::Result result =
      availableForecast(ForecastContract::State::Warning);
  result.contractVersion = QStringLiteral("budget-pacing-v2");
  result.kind = ForecastContract::Kind::BudgetOverrun;
  result.policyId = QStringLiteral("11111111-1111-4111-8111-111111111111");
  result.window = QStringLiteral("policy_period");
  result.scope = QStringLiteral("aggregate");
  result.currency = QStringLiteral("USD");
  result.unit = QStringLiteral("USD");
  result.periodStart = result.generatedAt.addDays(-3);
  result.periodEnd = result.generatedAt.addDays(20);
  result.methodId = QStringLiteral("budget-pacing-v2");
  result.spentMinor = 4200;
  result.remainingMinor = 5800;
  result.consumedPercent = 42.0;
  result.projectedPeriodEndMinor = 11000;
  result.predictedOverrun = true;
  result.safeTodayMinor = 300;
  result.remainingDailyAllowanceMinor = 290;
  result.previousPeriodSpentMinor = 3900;
  result.previousPeriodChangePercent = 7.69;
  result.predictedAt = result.generatedAt.addDays(10);
  QString diagnostic;
  QVERIFY2(result.isValid(&diagnostic), qPrintable(diagnostic));
  const auto parsed =
      ForecastContract::fromVariantMap(result.toVariantMap(), &diagnostic);
  QVERIFY2(parsed.has_value(), qPrintable(diagnostic));
  QCOMPARE(parsed->policyId, result.policyId);
  QCOMPARE(parsed->spentMinor, result.spentMinor);
  QCOMPARE(parsed->safeTodayMinor, result.safeTodayMinor);
  const QString stableId = parsed->stableId();
  result.scope = QStringLiteral("project:Renamed label");
  result.currency = QStringLiteral("EUR");
  QCOMPARE(result.stableId(), stableId);
}

void ForecastContractTest::invalidContractsFailClosed() {
  QString diagnostic;
  ForecastContract::Result result =
      availableForecast(ForecastContract::State::Warning);
  result.currentValue.reset();
  QVERIFY(!result.isValid(&diagnostic));
  QVERIFY(diagnostic.contains(QStringLiteral("incomplete")));

  result = availableForecast(ForecastContract::State::Unavailable);
  result.currentValue.reset();
  result.projectedValue.reset();
  result.limitValue.reset();
  result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
  result.reasonKey = QStringLiteral("invented_reason");
  QVERIFY(!result.isValid(&diagnostic));

  QVariantMap map =
      availableForecast(ForecastContract::State::Safe).toVariantMap();
  map.insert(QStringLiteral("coveragePercent"), 101.0);
  QVERIFY(!ForecastContract::fromVariantMap(map, &diagnostic).has_value());

  map = availableForecast(ForecastContract::State::Safe).toVariantMap();
  map.remove(QStringLiteral("currency"));
  QVERIFY(!ForecastContract::fromVariantMap(map, &diagnostic).has_value());
  QVERIFY(diagnostic.contains(QStringLiteral("currency")));

  result = availableForecast(ForecastContract::State::Safe);
  result.currentValue = -1.0;
  QVERIFY(!result.isValid(&diagnostic));
}

void ForecastContractTest::stableIdentityAndTransitions() {
  ForecastContract::Result result =
      availableForecast(ForecastContract::State::Safe);
  const QString stableId = result.stableId();
  QCOMPARE(stableId.size(), 64);
  result.generatedAt = result.generatedAt.addSecs(60);
  result.currentValue = 50.0;
  QCOMPARE(result.stableId(), stableId);
  result.scope = QStringLiteral("project:local-id");
  QVERIFY(result.stableId() != stableId);
  result.scope = QStringLiteral("account");
  result.valueClass = ForecastContract::ValueClass::Estimated;
  QVERIFY(result.stableId() != stableId);

  using State = ForecastContract::State;
  QCOMPARE(ForecastContract::transitionFor(State::Safe, State::Warning),
           QStringLiteral("warning"));
  QCOMPARE(ForecastContract::transitionFor(State::Warning, State::Warning),
           QString());
  QCOMPARE(ForecastContract::transitionFor(State::Warning, State::Critical),
           QStringLiteral("critical"));
  QCOMPARE(ForecastContract::transitionFor(State::Critical, State::Safe),
           QStringLiteral("recovered"));
  QCOMPARE(ForecastContract::transitionFor(State::Critical, State::Unavailable),
           QString());
  QCOMPARE(ForecastContract::transitionFor(State::Critical, State::Exceeded),
           QStringLiteral("exceeded"));
  QCOMPARE(ForecastContract::transitionFor(State::Exceeded, State::Safe),
           QStringLiteral("recovered"));
  QCOMPARE(ForecastContract::transitionFor(State::Safe, State::Safe),
           QString());
}

QTEST_MAIN(ForecastContractTest)
#include "test_forecastcontract.moc"
