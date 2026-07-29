#include "guardrailmodel.h"

#include "forecastcontract.h"
#include "runwayquery.h"

#include <KLocalizedString>
#include <QtConcurrentRun>

#include <algorithm>

GuardrailModel::GuardrailModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

GuardrailModel::~GuardrailModel()
{
    for (const std::shared_ptr<Work> &work : std::as_const(m_work)) {
        work->cancelled->store(true);
    }
}

int GuardrailModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_forecasts.size(); }

QVariant GuardrailModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_forecasts.size()) {
        return { };
    }
    const QVariantMap row = m_forecasts.at(index.row()).toMap();
    const QByteArray name = roleNames().value(role);
    return name.isEmpty() ? QVariant() : row.value(QString::fromLatin1(name));
}

QHash<int, QByteArray> GuardrailModel::roleNames() const
{
    return {
        { StableIdRole, "stableId" },
        { KindRole, "kind" },
        { StateRole, "state" },
        { SourceIdRole, "sourceId" },
        { SourceKindRole, "sourceKind" },
        { WindowRole, "window" },
        { ScopeRole, "scope" },
        { CurrentValueRole, "currentValue" },
        { ProjectedValueRole, "projectedValue" },
        { LimitValueRole, "limitValue" },
        { UnitRole, "unit" },
        { CurrencyRole, "currency" },
        { PredictedAtRole, "predictedAt" },
        { PeriodEndRole, "periodEnd" },
        { SampleCountRole, "sampleCount" },
        { CoveragePercentRole, "coveragePercent" },
        { EvidenceGradeRole, "evidenceGrade" },
        { MethodIdRole, "methodId" },
        { ReasonKeyRole, "reasonKey" },
        { ReasonTextRole, "reasonText" },
        { GeneratedAtRole, "generatedAt" },
        { ValueClassRole, "valueClass" },
    };
}

QVariantMap GuardrailModel::query() const { return m_query; }

void GuardrailModel::setQuery(const QVariantMap &query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    Q_EMIT queryChanged();
}

QVariantList GuardrailModel::forecasts() const { return m_forecasts; }

bool GuardrailModel::isBusy() const { return m_pendingWorkerCount > 0; }

int GuardrailModel::generation() const { return m_generation; }

int GuardrailModel::pendingWorkerCount() const { return m_pendingWorkerCount; }

QVariantMap GuardrailModel::methodContract() const { return RunwayQuery::methodContract(); }

void GuardrailModel::refresh() { start(m_query); }

void GuardrailModel::refreshWithQuery(const QVariantMap &query)
{
    setQuery(query);
    start(query);
}

void GuardrailModel::cancel()
{
    ++m_generation;
    Q_EMIT generationChanged();
    for (const std::shared_ptr<Work> &work : std::as_const(m_work)) {
        work->cancelled->store(true);
    }
}

QString GuardrailModel::localizedReason(const QString &reasonKey) const
{
    if (reasonKey == QLatin1String("insufficient_samples")) {
        return i18n("Not enough compatible observations");
    }
    if (reasonKey == QLatin1String("insufficient_span")) {
        return i18n("The observation span is too short");
    }
    if (reasonKey == QLatin1String("stale_data")) {
        return i18n("The latest compatible observation is stale");
    }
    if (reasonKey == QLatin1String("missing_value")) {
        return i18n("A required value is unavailable");
    }
    if (reasonKey == QLatin1String("unsupported_source")) {
        return i18n("The source cannot support this forecast");
    }
    if (reasonKey == QLatin1String("incompatible_window")) {
        return i18n("The quota window is incompatible");
    }
    if (reasonKey == QLatin1String("reset_detected")) {
        return i18n("The quota reset changed during the observation series");
    }
    if (reasonKey == QLatin1String("non_monotonic")) {
        return i18n("The remaining quota increased within one reset window");
    }
    if (reasonKey == QLatin1String("no_consumption")) {
        return i18n("No quota consumption was observed");
    }
    if (reasonKey == QLatin1String("missing_budget")) {
        return i18n("No compatible monthly budget is configured");
    }
    if (reasonKey == QLatin1String("mixed_currency")) {
        return i18n("The observations use more than one currency");
    }
    if (reasonKey == QLatin1String("currency_mismatch")) {
        return i18n("The budget and observations use different currencies");
    }
    if (reasonKey == QLatin1String("mixed_value_class")) {
        return i18n("Actual and estimated values cannot share one forecast");
    }
    if (reasonKey == QLatin1String("insufficient_coverage")) {
        return i18n("Too few completed UTC days are represented");
    }
    if (reasonKey == QLatin1String("incomplete_period")) {
        return i18n("The forecast period is incomplete");
    }
    if (reasonKey == QLatin1String("cancelled")) {
        return i18n("The forecast request was superseded");
    }
    if (reasonKey == QLatin1String("query_failed")) {
        return i18n("The local history query failed");
    }
    return reasonKey.isEmpty() ? QString() : i18n("Forecast unavailable");
}

void GuardrailModel::start(const QVariantMap &query)
{
    ++m_generation;
    Q_EMIT generationChanged();
    for (const std::shared_ptr<Work> &previous : std::as_const(m_work)) {
        previous->cancelled->store(true);
    }

    auto work = std::make_shared<Work>();
    work->generation = m_generation;
    work->cancelled = std::make_shared<std::atomic_bool>(false);
    work->watcher = new QFutureWatcher<QVariantList>(this);
    m_work.append(work);
    const bool wasBusy = isBusy();
    ++m_pendingWorkerCount;
    Q_EMIT pendingWorkerCountChanged();
    if (!wasBusy) {
        Q_EMIT busyChanged();
    }

    connect(work->watcher, &QFutureWatcher<QVariantList>::finished, this, [this, work]() { finish(work); });
    const std::shared_ptr<std::atomic_bool> cancellation = work->cancelled;
    work->watcher->setFuture(
        QtConcurrent::run([query, cancellation]() { return RunwayQuery::execute(query, cancellation.get()); }));
}

void GuardrailModel::finish(const std::shared_ptr<Work> &work)
{
    const bool isCurrent = work->generation == m_generation && !work->cancelled->load();
    if (isCurrent) {
        QVariantList forecasts;
        const QVariantList result = work->watcher->result();
        forecasts.reserve(result.size());
        for (const QVariant &entry : result) {
            forecasts.append(decorated(entry.toMap()));
        }
        beginResetModel();
        m_forecasts = forecasts;
        endResetModel();
        Q_EMIT forecastsChanged();
        Q_EMIT completed(work->generation);
    }

    work->watcher->deleteLater();
    m_work.removeAll(work);
    const bool wasBusy = isBusy();
    m_pendingWorkerCount = std::max(0, m_pendingWorkerCount - 1);
    Q_EMIT pendingWorkerCountChanged();
    if (wasBusy != isBusy()) {
        Q_EMIT busyChanged();
    }
}

QVariantMap GuardrailModel::decorated(const QVariantMap &forecast) const
{
    QVariantMap result = forecast;
    QString diagnostic;
    const std::optional<ForecastContract::Result> parsed = ForecastContract::fromVariantMap(forecast, &diagnostic);
    result.insert(QStringLiteral("stableId"), parsed ? parsed->stableId() : QString());
    result.insert(
        QStringLiteral("reasonText"), localizedReason(forecast.value(QStringLiteral("reasonKey")).toString()));
    return result;
}
