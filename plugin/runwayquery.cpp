#include "runwayquery.h"

#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double Epsilon = 1e-9;

QDateTime utcDateTime(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QString) {
        const QString text = value.toString();
        QDateTime dateTime = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (dateTime.isValid()) {
            dateTime.setTimeZone(QTimeZone::UTC);
            return dateTime;
        }
        dateTime = QDateTime::fromString(text, Qt::ISODateWithMs);
        if (!dateTime.isValid()) {
            dateTime = QDateTime::fromString(text, Qt::ISODate);
        }
        return dateTime.isValid() ? dateTime.toUTC() : QDateTime();
    }
    QDateTime dateTime = value.toDateTime();
    return dateTime.isValid() ? dateTime.toUTC() : QDateTime();
}

double median(QList<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const qsizetype middle = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values.at(middle - 1) + values.at(middle)) / 2.0;
    }
    return values.at(middle);
}

ForecastContract::Result unavailableQuota(const RunwayQuery::QuotaRequest &request, const QDateTime &generatedAt,
    const QString &reasonKey, int sampleCount = 0, double coverage = 0.0, const QDateTime &periodEnd = { })
{
    ForecastContract::Result result;
    result.kind = ForecastContract::Kind::QuotaExhaustion;
    result.state = ForecastContract::State::Unavailable;
    result.sourceId = request.sourceId;
    result.sourceKind = request.sourceKind;
    result.window = request.window;
    result.scope = request.scope;
    result.unit = request.unit.isEmpty() ? QStringLiteral("unknown") : request.unit;
    result.periodEnd = periodEnd.isValid() ? periodEnd.toUTC() : generatedAt.toUTC();
    result.sampleCount = sampleCount;
    result.coveragePercent = coverage;
    result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
    result.methodId = QStringLiteral("quota-runway-v1");
    result.reasonKey = reasonKey;
    result.generatedAt = generatedAt.toUTC();
    result.valueClass = ForecastContract::ValueClass::Actual;
    return result;
}

ForecastContract::Result unavailableBudget(const RunwayQuery::BudgetRequest &request, const QDateTime &generatedAt,
    const QString &reasonKey, int sampleCount = 0, double coverage = 0.0)
{
    const QDate currentDate = generatedAt.toUTC().date();
    const QDate monthEnd = currentDate.addMonths(1).addDays(1 - currentDate.day());
    ForecastContract::Result result;
    result.kind = ForecastContract::Kind::BudgetOverrun;
    result.state = ForecastContract::State::Unavailable;
    result.sourceId = request.sourceId;
    result.sourceKind = request.sourceKind;
    result.window = request.window;
    result.scope = request.scope;
    result.unit = request.budgetCurrency.isEmpty() ? QStringLiteral("unknown") : request.budgetCurrency.toUpper();
    if (!request.budgetCurrency.trimmed().isEmpty()) {
        result.currency = request.budgetCurrency.trimmed().toUpper();
    }
    result.periodEnd = monthEnd.startOfDay(QTimeZone::UTC);
    result.sampleCount = sampleCount;
    result.coveragePercent = coverage;
    result.evidenceGrade = ForecastContract::EvidenceGrade::Unavailable;
    result.methodId = QStringLiteral("budget-pacing-v1");
    result.reasonKey = reasonKey;
    result.generatedAt = generatedAt.toUTC();
    result.valueClass = request.valueClass;
    return result;
}

bool supportedQuotaSource(const QString &source)
{
    static const QSet<QString> supported {
        QStringLiteral("response_headers"),
        QStringLiteral("usage_api"),
        QStringLiteral("metrics_api"),
        QStringLiteral("billing_api"),
        QStringLiteral("browser_sync"),
    };
    return supported.contains(source.trimmed().toLower());
}

double quotaCoverage(const QList<RunwayQuery::QuotaSample> &samples)
{
    if (samples.size() < 2) {
        return 0.0;
    }
    QList<double> gaps;
    gaps.reserve(samples.size() - 1);
    for (qsizetype index = 1; index < samples.size(); ++index) {
        gaps.append(static_cast<double>(samples.at(index - 1).observedAt.secsTo(samples.at(index).observedAt)));
    }
    const double typicalGap = median(gaps);
    const double span = static_cast<double>(samples.first().observedAt.secsTo(samples.last().observedAt));
    if (span <= 0.0 || typicalGap <= 0.0) {
        return 0.0;
    }
    double covered = 0.0;
    for (double gap : std::as_const(gaps)) {
        covered += std::min(gap, typicalGap * 2.0);
    }
    return std::clamp(covered / span * 100.0, 0.0, 100.0);
}

QList<RunwayQuery::QuotaSample> boundedTheilSenSamples(const QList<RunwayQuery::QuotaSample> &samples)
{
    if (samples.size() <= RunwayQuery::MaximumTheilSenSamples) {
        return samples;
    }
    QList<RunwayQuery::QuotaSample> selected;
    selected.reserve(RunwayQuery::MaximumTheilSenSamples);
    const qsizetype last = samples.size() - 1;
    for (int index = 0; index < RunwayQuery::MaximumTheilSenSamples; ++index) {
        const qsizetype sourceIndex = (static_cast<qint64>(index) * last) / (RunwayQuery::MaximumTheilSenSamples - 1);
        selected.append(samples.at(sourceIndex));
    }
    return selected;
}

double medianConsumptionSlope(const QList<RunwayQuery::QuotaSample> &samples)
{
    const QList<RunwayQuery::QuotaSample> selected = boundedTheilSenSamples(samples);
    QList<double> slopes;
    slopes.reserve(selected.size() * (selected.size() - 1) / 2);
    for (qsizetype left = 0; left < selected.size(); ++left) {
        for (qsizetype right = left + 1; right < selected.size(); ++right) {
            const qint64 seconds = selected.at(left).observedAt.secsTo(selected.at(right).observedAt);
            if (seconds <= 0) {
                continue;
            }
            slopes.append(
                (*selected.at(left).remaining - *selected.at(right).remaining) / static_cast<double>(seconds));
        }
    }
    return median(slopes);
}

double quotaVolatility(const QList<RunwayQuery::QuotaSample> &samples, double slope)
{
    if (samples.size() < 2 || slope <= Epsilon) {
        return 0.0;
    }
    QList<double> adjacentSlopes;
    adjacentSlopes.reserve(samples.size() - 1);
    for (qsizetype index = 1; index < samples.size(); ++index) {
        const qint64 seconds = samples.at(index - 1).observedAt.secsTo(samples.at(index).observedAt);
        if (seconds <= 0) {
            continue;
        }
        adjacentSlopes.append(
            (*samples.at(index - 1).remaining - *samples.at(index).remaining) / static_cast<double>(seconds));
    }
    QList<double> deviations;
    deviations.reserve(adjacentSlopes.size());
    for (double candidate : std::as_const(adjacentSlopes)) {
        deviations.append(std::abs(candidate - slope));
    }
    return median(deviations) / slope;
}

ForecastContract::ValueClass valueClassFromKey(const QString &key)
{
    return key.trimmed().toLower() == QLatin1String("estimated") ? ForecastContract::ValueClass::Estimated
                                                                 : ForecastContract::ValueClass::Actual;
}

QString valueClassForObservation(const QString &semantic, const QString &source, const QString &quality)
{
    const QString normalizedSource = source.trimmed().toLower();
    const QString normalizedQuality = quality.trimmed().toLower();
    if (semantic == QLatin1String("local_estimate") || normalizedSource.contains(QStringLiteral("estimated"))
        || normalizedSource == QLatin1String("self_tracked") || normalizedSource == QLatin1String("browser_sync")
        || normalizedQuality.contains(QStringLiteral("estimated"))) {
        return QStringLiteral("estimated");
    }
    return QStringLiteral("actual");
}

RunwayQuery::QuotaRequest quotaRequestFromMap(const QVariantMap &map)
{
    RunwayQuery::QuotaRequest request;
    request.sourceId = map.value(QStringLiteral("sourceId")).toString();
    request.sourceKind = map.value(QStringLiteral("sourceKind")).toString();
    request.window = map.value(QStringLiteral("window")).toString();
    request.scope = map.value(QStringLiteral("scope")).toString();
    request.unit = map.value(QStringLiteral("unit")).toString();
    for (const QVariant &entry : map.value(QStringLiteral("samples")).toList()) {
        const QVariantMap sampleMap = entry.toMap();
        RunwayQuery::QuotaSample sample;
        sample.observedAt = utcDateTime(sampleMap.value(QStringLiteral("observedAt")));
        sample.resetAt = utcDateTime(sampleMap.value(QStringLiteral("resetAt")));
        if (sampleMap.value(QStringLiteral("remaining")).isValid()
            && !sampleMap.value(QStringLiteral("remaining")).isNull()) {
            sample.remaining = sampleMap.value(QStringLiteral("remaining")).toDouble();
        }
        if (sampleMap.value(QStringLiteral("limit")).isValid() && !sampleMap.value(QStringLiteral("limit")).isNull()) {
            sample.limit = sampleMap.value(QStringLiteral("limit")).toDouble();
        }
        sample.unit = sampleMap.value(QStringLiteral("unit"), request.unit).toString();
        sample.source = sampleMap.value(QStringLiteral("source")).toString();
        request.samples.append(sample);
    }
    return request;
}

RunwayQuery::BudgetRequest budgetRequestFromMap(const QVariantMap &map)
{
    RunwayQuery::BudgetRequest request;
    request.sourceId = map.value(QStringLiteral("sourceId")).toString();
    request.sourceKind = map.value(QStringLiteral("sourceKind")).toString();
    request.window = map.value(QStringLiteral("window"), QStringLiteral("calendar_month")).toString();
    request.scope = map.value(QStringLiteral("scope")).toString();
    request.budget = map.value(QStringLiteral("budget")).toDouble();
    request.budgetCurrency = map.value(QStringLiteral("budgetCurrency")).toString();
    request.valueClass = valueClassFromKey(map.value(QStringLiteral("valueClass")).toString());
    for (const QVariant &entry : map.value(QStringLiteral("days")).toList()) {
        const QVariantMap dayMap = entry.toMap();
        RunwayQuery::BudgetDay day;
        day.periodStart = utcDateTime(dayMap.value(QStringLiteral("periodStart")));
        day.periodEnd = utcDateTime(dayMap.value(QStringLiteral("periodEnd")));
        if (dayMap.value(QStringLiteral("value")).isValid() && !dayMap.value(QStringLiteral("value")).isNull()) {
            day.value = dayMap.value(QStringLiteral("value")).toDouble();
        }
        day.currency = dayMap.value(QStringLiteral("currency")).toString();
        day.valueClass = valueClassFromKey(dayMap.value(QStringLiteral("valueClass")).toString());
        request.days.append(day);
    }
    return request;
}

QList<RunwayQuery::QuotaSample> queryQuotaSamples(const QSqlDatabase &database, const QVariantMap &descriptor, bool *ok)
{
    QList<RunwayQuery::QuotaSample> samples;
    QSqlQuery query(database);
    query.setForwardOnly(true);
    query.prepare(QStringLiteral("SELECT remaining.observed_at_utc, remaining.value, quota.value, "
                                 "COALESCE(remaining.reset_at_utc, quota.reset_at_utc), "
                                 "remaining.unit, remaining.source "
                                 "FROM observations remaining "
                                 "JOIN observations quota "
                                 "ON quota.correlation_id = remaining.correlation_id "
                                 "AND quota.provider = remaining.provider "
                                 "AND quota.scope = remaining.scope "
                                 "AND quota.window = remaining.window "
                                 "WHERE remaining.provider = ? "
                                 "AND remaining.metric_kind = ? AND quota.metric_kind = ? "
                                 "AND remaining.window = ? AND remaining.scope = ? "
                                 "AND COALESCE(remaining.model_scope,'') = '' "
                                 "AND COALESCE(remaining.project_scope,'') = '' "
                                 "AND remaining.value IS NOT NULL AND quota.value IS NOT NULL "
                                 "ORDER BY remaining.observed_at_utc, remaining.id"));
    query.addBindValue(descriptor.value(QStringLiteral("provider")));
    query.addBindValue(descriptor.value(QStringLiteral("remainingKind")));
    query.addBindValue(descriptor.value(QStringLiteral("limitKind")));
    query.addBindValue(descriptor.value(QStringLiteral("window")));
    query.addBindValue(descriptor.value(QStringLiteral("scope")));
    if (!query.exec()) {
        *ok = false;
        return { };
    }
    while (query.next()) {
        RunwayQuery::QuotaSample sample;
        sample.observedAt = utcDateTime(query.value(0));
        sample.remaining = query.value(1).toDouble();
        sample.limit = query.value(2).toDouble();
        sample.resetAt = utcDateTime(query.value(3));
        sample.unit = query.value(4).toString();
        sample.source = query.value(5).toString();
        samples.append(sample);
    }
    return samples;
}

QList<RunwayQuery::BudgetDay> queryBudgetDays(
    const QSqlDatabase &database, const QVariantMap &descriptor, const QDateTime &generatedAt, bool *ok)
{
    const QDate today = generatedAt.toUTC().date();
    const QDate monthStart(today.year(), today.month(), 1);
    const QString requestedValueClass
        = descriptor.value(QStringLiteral("valueClass"), QStringLiteral("actual")).toString().trimmed().toLower();
    QSqlQuery query(database);
    query.setForwardOnly(true);
    query.prepare(QStringLiteral("SELECT observed_at_utc, interval_start_utc, interval_end_utc, value, "
                                 "currency, semantic, source, data_quality "
                                 "FROM observations WHERE provider = ? AND metric_kind = 'cost' "
                                 "AND window = 'day' AND scope = ? "
                                 "AND COALESCE(model_scope,'') = '' "
                                 "AND COALESCE(project_scope,'') = '' "
                                 "AND interval_start_utc >= ? AND interval_start_utc < ? "
                                 "AND value IS NOT NULL ORDER BY observed_at_utc, id"));
    query.addBindValue(descriptor.value(QStringLiteral("provider")));
    query.addBindValue(descriptor.value(QStringLiteral("scope")));
    query.addBindValue(monthStart.startOfDay(QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.addBindValue(today.startOfDay(QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!query.exec()) {
        *ok = false;
        return { };
    }

    QMap<QString, RunwayQuery::BudgetDay> latestByPeriod;
    while (query.next()) {
        const QDateTime start = utcDateTime(query.value(1));
        const QDateTime end = utcDateTime(query.value(2));
        if (!start.isValid() || !end.isValid() || start.secsTo(end) != 24 * 60 * 60 || start.time() != QTime(0, 0)
            || end.time() != QTime(0, 0)) {
            continue;
        }
        const QString observationClass
            = valueClassForObservation(query.value(5).toString(), query.value(6).toString(), query.value(7).toString());
        if (observationClass != requestedValueClass) {
            continue;
        }
        RunwayQuery::BudgetDay day;
        day.periodStart = start;
        day.periodEnd = end;
        day.value = query.value(3).toDouble();
        day.currency = query.value(4).toString();
        day.valueClass = valueClassFromKey(observationClass);
        latestByPeriod.insert(start.toString(Qt::ISODateWithMs), day);
    }
    return latestByPeriod.values();
}

QVariantMap quotaMapForDatabase(const QVariantMap &descriptor, const QSqlDatabase &database, bool *ok)
{
    QVariantMap map = descriptor;
    QVariantList samples;
    for (const RunwayQuery::QuotaSample &sample : queryQuotaSamples(database, descriptor, ok)) {
        samples.append(QVariantMap {
            { QStringLiteral("observedAt"), sample.observedAt },
            { QStringLiteral("resetAt"), sample.resetAt },
            { QStringLiteral("remaining"), sample.remaining ? QVariant(*sample.remaining) : QVariant() },
            { QStringLiteral("limit"), sample.limit ? QVariant(*sample.limit) : QVariant() },
            { QStringLiteral("unit"), sample.unit },
            { QStringLiteral("source"), sample.source },
        });
    }
    map.insert(QStringLiteral("samples"), samples);
    return map;
}

QVariantMap budgetMapForDatabase(
    const QVariantMap &descriptor, const QSqlDatabase &database, const QDateTime &generatedAt, bool *ok)
{
    QVariantMap map = descriptor;
    QVariantList days;
    for (const RunwayQuery::BudgetDay &day : queryBudgetDays(database, descriptor, generatedAt, ok)) {
        days.append(QVariantMap {
            { QStringLiteral("periodStart"), day.periodStart },
            { QStringLiteral("periodEnd"), day.periodEnd },
            { QStringLiteral("value"), day.value ? QVariant(*day.value) : QVariant() },
            { QStringLiteral("currency"), day.currency },
            { QStringLiteral("valueClass"), ForecastContract::valueClassKey(day.valueClass) },
        });
    }
    map.insert(QStringLiteral("days"), days);
    return map;
}

QVariantList queryDatabase(
    const QString &path, const QVariantMap &request, const QDateTime &generatedAt, const std::atomic_bool *cancelled)
{
    QVariantList results;
    const QString connectionName
        = QStringLiteral("aiusagemonitor_runway_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool databaseOk = true;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        database.setDatabaseName(path);
        databaseOk = database.open();
        if (databaseOk) {
            for (const QVariant &entry : request.value(QStringLiteral("quotaSources")).toList()) {
                if (cancelled && cancelled->load()) {
                    break;
                }
                bool queryOk = true;
                const QVariantMap map = quotaMapForDatabase(entry.toMap(), database, &queryOk);
                ForecastContract::Result result = queryOk
                    ? RunwayQuery::quotaRunway(quotaRequestFromMap(map), generatedAt)
                    : unavailableQuota(quotaRequestFromMap(entry.toMap()), generatedAt, QStringLiteral("query_failed"));
                results.append(result.toVariantMap());
            }
            for (const QVariant &entry : request.value(QStringLiteral("budgets")).toList()) {
                if (cancelled && cancelled->load()) {
                    break;
                }
                bool queryOk = true;
                const QVariantMap map = budgetMapForDatabase(entry.toMap(), database, generatedAt, &queryOk);
                ForecastContract::Result result = queryOk
                    ? RunwayQuery::budgetPacing(budgetRequestFromMap(map), generatedAt)
                    : unavailableBudget(
                          budgetRequestFromMap(entry.toMap()), generatedAt, QStringLiteral("query_failed"));
                results.append(result.toVariantMap());
            }
        }
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (!databaseOk) {
        for (const QVariant &entry : request.value(QStringLiteral("quotaSources")).toList()) {
            results.append(
                unavailableQuota(quotaRequestFromMap(entry.toMap()), generatedAt, QStringLiteral("query_failed"))
                    .toVariantMap());
        }
        for (const QVariant &entry : request.value(QStringLiteral("budgets")).toList()) {
            results.append(
                unavailableBudget(budgetRequestFromMap(entry.toMap()), generatedAt, QStringLiteral("query_failed"))
                    .toVariantMap());
        }
    }
    return results;
}

} // namespace

ForecastContract::Result RunwayQuery::quotaRunway(const QuotaRequest &request, const QDateTime &generatedAt)
{
    const QDateTime now = generatedAt.toUTC();
    QList<QuotaSample> samples = request.samples;
    std::sort(samples.begin(), samples.end(),
        [](const QuotaSample &left, const QuotaSample &right) { return left.observedAt < right.observedAt; });

    if (samples.isEmpty()) {
        return unavailableQuota(request, now, QStringLiteral("missing_value"));
    }
    const QDateTime periodEnd = samples.last().resetAt;
    const QString quotaSource = samples.first().source.trimmed().toLower();
    for (const QuotaSample &sample : std::as_const(samples)) {
        if (!sample.observedAt.isValid() || !sample.resetAt.isValid() || !sample.remaining || !sample.limit
            || !std::isfinite(*sample.remaining) || !std::isfinite(*sample.limit) || *sample.remaining < 0.0
            || *sample.limit <= 0.0 || *sample.remaining > *sample.limit + Epsilon) {
            return unavailableQuota(request, now, QStringLiteral("missing_value"), samples.size(), 0.0, periodEnd);
        }
        if (!supportedQuotaSource(sample.source)) {
            return unavailableQuota(request, now, QStringLiteral("unsupported_source"), samples.size(), 0.0, periodEnd);
        }
        if (sample.resetAt != periodEnd || sample.unit.compare(request.unit, Qt::CaseInsensitive) != 0
            || sample.source.trimmed().toLower() != quotaSource
            || std::abs(*sample.limit - *samples.first().limit) > Epsilon) {
            return unavailableQuota(request, now, QStringLiteral("reset_detected"), samples.size(), 0.0, periodEnd);
        }
    }

    const double coverage = quotaCoverage(samples);
    if (samples.size() < MinimumQuotaSamples) {
        return unavailableQuota(
            request, now, QStringLiteral("insufficient_samples"), samples.size(), coverage, periodEnd);
    }
    const qint64 span = samples.first().observedAt.secsTo(samples.last().observedAt);
    if (span < MinimumQuotaSpanSeconds) {
        return unavailableQuota(request, now, QStringLiteral("insufficient_span"), samples.size(), coverage, periodEnd);
    }
    if (samples.last().observedAt.secsTo(now) > MaximumQuotaAgeSeconds || samples.last().observedAt > now.addSecs(1)) {
        return unavailableQuota(request, now, QStringLiteral("stale_data"), samples.size(), coverage, periodEnd);
    }
    if (periodEnd <= now) {
        return unavailableQuota(
            request, now, QStringLiteral("incompatible_window"), samples.size(), coverage, periodEnd);
    }
    for (qsizetype index = 1; index < samples.size(); ++index) {
        if (*samples.at(index).remaining > *samples.at(index - 1).remaining + Epsilon) {
            return unavailableQuota(request, now, QStringLiteral("non_monotonic"), samples.size(), coverage, periodEnd);
        }
    }

    const double slope = medianConsumptionSlope(samples);
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
    result.coveragePercent = coverage;
    result.methodId = QStringLiteral("quota-runway-v1");
    result.generatedAt = now;
    result.valueClass = ForecastContract::ValueClass::Actual;
    const double volatility = quotaVolatility(samples, slope);
    result.evidenceGrade = samples.size() >= 8 && coverage >= 80.0 && volatility <= 0.25
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
    const double projected
        = std::max(0.0, *samples.last().remaining - slope * static_cast<double>(remainingWindowSeconds));
    result.projectedValue = projected;
    if (secondsToExhaustion >= static_cast<double>(remainingWindowSeconds)) {
        result.state = ForecastContract::State::Safe;
        return result;
    }

    result.predictedAt = predictionOrigin.addMSecs(static_cast<qint64>(secondsToExhaustion * 1000.0));
    const double criticalHorizon = std::max(60.0 * 60.0, static_cast<double>(now.secsTo(periodEnd)) * 0.25);
    result.state = now.msecsTo(*result.predictedAt) / 1000.0 <= criticalHorizon ? ForecastContract::State::Critical
                                                                                : ForecastContract::State::Warning;
    return result;
}

ForecastContract::Result RunwayQuery::budgetPacing(const BudgetRequest &request, const QDateTime &generatedAt)
{
    const QDateTime now = generatedAt.toUTC();
    const QDate today = now.date();
    const QDate monthStart(today.year(), today.month(), 1);
    const QDate monthEnd = monthStart.addMonths(1);
    const QDateTime periodEnd = monthEnd.startOfDay(QTimeZone::UTC);
    if (!std::isfinite(request.budget) || request.budget <= 0.0 || request.budgetCurrency.trimmed().isEmpty()) {
        return unavailableBudget(request, now, QStringLiteral("missing_budget"));
    }

    QMap<QDate, double> valuesByDay;
    QSet<QString> currencies;
    QSet<ForecastContract::ValueClass> valueClasses;
    bool missingValue = false;
    for (const BudgetDay &day : request.days) {
        if (!day.periodStart.isValid() || !day.periodEnd.isValid() || day.periodStart.toUTC().time() != QTime(0, 0)
            || day.periodEnd.toUTC().time() != QTime(0, 0) || day.periodStart.secsTo(day.periodEnd) != 24 * 60 * 60) {
            continue;
        }
        const QDate date = day.periodStart.toUTC().date();
        if (date < monthStart || date >= today) {
            continue;
        }
        if (!day.value || !std::isfinite(*day.value) || *day.value < 0.0) {
            missingValue = true;
            continue;
        }
        currencies.insert(day.currency.trimmed().toUpper());
        valueClasses.insert(day.valueClass);
        valuesByDay.insert(date, *day.value);
    }
    if (missingValue && valuesByDay.isEmpty()) {
        return unavailableBudget(request, now, QStringLiteral("missing_value"));
    }
    if (currencies.size() > 1) {
        return unavailableBudget(request, now, QStringLiteral("mixed_currency"), valuesByDay.size());
    }
    if (valueClasses.size() > 1 || (!valueClasses.isEmpty() && *valueClasses.cbegin() != request.valueClass)) {
        return unavailableBudget(request, now, QStringLiteral("mixed_value_class"), valuesByDay.size());
    }
    const QString currency = currencies.isEmpty() ? QString() : *currencies.cbegin();
    if (currency.isEmpty()) {
        return unavailableBudget(request, now, QStringLiteral("missing_value"));
    }
    if (currency != request.budgetCurrency.trimmed().toUpper()) {
        return unavailableBudget(request, now, QStringLiteral("currency_mismatch"), valuesByDay.size());
    }

    const int completedCalendarDays = monthStart.daysTo(today);
    const double coverage = completedCalendarDays > 0
        ? static_cast<double>(valuesByDay.size()) / static_cast<double>(completedCalendarDays) * 100.0
        : 0.0;
    if (valuesByDay.size() < MinimumBudgetDays) {
        return unavailableBudget(request, now, QStringLiteral("insufficient_samples"), valuesByDay.size(), coverage);
    }
    if (coverage + Epsilon < MinimumBudgetCoveragePercent) {
        return unavailableBudget(request, now, QStringLiteral("insufficient_coverage"), valuesByDay.size(), coverage);
    }

    QList<double> dailyValues = valuesByDay.values();
    const double dailyBaseline = median(dailyValues);
    double monthToDate = 0.0;
    for (double value : std::as_const(dailyValues)) {
        monthToDate += value;
    }
    const int remainingCalendarDays = today.daysTo(monthEnd);
    const double projected = monthToDate + dailyBaseline * remainingCalendarDays;
    QList<double> deviations;
    deviations.reserve(dailyValues.size());
    for (double value : std::as_const(dailyValues)) {
        deviations.append(std::abs(value - dailyBaseline));
    }
    const double volatility = dailyBaseline > Epsilon ? median(deviations) / dailyBaseline
                                                      : (std::all_of(dailyValues.cbegin(), dailyValues.cend(),
                                                             [](double value) { return std::abs(value) <= Epsilon; })
                                                                ? 0.0
                                                                : 1.0);

    ForecastContract::Result result;
    result.kind = ForecastContract::Kind::BudgetOverrun;
    result.sourceId = request.sourceId;
    result.sourceKind = request.sourceKind;
    result.window = request.window;
    result.scope = request.scope;
    result.currentValue = monthToDate;
    result.projectedValue = projected;
    result.limitValue = request.budget;
    result.unit = currency;
    result.currency = currency;
    result.periodEnd = periodEnd;
    result.sampleCount = valuesByDay.size();
    result.coveragePercent = coverage;
    result.evidenceGrade = valuesByDay.size() >= 10 && coverage >= 90.0 && volatility <= 0.25
            && valuesByDay.lastKey() == today.addDays(-1)
        ? ForecastContract::EvidenceGrade::Strong
        : ForecastContract::EvidenceGrade::Usable;
    result.methodId = QStringLiteral("budget-pacing-v1");
    result.generatedAt = now;
    result.valueClass = request.valueClass;

    if (projected <= request.budget + Epsilon) {
        result.state = ForecastContract::State::Safe;
        return result;
    }
    const double amountToLimit = request.budget - monthToDate;
    if (amountToLimit <= 0.0 || dailyBaseline <= Epsilon) {
        result.predictedAt = now;
    } else {
        result.predictedAt
            = today.startOfDay(QTimeZone::UTC)
                  .addMSecs(static_cast<qint64>(amountToLimit / dailyBaseline * 24.0 * 60.0 * 60.0 * 1000.0));
    }
    result.state = monthToDate >= request.budget || projected >= request.budget * 1.10
        ? ForecastContract::State::Critical
        : ForecastContract::State::Warning;
    return result;
}

QVariantList RunwayQuery::execute(const QVariantMap &request, const std::atomic_bool *cancelled)
{
    const QDateTime requestedGeneratedAt = utcDateTime(request.value(QStringLiteral("generatedAt")));
    const QDateTime generatedAt
        = requestedGeneratedAt.isValid() ? requestedGeneratedAt : QDateTime::currentDateTimeUtc();
    QVariantList results;
    for (const QVariant &entry : request.value(QStringLiteral("quotaSeries")).toList()) {
        if (cancelled && cancelled->load()) {
            return { };
        }
        results.append(quotaRunway(quotaRequestFromMap(entry.toMap()), generatedAt).toVariantMap());
    }
    for (const QVariant &entry : request.value(QStringLiteral("budgetSeries")).toList()) {
        if (cancelled && cancelled->load()) {
            return { };
        }
        results.append(budgetPacing(budgetRequestFromMap(entry.toMap()), generatedAt).toVariantMap());
    }
    const bool hasDatabaseDescriptors = !request.value(QStringLiteral("quotaSources")).toList().isEmpty()
        || !request.value(QStringLiteral("budgets")).toList().isEmpty();
    QString databasePath = request.value(QStringLiteral("databasePath")).toString();
    if (databasePath.isEmpty() && hasDatabaseDescriptors) {
        databasePath = defaultDatabasePath();
    }
    if (hasDatabaseDescriptors && !(cancelled && cancelled->load())) {
        results.append(queryDatabase(databasePath, request, generatedAt, cancelled));
    }
    return results;
}

QVariantMap RunwayQuery::methodContract()
{
    return {
        { QStringLiteral("quotaMethodId"), QStringLiteral("quota-runway-v1") },
        { QStringLiteral("quotaSlope"), QStringLiteral("bounded_theil_sen_256") },
        { QStringLiteral("minimumQuotaSamples"), MinimumQuotaSamples },
        { QStringLiteral("minimumQuotaSpanSeconds"), MinimumQuotaSpanSeconds },
        { QStringLiteral("maximumQuotaAgeSeconds"), MaximumQuotaAgeSeconds },
        { QStringLiteral("quotaCriticalRule"),
            QStringLiteral("within_max_60_minutes_or_25_percent_of_remaining_window") },
        { QStringLiteral("budgetMethodId"), QStringLiteral("budget-pacing-v1") },
        { QStringLiteral("minimumBudgetDays"), MinimumBudgetDays },
        { QStringLiteral("minimumBudgetCoveragePercent"), MinimumBudgetCoveragePercent },
        { QStringLiteral("budgetCoverage"),
            QStringLiteral("reported_completed_utc_days_over_elapsed_completed_utc_days") },
        { QStringLiteral("budgetBaseline"), QStringLiteral("median_compatible_completed_utc_day") },
        { QStringLiteral("budgetCriticalRule"),
            QStringLiteral("already_over_budget_or_projected_at_least_110_percent") },
        { QStringLiteral("strongEvidence"),
            QStringLiteral("quota_8_samples_80_percent_low_mad_budget_10_days_90_percent_fresh_low_mad") },
    };
}

QString RunwayQuery::defaultDatabasePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
}
