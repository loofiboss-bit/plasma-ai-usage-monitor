#ifndef BUDGETPACINGQUERY_H
#define BUDGETPACINGQUERY_H

#include "billingcycleresolver.h"
#include "forecastcontract.h"

#include <QList>
#include <optional>

class BudgetPacingQuery final {
public:
  struct Observation {
    QDateTime observedAt;
    QDateTime intervalStart;
    QDateTime intervalEnd;
    std::optional<qint64> valueMinor;
    QString currency;
    ForecastContract::ValueClass valueClass =
        ForecastContract::ValueClass::Actual;
    QString scopeKind;
    QString scopeIdentity;
  };

  struct Request {
    QString policyId;
    QString sourceId;
    QString sourceKind;
    QString scopeMode = QStringLiteral("aggregate");
    QString scopeKind;
    QString scopeIdentity;
    QString scopeLabel;
    ForecastContract::ValueClass valueClass =
        ForecastContract::ValueClass::Actual;
    qint64 limitMinor = 0;
    QString currency;
    int warningPercent = 80;
    int criticalPercent = 90;
    QString periodType = QStringLiteral("calendar_month");
    QString timeZoneId;
    BillingCycleResolver::Cycle cycle;
    BillingCycleResolver::Cycle previousCycle;
    QDateTime generatedAt;
    QString preflightReason;
    QList<Observation> observations;
  };

  static ForecastContract::Result evaluate(const Request &request);

  static constexpr int MinimumSamples = 5;
  static constexpr double MinimumCoveragePercent = 70.0;
};

#endif
