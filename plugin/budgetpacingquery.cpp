#include "budgetpacingquery.h"

#include "currencyminorunits.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace {
constexpr double Epsilon = 1e-9;

double median(QList<qint64> values) {
  if (values.isEmpty())
    return 0.0;
  std::sort(values.begin(), values.end());
  const qsizetype middle = values.size() / 2;
  if (values.size() % 2 == 0)
    return (static_cast<double>(values.at(middle - 1)) +
            static_cast<double>(values.at(middle))) /
           2.0;
  return static_cast<double>(values.at(middle));
}

bool isCompleteUtcDay(const BudgetPacingQuery::Observation &observation,
                      const QDateTime &todayStartUtc) {
  const QDateTime start = observation.intervalStart.toUTC();
  const QDateTime end = observation.intervalEnd.toUTC();
  return start.isValid() && end.isValid() && start.time() == QTime(0, 0) &&
         end == start.addDays(1) && end <= todayStartUtc;
}

ForecastContract::Result unavailable(const BudgetPacingQuery::Request &request,
                                     const QString &reason, int samples = 0,
                                     double coverage = 0.0) {
  ForecastContract::Result result;
  result.contractVersion = QStringLiteral("budget-pacing-v2");
  result.kind = ForecastContract::Kind::BudgetOverrun;
  result.state = ForecastContract::State::Unavailable;
  result.policyId = request.policyId;
  result.sourceId = request.sourceId;
  result.sourceKind = request.sourceKind;
  result.window = request.periodType;
  result.scope = request.scopeMode == QLatin1String("scoped")
                     ? request.scopeKind + QLatin1Char(':') + request.scopeLabel
                     : QStringLiteral("aggregate");
  result.currency = request.currency.trimmed().toUpper();
  result.unit =
      result.currency->isEmpty() ? QStringLiteral("unknown") : *result.currency;
  result.periodStart = request.cycle.startUtc;
  result.periodEnd = request.cycle.endUtc.isValid()
                         ? request.cycle.endUtc
                         : request.generatedAt.toUTC();
  result.sampleCount = samples;
  result.coveragePercent = coverage;
  result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
  result.methodId = QStringLiteral("budget-pacing-v2");
  result.reasonKey = reason;
  result.generatedAt = request.generatedAt.toUTC();
  result.valueClass = request.valueClass;
  return result;
}

bool matchesScope(const BudgetPacingQuery::Request &request,
                  const BudgetPacingQuery::Observation &observation) {
  if (request.scopeMode == QLatin1String("aggregate"))
    return observation.scopeKind.isEmpty() &&
           observation.scopeIdentity.isEmpty();
  return observation.scopeKind == request.scopeKind &&
         observation.scopeIdentity == request.scopeIdentity;
}

qint64 sumWithin(const QList<BudgetPacingQuery::Observation> &observations,
                 const BillingCycleResolver::Cycle &cycle, bool *hasAny) {
  qint64 total = 0;
  *hasAny = false;
  for (const auto &observation : observations) {
    const QDateTime start = observation.intervalStart.toUTC();
    const QDateTime end = observation.intervalEnd.toUTC();
    if (start >= cycle.startUtc && end <= cycle.endUtc && start < end &&
        observation.valueMinor) {
      total += *observation.valueMinor;
      *hasAny = true;
    }
  }
  return total;
}
} // namespace

ForecastContract::Result BudgetPacingQuery::evaluate(const Request &request) {
  const QDateTime now = request.generatedAt.toUTC();
  if (!request.preflightReason.isEmpty())
    return unavailable(request, request.preflightReason);
  if (request.policyId.trimmed().isEmpty() ||
      request.sourceId.trimmed().isEmpty() ||
      request.sourceKind.trimmed().isEmpty() || request.limitMinor <= 0 ||
      request.warningPercent <= 0 ||
      request.warningPercent > request.criticalPercent ||
      request.criticalPercent > 100 || !request.cycle.isValid() ||
      now < request.cycle.startUtc || now >= request.cycle.endUtc) {
    return unavailable(request, request.cycle.reasonKey.isEmpty()
                                    ? QStringLiteral("invalid-policy")
                                    : request.cycle.reasonKey);
  }
  const QString currency = request.currency.trimmed().toUpper();
  if (!CurrencyMinorUnits::digits(currency))
    return unavailable(request, QStringLiteral("unknown-currency"));
  const QTimeZone policyZone(request.timeZoneId.toUtf8());
  if (!policyZone.isValid())
    return unavailable(request, QStringLiteral("invalid-policy"));

  QHash<QPair<qint64, qint64>, Observation> latest;
  latest.reserve(request.observations.size());
  bool sawOtherScope = false;
  for (const Observation &candidate : request.observations) {
    if (!matchesScope(request, candidate)) {
      sawOtherScope = true;
      continue;
    }
    const QPair<qint64, qint64> key{candidate.intervalStart.toMSecsSinceEpoch(),
                                    candidate.intervalEnd.toMSecsSinceEpoch()};
    auto existing = latest.find(key);
    if (existing == latest.end() || existing->observedAt < candidate.observedAt)
      latest.insert(key, candidate);
  }
  if (latest.isEmpty())
    return unavailable(request, request.scopeMode == QLatin1String("scoped") &&
                                        sawOtherScope
                                    ? QStringLiteral("scope-unavailable")
                                    : QStringLiteral("no-data"));

  QList<Observation> compatible;
  compatible.reserve(latest.size());
  for (const Observation &observation : latest) {
    if (!observation.valueMinor || *observation.valueMinor < 0)
      return unavailable(request, QStringLiteral("no-data"));
    if (observation.currency.trimmed().toUpper() != currency)
      return unavailable(request, QStringLiteral("mixed-currency"));
    if (observation.valueClass != request.valueClass)
      return unavailable(request, QStringLiteral("mixed-value-class"));
    compatible.append(observation);
  }

  bool hasCurrent = false;
  const qint64 spent = sumWithin(compatible, request.cycle, &hasCurrent);
  if (!hasCurrent)
    return unavailable(request, QStringLiteral("no-data"));

  const QDateTime todayStartUtc = now.date().startOfDay(QTimeZone::UTC);
  QMap<QDate, qint64> baselineByDay;
  for (const Observation &observation : compatible) {
    if (isCompleteUtcDay(observation, todayStartUtc))
      baselineByDay.insert(observation.intervalStart.toUTC().date(),
                           *observation.valueMinor);
  }
  const int sampleCount = baselineByDay.size();
  double coverage = 0.0;
  if (!baselineByDay.isEmpty()) {
    const int expectedDays =
        baselineByDay.firstKey().daysTo(baselineByDay.lastKey()) + 1;
    coverage = expectedDays > 0
                   ? static_cast<double>(sampleCount) / expectedDays * 100.0
                   : 0.0;
  }
  if (sampleCount < MinimumSamples ||
      coverage + Epsilon < MinimumCoveragePercent)
    return unavailable(request, QStringLiteral("insufficient-samples"),
                       sampleCount, coverage);

  const QList<qint64> baselineValues = baselineByDay.values();
  const double dailyBaseline = median(baselineValues);
  const double remainingDayEquivalents =
      std::max(0.0, static_cast<double>(now.msecsTo(request.cycle.endUtc)) /
                        (24.0 * 60.0 * 60.0 * 1000.0));
  const qint64 projected = qMax<qint64>(
      spent, static_cast<qint64>(std::llround(
                 spent + dailyBaseline * remainingDayEquivalents)));
  const qint64 remaining = qMax<qint64>(0, request.limitMinor - spent);
  const double consumed = static_cast<double>(spent) /
                          static_cast<double>(request.limitMinor) * 100.0;
  const bool predictedOverrun = projected >= request.limitMinor;

  const QDate localToday = now.toTimeZone(policyZone).date();
  const QDateTime localTomorrowUtc =
      QDateTime(localToday.addDays(1), QTime(0, 0), policyZone).toUTC();
  const QDateTime todayBoundary = qMin(localTomorrowUtc, request.cycle.endUtc);
  const qint64 periodMillis =
      request.cycle.startUtc.msecsTo(request.cycle.endUtc);
  const qint64 elapsedAtTodayEnd =
      request.cycle.startUtc.msecsTo(todayBoundary);
  const qint64 proRataAtTodayEnd =
      periodMillis > 0 ? static_cast<qint64>(std::floor(
                             static_cast<double>(request.limitMinor) *
                             elapsedAtTodayEnd / periodMillis))
                       : 0;
  const qint64 safeToday =
      qMax<qint64>(0, qMin(remaining, proRataAtTodayEnd - spent));
  const QDate lastLocalDate =
      request.cycle.endUtc.addMSecs(-1).toTimeZone(policyZone).date();
  const int remainingCalendarDays =
      qMax(1, localToday.daysTo(lastLocalDate) + 1);
  const qint64 remainingDailyAllowance = remaining / remainingCalendarDays;

  bool hasPrevious = false;
  qint64 previousSpent = 0;
  if (request.previousCycle.isValid())
    previousSpent = sumWithin(compatible, request.previousCycle, &hasPrevious);

  ForecastContract::Result result;
  result.contractVersion = QStringLiteral("budget-pacing-v2");
  result.kind = ForecastContract::Kind::BudgetOverrun;
  result.policyId = request.policyId;
  result.sourceId = request.sourceId;
  result.sourceKind = request.sourceKind;
  result.window = request.periodType;
  result.scope = request.scopeMode == QLatin1String("scoped")
                     ? request.scopeKind + QLatin1Char(':') + request.scopeLabel
                     : QStringLiteral("aggregate");
  result.currentValue = CurrencyMinorUnits::toMajor(spent, currency);
  result.projectedValue = CurrencyMinorUnits::toMajor(projected, currency);
  result.limitValue = CurrencyMinorUnits::toMajor(request.limitMinor, currency);
  result.unit = currency;
  result.currency = currency;
  result.periodStart = request.cycle.startUtc;
  result.periodEnd = request.cycle.endUtc;
  result.sampleCount = sampleCount;
  result.coveragePercent = coverage;
  result.evidenceGrade = sampleCount >= 10 && coverage >= 90.0
                             ? ForecastContract::EvidenceGrade::Strong
                             : ForecastContract::EvidenceGrade::Usable;
  result.methodId = QStringLiteral("budget-pacing-v2");
  result.generatedAt = now;
  result.valueClass = request.valueClass;
  result.spentMinor = spent;
  result.remainingMinor = remaining;
  result.consumedPercent = consumed;
  result.projectedPeriodEndMinor = projected;
  result.predictedOverrun = predictedOverrun;
  result.safeTodayMinor = safeToday;
  result.remainingDailyAllowanceMinor = remainingDailyAllowance;
  if (hasPrevious) {
    result.previousPeriodSpentMinor = previousSpent;
    if (previousSpent > 0)
      result.previousPeriodChangePercent =
          static_cast<double>(spent - previousSpent) /
          static_cast<double>(previousSpent) * 100.0;
  }

  if (spent >= request.limitMinor)
    result.state = ForecastContract::State::Exceeded;
  else if (consumed + Epsilon >= request.criticalPercent)
    result.state = ForecastContract::State::Critical;
  else if (consumed + Epsilon >= request.warningPercent || predictedOverrun)
    result.state = ForecastContract::State::Warning;
  else
    result.state = ForecastContract::State::Safe;

  if (predictedOverrun && dailyBaseline > Epsilon) {
    const double daysToLimit = qMax(
        0.0, static_cast<double>(request.limitMinor - spent) / dailyBaseline);
    result.predictedAt = now.addMSecs(
        static_cast<qint64>(daysToLimit * 24.0 * 60.0 * 60.0 * 1000.0));
  }
  return result;
}
