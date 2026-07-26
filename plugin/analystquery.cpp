#include "analystquery.h"

#include <QMap>
#include <QSet>
#include <QSqlQuery>

#include <algorithm>
#include <cmath>

namespace {
struct CostObservation {
  QString date;
  QString provider;
  QString model;
  QString currency;
  QString quality;
  QString semantic;
  QString source;
  QString scope;
  QString window;
  double value = 0.0;
};

struct DailyCost {
  double actual = 0.0;
  double estimated = 0.0;
  int actualSamples = 0;
  int estimatedSamples = 0;
};

struct Driver {
  QString provider;
  QString model;
  QString quality;
  double value = 0.0;
  int sampleCount = 0;
};

struct DailyActivity {
  double tokens = 0.0;
  double requests = 0.0;
  double toolUsage = 0.0;
  int tokenSamples = 0;
  int requestSamples = 0;
  int toolSamples = 0;
};

struct AnalystRows {
  QList<CostObservation> costs;
  QMap<QString, DailyActivity> activity;
  QVariantList ratios;
  QSet<QString> currencies;
  QString errorKey;
};

QString toDatabaseDateTime(const QDateTime &value) {
  return value.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

double percentChange(double previous, double current) {
  return qFuzzyIsNull(previous)
             ? 0.0
             : ((current - previous) / std::abs(previous)) * 100.0;
}

QString qualityClass(const QString &source, const QString &quality,
                     const QString &semantic) {
  const QString normalizedSource = source.trimmed().toLower();
  const QString normalizedQuality = quality.trimmed().toLower();
  if (semantic == QLatin1String("local_estimate") ||
      normalizedQuality.contains(QStringLiteral("estimated")) ||
      normalizedSource.contains(QStringLiteral("estimated")) ||
      normalizedSource == QLatin1String("self_tracked") ||
      normalizedSource == QLatin1String("browser_sync")) {
    return QStringLiteral("estimated");
  }
  if (normalizedSource.contains(QStringLiteral("connectivity")) ||
      normalizedSource.contains(QStringLiteral("model_discovery"))) {
    return QStringLiteral("connectivity_only");
  }
  if (normalizedSource == QLatin1String("unknown") &&
      (normalizedQuality.isEmpty() ||
       normalizedQuality == QLatin1String("unknown"))) {
    return QStringLiteral("unavailable");
  }
  return QStringLiteral("actual");
}

QVariantMap kpi(bool available, const QVariant &value, const QString &reasonKey,
                int sampleCount, int minimumSamples) {
  return {
      {QStringLiteral("available"), available},
      {QStringLiteral("value"), available ? value : QVariant()},
      {QStringLiteral("reasonKey"), available ? QString() : reasonKey},
      {QStringLiteral("sampleCount"), sampleCount},
      {QStringLiteral("minimumSamples"), minimumSamples},
  };
}

AnalystRows queryRows(const QSqlDatabase &database, const QDateTime &fromUtc,
                      const QDateTime &toUtc) {
  AnalystRows rows;
  const QString from = toDatabaseDateTime(fromUtc);
  const QString to = toDatabaseDateTime(toUtc);

  QSqlQuery costQuery(database);
  costQuery.setForwardOnly(true);
  costQuery.prepare(QStringLiteral(
      "SELECT date(observed_at_utc), provider, model_scope, currency, "
      "source, data_quality, semantic, value, scope, window "
      "FROM observations "
      "WHERE metric_kind='cost' AND value IS NOT NULL "
      "AND observed_at_utc >= ? AND observed_at_utc < ? "
      "AND lower(source) != 'unknown' "
      "AND lower(source) NOT LIKE '%connectivity%' "
      "AND lower(source) NOT LIKE '%model_discovery%' "
      "AND semantic IN ('gauge','interval_total','local_estimate') "
      "ORDER BY observed_at_utc, id"));
  costQuery.addBindValue(from);
  costQuery.addBindValue(to);
  if (!costQuery.exec()) {
    rows.errorKey = QStringLiteral("cost_query_failed");
  } else {
    while (costQuery.next()) {
      CostObservation observation;
      observation.date = costQuery.value(0).toString();
      observation.provider = costQuery.value(1).toString();
      observation.model = costQuery.value(2).toString();
      observation.currency = costQuery.value(3).toString().trimmed().toUpper();
      observation.source = costQuery.value(4).toString();
      observation.quality =
          qualityClass(observation.source, costQuery.value(5).toString(),
                       costQuery.value(6).toString());
      observation.semantic = costQuery.value(6).toString();
      observation.value = costQuery.value(7).toDouble();
      observation.scope = costQuery.value(8).toString();
      observation.window = costQuery.value(9).toString();
      if (!observation.currency.isEmpty()) {
        rows.currencies.insert(observation.currency);
        rows.costs.append(observation);
      }
    }
  }

  QMap<QString, QMap<QString, QPair<QString, double>>> activityBySource;
  QMap<QString, QMap<QString, int>> activitySamples;
  QSqlQuery activityQuery(database);
  activityQuery.setForwardOnly(true);
  activityQuery.prepare(QStringLiteral(
      "SELECT date(observed_at_utc), provider, metric_kind, semantic, value "
      "FROM observations "
      "WHERE metric_kind IN ('input_tokens','output_tokens','requests') "
      "AND value IS NOT NULL AND observed_at_utc >= ? "
      "AND observed_at_utc < ? "
      "AND lower(source) != 'unknown' "
      "AND lower(source) NOT LIKE '%connectivity%' "
      "AND lower(source) NOT LIKE '%model_discovery%' "
      "ORDER BY observed_at_utc"));
  activityQuery.addBindValue(from);
  activityQuery.addBindValue(to);
  if (activityQuery.exec()) {
    while (activityQuery.next()) {
      const QString day = activityQuery.value(0).toString();
      const QString key = activityQuery.value(1).toString() + QChar(0x1f) +
                          activityQuery.value(2).toString();
      const QString semantic = activityQuery.value(3).toString();
      const double value = activityQuery.value(4).toDouble();
      auto &aggregate = activityBySource[day][key];
      if (aggregate.first.isEmpty()) {
        aggregate = {semantic, value};
      } else if (semantic == QLatin1String("interval_total")) {
        aggregate.second += value;
      } else {
        aggregate.second = std::max(aggregate.second, value);
      }
      activitySamples[day][key]++;
    }
  }
  for (auto day = activityBySource.cbegin(); day != activityBySource.cend();
       ++day) {
    DailyActivity &result = rows.activity[day.key()];
    for (auto source = day.value().cbegin(); source != day.value().cend();
         ++source) {
      const QString kind = source.key().section(QChar(0x1f), 1, 1);
      if (kind == QLatin1String("requests")) {
        result.requests += source.value().second;
        result.requestSamples +=
            activitySamples.value(day.key()).value(source.key());
      } else {
        result.tokens += source.value().second;
        result.tokenSamples +=
            activitySamples.value(day.key()).value(source.key());
      }
    }
  }

  QSqlQuery toolQuery(database);
  toolQuery.setForwardOnly(true);
  toolQuery.prepare(QStringLiteral(
      "SELECT date(timestamp), tool_name, MAX(usage_count), COUNT(*) "
      "FROM subscription_tool_usage "
      "WHERE timestamp >= ? AND timestamp < ? "
      "GROUP BY date(timestamp), tool_name ORDER BY date(timestamp)"));
  toolQuery.addBindValue(from);
  toolQuery.addBindValue(to);
  if (toolQuery.exec()) {
    while (toolQuery.next()) {
      DailyActivity &day = rows.activity[toolQuery.value(0).toString()];
      day.toolUsage += toolQuery.value(2).toDouble();
      day.toolSamples += toolQuery.value(3).toInt();
    }
  }

  QSqlQuery ratioQuery(database);
  ratioQuery.setForwardOnly(true);
  ratioQuery.prepare(QStringLiteral(
      "SELECT day, SUM(provider_input), SUM(provider_output) FROM ("
      " SELECT date(timestamp) AS day, provider, "
      " MAX(input_tokens) AS provider_input, "
      " MAX(output_tokens) AS provider_output "
      " FROM usage_snapshots "
      " WHERE timestamp >= ? AND timestamp < ? "
      " AND lower(usage_source) != 'unknown' "
      " AND lower(usage_source) NOT LIKE '%connectivity%' "
      " AND lower(usage_source) NOT LIKE '%model_discovery%' "
      " GROUP BY day, provider"
      ") GROUP BY day HAVING SUM(provider_input) > 0 ORDER BY day"));
  ratioQuery.addBindValue(from);
  ratioQuery.addBindValue(to);
  if (ratioQuery.exec()) {
    while (ratioQuery.next()) {
      const double input = ratioQuery.value(1).toDouble();
      const double output = ratioQuery.value(2).toDouble();
      rows.ratios.append(
          QVariantMap{{QStringLiteral("date"), ratioQuery.value(0).toString()},
                      {QStringLiteral("value"), output / input},
                      {QStringLiteral("inputTokens"), input},
                      {QStringLiteral("outputTokens"), output}});
    }
  }
  return rows;
}

QVariantMap initialSnapshot(bool initialized, const QDateTime &fromUtc,
                            const QDateTime &toUtc) {
  const bool valid =
      initialized && fromUtc.isValid() && toUtc.isValid() && fromUtc < toUtc;
  return {
      {QStringLiteral("from"), fromUtc},
      {QStringLiteral("to"), toUtc},
      {QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc()},
      {QStringLiteral("ok"), valid},
      {QStringLiteral("errorKey"),
       valid ? QString() : QStringLiteral("invalid_request")},
      {QStringLiteral("methods"),
       QVariantMap{
           {QStringLiteral("averageDailySpendMinimumDays"), 3},
           {QStringLiteral("weekOverWeekDaysPerWindow"), 7},
           {QStringLiteral("volatilityMinimumDays"), 7},
           {QStringLiteral("ratioMinimumDays"), 3},
           {QStringLiteral("anomalyMinimumDays"), 7},
           {QStringLiteral("anomalyBaseline"),
            QStringLiteral("period_mean_and_population_standard_deviation")},
           {QStringLiteral("anomalySigmaThreshold"), 2.0},
           {QStringLiteral("anomalyMinimumRelativeIncreasePercent"), 50.0},
           {QStringLiteral("anomalyMinimumAbsoluteIncrease"), 1.0},
       }},
      {QStringLiteral("kpis"),
       QVariantMap{
           {QStringLiteral("averageDailySpend"),
            kpi(false, {}, QStringLiteral("no_compatible_cost"), 0, 3)},
           {QStringLiteral("weekOverWeekChange"),
            kpi(false, {}, QStringLiteral("incomplete_comparison_windows"), 0,
                14)},
           {QStringLiteral("volatility"),
            kpi(false, {}, QStringLiteral("insufficient_daily_samples"), 0, 7)},
           {QStringLiteral("outputInputRatio"),
            kpi(false, {}, QStringLiteral("insufficient_ratio_samples"), 0, 3)},
       }},
      {QStringLiteral("spendSeries"), QVariantList{}},
      {QStringLiteral("activitySeries"), QVariantList{}},
      {QStringLiteral("ratioSeries"), QVariantList{}},
      {QStringLiteral("topDrivers"), QVariantList{}},
      {QStringLiteral("anomalies"), QVariantList{}},
      {QStringLiteral("actualSampleCount"), 0},
      {QStringLiteral("estimatedSampleCount"), 0},
      {QStringLiteral("currencies"), QStringList{}},
      {QStringLiteral("currency"), QString()},
      {QStringLiteral("currencyStatus"), QStringLiteral("none")},
      {QStringLiteral("mixedCurrencies"), false},
  };
}
} // namespace

QVariantMap AnalystQuery::execute(const QSqlDatabase &database,
                                  bool initialized,
                                  const QDateTime &fromInclusive,
                                  const QDateTime &toExclusive,
                                  const QString &requestedCurrency) {
  const QDateTime fromUtc = fromInclusive.toUTC();
  const QDateTime toUtc = toExclusive.toUTC();
  QVariantMap snapshot = initialSnapshot(initialized, fromUtc, toUtc);
  const int requestedDays = std::max(
      1, static_cast<int>(fromUtc.date().daysTo(toUtc.date())));
  if (!snapshot.value(QStringLiteral("ok")).toBool()) {
    snapshot.insert(
        QStringLiteral("coverage"),
        QVariantMap{{QStringLiteral("requestedDayCount"), 0},
                    {QStringLiteral("observedDayCount"), 0},
                    {QStringLiteral("percent"), 0.0},
                    {QStringLiteral("firstObservation"), QVariant()},
                    {QStringLiteral("lastObservation"), QVariant()}});
    return snapshot;
  }

  const AnalystRows rows = queryRows(database, fromUtc, toUtc);
  if (!rows.errorKey.isEmpty()) {
    snapshot.insert(QStringLiteral("ok"), false);
    snapshot.insert(QStringLiteral("errorKey"), rows.errorKey);
  }

  QStringList currencies = rows.currencies.values();
  currencies.sort();
  snapshot.insert(QStringLiteral("currencies"), currencies);
  const QString requested = requestedCurrency.trimmed().toUpper();
  QString currency;
  if (!requested.isEmpty()) {
    currency = requested;
    snapshot.insert(QStringLiteral("currencyStatus"),
                    QStringLiteral("selected"));
  } else if (currencies.size() == 1) {
    currency = currencies.constFirst();
    snapshot.insert(QStringLiteral("currencyStatus"), QStringLiteral("single"));
  } else if (currencies.size() > 1) {
    snapshot.insert(QStringLiteral("currencyStatus"), QStringLiteral("mixed"));
    snapshot.insert(QStringLiteral("mixedCurrencies"), true);
  }
  snapshot.insert(QStringLiteral("currency"), currency);

  QMap<QString, DailyCost> dailyCosts;
  QMap<QString, Driver> driverTotals;
  int actualSamples = 0;
  int estimatedSamples = 0;
  if (!currency.isEmpty()) {
    QList<CostObservation> effective;
    QMap<QString, CostObservation> gauges;
    for (const CostObservation &observation : rows.costs) {
      if (observation.currency != currency) {
        continue;
      }
      if (observation.semantic == QLatin1String("gauge")) {
        gauges.insert(QStringList{observation.date, observation.provider,
                                  observation.model, observation.currency,
                                  observation.quality, observation.source,
                                  observation.scope, observation.window}
                          .join(QChar(0x1f)),
                      observation);
      } else {
        effective.append(observation);
      }
    }
    effective.append(gauges.values());
    for (const CostObservation &observation : effective) {
      const bool estimated = observation.quality == QLatin1String("estimated");
      DailyCost &day = dailyCosts[observation.date];
      if (estimated) {
        day.estimated += observation.value;
        day.estimatedSamples++;
        estimatedSamples++;
      } else {
        day.actual += observation.value;
        day.actualSamples++;
        actualSamples++;
      }
      const QString model = observation.model.trimmed().isEmpty()
                                ? observation.provider
                                : observation.model.trimmed();
      Driver &driver = driverTotals[observation.provider + QChar(0x1f) + model +
                                    QChar(0x1f) + observation.quality];
      driver.provider = observation.provider;
      driver.model = model;
      driver.quality = observation.quality;
      driver.value += observation.value;
      driver.sampleCount++;
    }
  }
  snapshot.insert(QStringLiteral("actualSampleCount"), actualSamples);
  snapshot.insert(QStringLiteral("estimatedSampleCount"), estimatedSamples);

  QVariantList spendSeries;
  QList<double> combinedDailyCosts;
  double totalSpend = 0.0;
  for (auto day = dailyCosts.cbegin(); day != dailyCosts.cend(); ++day) {
    const double combined = day->actual + day->estimated;
    spendSeries.append(QVariantMap{
        {QStringLiteral("date"), day.key()},
        {QStringLiteral("actualAvailable"), day->actualSamples > 0},
        {QStringLiteral("actual"),
         day->actualSamples > 0 ? QVariant(day->actual) : QVariant()},
        {QStringLiteral("estimatedAvailable"), day->estimatedSamples > 0},
        {QStringLiteral("estimated"),
         day->estimatedSamples > 0 ? QVariant(day->estimated) : QVariant()},
        {QStringLiteral("currency"), currency},
        {QStringLiteral("sampleCount"),
         day->actualSamples + day->estimatedSamples},
    });
    combinedDailyCosts.append(combined);
    totalSpend += combined;
  }
  snapshot.insert(QStringLiteral("spendSeries"), spendSeries);

  QList<Driver> drivers = driverTotals.values();
  std::sort(drivers.begin(), drivers.end(),
            [](const Driver &left, const Driver &right) {
              if (!qFuzzyCompare(left.value + 1.0, right.value + 1.0)) {
                return left.value > right.value;
              }
              return left.provider == right.provider
                         ? left.model < right.model
                         : left.provider < right.provider;
            });
  QVariantList topDrivers;
  for (int index = 0; index < std::min(5, static_cast<int>(drivers.size()));
       ++index) {
    const Driver &driver = drivers.at(index);
    topDrivers.append(QVariantMap{
        {QStringLiteral("provider"), driver.provider},
        {QStringLiteral("model"), driver.model},
        {QStringLiteral("value"), driver.value},
        {QStringLiteral("quality"), driver.quality},
        {QStringLiteral("estimated"),
         driver.quality == QLatin1String("estimated")},
        {QStringLiteral("currency"), currency},
        {QStringLiteral("sampleCount"), driver.sampleCount},
    });
  }
  snapshot.insert(QStringLiteral("topDrivers"), topDrivers);

  QVariantMap kpis = snapshot.value(QStringLiteral("kpis")).toMap();
  const QString costReason =
      snapshot.value(QStringLiteral("mixedCurrencies")).toBool()
          ? QStringLiteral("mixed_currencies")
          : QStringLiteral("no_compatible_cost");
  kpis.insert(
      QStringLiteral("averageDailySpend"),
      dailyCosts.size() >= 3
          ? kpi(true, totalSpend / dailyCosts.size(), {}, dailyCosts.size(), 3)
          : kpi(false, {},
                dailyCosts.isEmpty()
                    ? costReason
                    : QStringLiteral("insufficient_daily_samples"),
                dailyCosts.size(), 3));

  if (dailyCosts.size() >= 7) {
    const double mean = totalSpend / combinedDailyCosts.size();
    double variance = 0.0;
    for (double value : combinedDailyCosts) {
      const double delta = value - mean;
      variance += delta * delta;
    }
    variance /= combinedDailyCosts.size();
    const double standardDeviation = std::sqrt(variance);
    const bool available = !qFuzzyIsNull(mean);
    kpis.insert(QStringLiteral("volatility"),
                kpi(available,
                    available
                        ? QVariant((standardDeviation / std::abs(mean)) * 100.0)
                        : QVariant(),
                    available ? QString() : QStringLiteral("zero_baseline"),
                    dailyCosts.size(), 7));
    QVariantList anomalies;
    const double absoluteThreshold = std::max(1.0, std::abs(mean) * 0.5);
    int index = 0;
    for (auto day = dailyCosts.cbegin(); day != dailyCosts.cend();
         ++day, ++index) {
      const double value = combinedDailyCosts.at(index);
      if (value - mean < absoluteThreshold ||
          value < mean + 2.0 * standardDeviation) {
        continue;
      }
      anomalies.append(QVariantMap{
          {QStringLiteral("date"), day.key()},
          {QStringLiteral("value"), value},
          {QStringLiteral("currency"), currency},
          {QStringLiteral("baseline"), mean},
          {QStringLiteral("standardDeviation"), standardDeviation},
          {QStringLiteral("deltaPercent"),
           qFuzzyIsNull(mean) ? QVariant()
                              : QVariant(percentChange(mean, value))},
      });
    }
    snapshot.insert(QStringLiteral("anomalies"), anomalies);
    snapshot.insert(QStringLiteral("anomaliesAvailable"), true);
    snapshot.insert(QStringLiteral("anomaliesReasonKey"), QString());
  } else {
    kpis.insert(QStringLiteral("volatility"),
                kpi(false, {},
                    dailyCosts.isEmpty()
                        ? costReason
                        : QStringLiteral("insufficient_daily_samples"),
                    dailyCosts.size(), 7));
    snapshot.insert(QStringLiteral("anomaliesAvailable"), false);
    snapshot.insert(QStringLiteral("anomaliesReasonKey"),
                    dailyCosts.isEmpty()
                        ? costReason
                        : QStringLiteral("insufficient_daily_samples"));
  }

  const QDate comparisonEnd = toUtc.addMSecs(-1).date();
  bool completeComparison = true;
  double currentWeek = 0.0;
  double previousWeek = 0.0;
  for (int offset = 0; offset < 14; ++offset) {
    const QString day = comparisonEnd.addDays(-offset).toString(Qt::ISODate);
    if (!dailyCosts.contains(day)) {
      completeComparison = false;
      break;
    }
    const double value =
        dailyCosts.value(day).actual + dailyCosts.value(day).estimated;
    (offset < 7 ? currentWeek : previousWeek) += value;
  }
  kpis.insert(
      QStringLiteral("weekOverWeekChange"),
      completeComparison && !qFuzzyIsNull(previousWeek)
          ? kpi(true, percentChange(previousWeek, currentWeek), {}, 14, 14)
          : kpi(false, {},
                completeComparison
                    ? QStringLiteral("zero_previous_window")
                    : QStringLiteral("incomplete_comparison_windows"),
                dailyCosts.size(), 14));

  QVariantList activitySeries;
  for (auto day = rows.activity.cbegin(); day != rows.activity.cend(); ++day) {
    activitySeries.append(QVariantMap{
        {QStringLiteral("date"), day.key()},
        {QStringLiteral("tokensAvailable"), day->tokenSamples > 0},
        {QStringLiteral("tokens"),
         day->tokenSamples > 0 ? QVariant(day->tokens) : QVariant()},
        {QStringLiteral("requestsAvailable"), day->requestSamples > 0},
        {QStringLiteral("requests"),
         day->requestSamples > 0 ? QVariant(day->requests) : QVariant()},
        {QStringLiteral("toolUsageAvailable"), day->toolSamples > 0},
        {QStringLiteral("toolUsage"),
         day->toolSamples > 0 ? QVariant(day->toolUsage) : QVariant()},
        {QStringLiteral("sampleCount"),
         day->tokenSamples + day->requestSamples + day->toolSamples},
    });
  }
  snapshot.insert(QStringLiteral("activitySeries"), activitySeries);
  snapshot.insert(QStringLiteral("activityAvailable"),
                  !activitySeries.isEmpty());
  snapshot.insert(QStringLiteral("activityReasonKey"),
                  activitySeries.isEmpty()
                      ? QStringLiteral("no_compatible_activity")
                      : QString());

  snapshot.insert(QStringLiteral("ratioSeries"), rows.ratios);
  double ratioTotal = 0.0;
  for (const QVariant &ratio : rows.ratios) {
    ratioTotal += ratio.toMap().value(QStringLiteral("value")).toDouble();
  }
  kpis.insert(QStringLiteral("outputInputRatio"),
              rows.ratios.size() >= 3
                  ? kpi(true, ratioTotal / rows.ratios.size(), {},
                        rows.ratios.size(), 3)
                  : kpi(false, {}, QStringLiteral("insufficient_ratio_samples"),
                        rows.ratios.size(), 3));
  snapshot.insert(QStringLiteral("kpis"), kpis);

  QSet<QString> observedDays;
  for (auto day = dailyCosts.cbegin(); day != dailyCosts.cend(); ++day) {
    observedDays.insert(day.key());
  }
  for (auto day = rows.activity.cbegin(); day != rows.activity.cend(); ++day) {
    observedDays.insert(day.key());
  }
  for (const QVariant &ratio : rows.ratios) {
    observedDays.insert(ratio.toMap().value(QStringLiteral("date")).toString());
  }
  QStringList sortedDays = observedDays.values();
  sortedDays.sort();
  snapshot.insert(
      QStringLiteral("coverage"),
      QVariantMap{
          {QStringLiteral("requestedDayCount"), requestedDays},
          {QStringLiteral("observedDayCount"), observedDays.size()},
          {QStringLiteral("percent"),
           static_cast<double>(observedDays.size()) / requestedDays * 100.0},
          {QStringLiteral("firstObservation"),
           sortedDays.isEmpty() ? QVariant()
                                : QVariant(QDate::fromString(
                                      sortedDays.constFirst(), Qt::ISODate))},
          {QStringLiteral("lastObservation"),
           sortedDays.isEmpty() ? QVariant()
                                : QVariant(QDate::fromString(
                                      sortedDays.constLast(), Qt::ISODate))},
      });
  return snapshot;
}
