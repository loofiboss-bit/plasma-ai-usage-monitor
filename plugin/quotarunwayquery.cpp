#include "quotarunwayquery.h"

#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr double Epsilon = 1e-9;

double median(QList<double> values) {
  if (values.isEmpty())
    return 0.0;
  std::sort(values.begin(), values.end());
  const qsizetype middle = values.size() / 2;
  return values.size() % 2 == 0
             ? (values.at(middle - 1) + values.at(middle)) / 2.0
             : values.at(middle);
}

bool supportedSource(const QString &source) {
  static const QSet<QString> supported{
      QStringLiteral("response_headers"), QStringLiteral("usage_api"),
      QStringLiteral("metrics_api"), QStringLiteral("billing_api"),
      QStringLiteral("browser_sync")};
  return supported.contains(source.trimmed().toLower());
}

double coverage(const QList<QuotaRunwayQuery::Sample> &samples) {
  if (samples.size() < 2)
    return 0.0;
  QList<double> gaps;
  gaps.reserve(samples.size() - 1);
  for (qsizetype index = 1; index < samples.size(); ++index)
    gaps.append(static_cast<double>(
        samples.at(index - 1).observedAt.secsTo(samples.at(index).observedAt)));
  const double typicalGap = median(gaps);
  const double span = static_cast<double>(
      samples.first().observedAt.secsTo(samples.last().observedAt));
  if (span <= 0.0 || typicalGap <= 0.0)
    return 0.0;
  double covered = 0.0;
  for (double gap : std::as_const(gaps))
    covered += std::min(gap, typicalGap * 2.0);
  return std::clamp(covered / span * 100.0, 0.0, 100.0);
}

QList<QuotaRunwayQuery::Sample>
boundedSamples(const QList<QuotaRunwayQuery::Sample> &samples) {
  if (samples.size() <= QuotaRunwayQuery::MaximumTheilSenSamples)
    return samples;
  QList<QuotaRunwayQuery::Sample> selected;
  selected.reserve(QuotaRunwayQuery::MaximumTheilSenSamples);
  const qsizetype last = samples.size() - 1;
  for (int index = 0; index < QuotaRunwayQuery::MaximumTheilSenSamples;
       ++index) {
    const qsizetype sourceIndex =
        (static_cast<qint64>(index) * last) /
        (QuotaRunwayQuery::MaximumTheilSenSamples - 1);
    selected.append(samples.at(sourceIndex));
  }
  return selected;
}

double consumptionSlope(const QList<QuotaRunwayQuery::Sample> &samples) {
  const QList<QuotaRunwayQuery::Sample> selected = boundedSamples(samples);
  QList<double> slopes;
  slopes.reserve(selected.size() * (selected.size() - 1) / 2);
  for (qsizetype left = 0; left < selected.size(); ++left) {
    for (qsizetype right = left + 1; right < selected.size(); ++right) {
      const qint64 seconds =
          selected.at(left).observedAt.secsTo(selected.at(right).observedAt);
      if (seconds > 0)
        slopes.append(
            (*selected.at(left).remaining - *selected.at(right).remaining) /
            static_cast<double>(seconds));
    }
  }
  return median(slopes);
}

double volatility(const QList<QuotaRunwayQuery::Sample> &samples,
                  double slope) {
  if (samples.size() < 2 || slope <= Epsilon)
    return 0.0;
  QList<double> deviations;
  for (qsizetype index = 1; index < samples.size(); ++index) {
    const qint64 seconds =
        samples.at(index - 1).observedAt.secsTo(samples.at(index).observedAt);
    if (seconds <= 0)
      continue;
    const double candidate =
        (*samples.at(index - 1).remaining - *samples.at(index).remaining) /
        static_cast<double>(seconds);
    deviations.append(std::abs(candidate - slope));
  }
  return median(deviations) / slope;
}
} // namespace

ForecastContract::Result QuotaRunwayQuery::unavailable(
    const Request &request, const QDateTime &generatedAt,
    const QString &reasonKey, int sampleCount, double coveragePercent,
    const QDateTime &periodEnd) {
  ForecastContract::Result result;
  result.kind = ForecastContract::Kind::QuotaExhaustion;
  result.state = ForecastContract::State::Unavailable;
  result.sourceId = request.sourceId;
  result.sourceKind = request.sourceKind;
  result.window = request.window;
  result.scope = request.scope;
  result.unit =
      request.unit.isEmpty() ? QStringLiteral("unknown") : request.unit;
  result.periodEnd =
      periodEnd.isValid() ? periodEnd.toUTC() : generatedAt.toUTC();
  result.sampleCount = sampleCount;
  result.coveragePercent = coveragePercent;
  result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
  result.methodId = QStringLiteral("quota-runway-v1");
  result.reasonKey = reasonKey;
  result.generatedAt = generatedAt.toUTC();
  result.valueClass = ForecastContract::ValueClass::Actual;
  return result;
}

ForecastContract::Result
QuotaRunwayQuery::evaluate(const Request &request,
                           const QDateTime &generatedAt) {
  const QDateTime now = generatedAt.toUTC();
  QList<Sample> samples = request.samples;
  std::sort(samples.begin(), samples.end(),
            [](const Sample &left, const Sample &right) {
              return left.observedAt < right.observedAt;
            });

  if (samples.isEmpty())
    return unavailable(request, now, QStringLiteral("missing_value"));
  const QDateTime periodEnd = samples.last().resetAt;
  const QString quotaSource = samples.first().source.trimmed().toLower();
  for (const Sample &sample : std::as_const(samples)) {
    if (!sample.observedAt.isValid() || !sample.resetAt.isValid() ||
        !sample.remaining || !sample.limit ||
        !std::isfinite(*sample.remaining) || !std::isfinite(*sample.limit) ||
        *sample.remaining < 0.0 || *sample.limit <= 0.0 ||
        *sample.remaining > *sample.limit + Epsilon)
      return unavailable(request, now, QStringLiteral("missing_value"),
                         samples.size(), 0.0, periodEnd);
    if (!supportedSource(sample.source))
      return unavailable(request, now, QStringLiteral("unsupported_source"),
                         samples.size(), 0.0, periodEnd);
    if (sample.resetAt != periodEnd ||
        sample.unit.compare(request.unit, Qt::CaseInsensitive) != 0 ||
        sample.source.trimmed().toLower() != quotaSource ||
        std::abs(*sample.limit - *samples.first().limit) > Epsilon)
      return unavailable(request, now, QStringLiteral("reset_detected"),
                         samples.size(), 0.0, periodEnd);
  }

  const double coveragePercent = coverage(samples);
  if (samples.size() < MinimumSamples)
    return unavailable(request, now, QStringLiteral("insufficient_samples"),
                       samples.size(), coveragePercent, periodEnd);
  const qint64 span =
      samples.first().observedAt.secsTo(samples.last().observedAt);
  if (span < MinimumSpanSeconds)
    return unavailable(request, now, QStringLiteral("insufficient_span"),
                       samples.size(), coveragePercent, periodEnd);
  if (samples.last().observedAt.secsTo(now) > MaximumAgeSeconds ||
      samples.last().observedAt > now.addSecs(1))
    return unavailable(request, now, QStringLiteral("stale_data"),
                       samples.size(), coveragePercent, periodEnd);
  if (periodEnd <= now)
    return unavailable(request, now, QStringLiteral("incompatible_window"),
                       samples.size(), coveragePercent, periodEnd);
  for (qsizetype index = 1; index < samples.size(); ++index) {
    if (*samples.at(index).remaining >
        *samples.at(index - 1).remaining + Epsilon)
      return unavailable(request, now, QStringLiteral("non_monotonic"),
                         samples.size(), coveragePercent, periodEnd);
  }

  const double slope = consumptionSlope(samples);
  ForecastContract::Result result;
  result.kind = ForecastContract::Kind::QuotaExhaustion;
  result.sourceId = request.sourceId;
  result.sourceKind = request.sourceKind;
  result.window = request.window;
  result.scope = request.scope;
  result.currentValue = *samples.last().remaining;
  result.limitValue = *samples.last().limit;
  result.unit = request.unit;
  result.periodEnd = periodEnd;
  result.sampleCount = samples.size();
  result.coveragePercent = coveragePercent;
  result.methodId = QStringLiteral("quota-runway-v1");
  result.generatedAt = now;
  result.valueClass = ForecastContract::ValueClass::Actual;
  const double sampleVolatility = volatility(samples, slope);
  result.evidenceGrade =
      samples.size() >= 8 && coveragePercent >= 80.0 && sampleVolatility <= 0.25
          ? ForecastContract::EvidenceGrade::Strong
          : ForecastContract::EvidenceGrade::Usable;

  const QDateTime predictionOrigin = samples.last().observedAt;
  const qint64 remainingWindowSeconds = predictionOrigin.secsTo(periodEnd);
  if (slope <= Epsilon) {
    result.state = ForecastContract::State::Safe;
    result.projectedValue = *samples.last().remaining;
    return result;
  }
  const double secondsToExhaustion = *samples.last().remaining / slope;
  result.projectedValue =
      std::max(0.0, *samples.last().remaining -
                        slope * static_cast<double>(remainingWindowSeconds));
  if (secondsToExhaustion >= static_cast<double>(remainingWindowSeconds)) {
    result.state = ForecastContract::State::Safe;
    return result;
  }
  result.predictedAt = predictionOrigin.addMSecs(
      static_cast<qint64>(secondsToExhaustion * 1000.0));
  const double criticalHorizon =
      std::max(3600.0, static_cast<double>(now.secsTo(periodEnd)) * 0.25);
  result.state = now.msecsTo(*result.predictedAt) / 1000.0 <= criticalHorizon
                     ? ForecastContract::State::Critical
                     : ForecastContract::State::Warning;
  return result;
}
