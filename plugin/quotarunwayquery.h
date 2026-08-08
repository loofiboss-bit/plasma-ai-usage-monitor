#ifndef QUOTARUNWAYQUERY_H
#define QUOTARUNWAYQUERY_H

#include "forecastcontract.h"

#include <QList>
#include <optional>

class QuotaRunwayQuery final {
public:
  struct Sample {
    QDateTime observedAt;
    QDateTime resetAt;
    std::optional<double> remaining;
    std::optional<double> limit;
    QString unit;
    QString source;
  };

  struct Request {
    QString sourceId;
    QString sourceKind;
    QString window;
    QString scope;
    QString unit;
    QList<Sample> samples;
  };

  static ForecastContract::Result evaluate(const Request &request,
                                           const QDateTime &generatedAt);
  static ForecastContract::Result
  unavailable(const Request &request, const QDateTime &generatedAt,
              const QString &reasonKey, int sampleCount = 0,
              double coverage = 0.0, const QDateTime &periodEnd = {});

  static constexpr int MinimumSamples = 4;
  static constexpr qint64 MinimumSpanSeconds = 15 * 60;
  static constexpr qint64 MaximumAgeSeconds = 15 * 60;
  static constexpr int MaximumTheilSenSamples = 256;
  static constexpr int MinimumQuotaSamples = MinimumSamples;
  static constexpr qint64 MinimumQuotaSpanSeconds = MinimumSpanSeconds;
  static constexpr qint64 MaximumQuotaAgeSeconds = MaximumAgeSeconds;
};

#endif
