#include "openaiprovider.h"
#include <KLocalizedString>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonParseError>
#include <QMap>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QUrlQuery>

#include <limits>
#include <utility>

namespace {
QJsonArray bucketResults(const QJsonObject &bucket)
{
    QJsonArray results = bucket.value(QStringLiteral("results")).toArray();
    if (results.isEmpty()) {
        results = bucket.value(QStringLiteral("result")).toArray();
    }
    return results;
}

double parseOpenAiCostAmountUsd(const QJsonObject &row)
{
    const QJsonValue amountValue = row.value(QStringLiteral("amount"));

    if (amountValue.isObject()) {
        const QJsonObject amount = amountValue.toObject();
        const QString currency = amount.value(QStringLiteral("currency")).toString().toLower();

        if (!currency.isEmpty() && currency != QLatin1String("usd")) {
            qWarning() << "OpenAI cost currency is not USD:" << currency;
        }

        return amount.value(QStringLiteral("value")).toDouble(0.0);
    }

    // Legacy/mock fallback only. Old fixtures represented cents as a number.
    return amountValue.toDouble(0.0) / 100.0;
}

QByteArray jsonDigest(const QJsonValue &value)
{
    return QCryptographicHash::hash(
        QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
}

struct BucketRange {
    QDateTime start;
    QDateTime end;
};

BucketRange bucketRange(const QJsonArray &buckets)
{
    qint64 first = std::numeric_limits<qint64>::max();
    qint64 last = std::numeric_limits<qint64>::min();
    for (const QJsonValue &value : buckets) {
        const QJsonObject bucket = value.toObject();
        if (!bucket.value(QStringLiteral("start_time")).isDouble()
            || !bucket.value(QStringLiteral("end_time")).isDouble()) {
            continue;
        }
        first = qMin(first,
                     bucket.value(QStringLiteral("start_time")).toInteger());
        last = qMax(last,
                    bucket.value(QStringLiteral("end_time")).toInteger());
    }
    if (first == std::numeric_limits<qint64>::max()
        || last == std::numeric_limits<qint64>::min() || last <= first) {
        return {};
    }
    return {
        QDateTime::fromSecsSinceEpoch(first, QTimeZone::UTC),
        QDateTime::fromSecsSinceEpoch(last, QTimeZone::UTC),
    };
}

QString costScope(const QString &lineItem)
{
    return lineItem.isEmpty()
        ? QStringLiteral("organization_scoped")
        : QStringLiteral("organization_scoped:line_item:") + lineItem;
}
} // namespace

OpenAIProvider::OpenAIProvider(QObject *parent)
    : ProviderBackend(parent)
{
    registerCatalogPricing(QStringLiteral("openai"));
    setPricingModel(m_model);
}

QString OpenAIProvider::projectId() const { return m_projectId; }
void OpenAIProvider::setProjectId(const QString &id)
{
    if (m_projectId != id) {
        m_projectId = id;
        Q_EMIT projectIdChanged();
    }
}

QString OpenAIProvider::model() const { return m_model; }
void OpenAIProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        setPricingModel(m_model);
        Q_EMIT modelChanged();
    }
}

void OpenAIProvider::refreshImpl()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();
    m_pendingRequests = 0;
    m_logicalPageRequests = 0;
    m_usagePagination = { };
    m_dailyCostsPagination = { };
    m_monthlyCostsPagination = { };
    setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("pending"));
    setCapabilityStatus(QStringLiteral("daily_billing"), QStringLiteral("pending"));
    setCapabilityStatus(QStringLiteral("monthly_billing"), QStringLiteral("pending"));

    fetchUsage();
    fetchCosts();
    fetchMonthlyCosts();
}

QDateTime OpenAIProvider::currentDateTimeUtc() const { return QDateTime::currentDateTimeUtc(); }

void OpenAIProvider::fetchUsage()
{
    // Query the last 24 hours of completion usage
    const QDateTime now = currentDateTimeUtc();
    const QDateTime dayAgo = now.addDays(-1);

    QUrl url(QStringLiteral("%1/organization/usage/completions").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(dayAgo.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(USAGE_BUCKET_LIMIT));
    query.addQueryItem(QStringLiteral("group_by"), QStringLiteral("model"));
    query.addQueryItem(QStringLiteral("group_by"), QStringLiteral("project_id"));

    // Add filters
    if (!m_model.isEmpty()) {
        query.addQueryItem(QStringLiteral("models"), m_model);
    }
    if (!m_projectId.isEmpty()) {
        query.addQueryItem(QStringLiteral("project_ids"), m_projectId);
    }

    url.setQuery(query);
    startPagination(RequestKind::Usage, url);
}

void OpenAIProvider::fetchCosts()
{
    const QDateTime now = currentDateTimeUtc();
    const QDateTime dayAgo = now.addDays(-1);

    QUrl url(QStringLiteral("%1/organization/costs").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(dayAgo.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(COST_BUCKET_LIMIT));
    query.addQueryItem(QStringLiteral("group_by"),
                       QStringLiteral("project_id"));
    query.addQueryItem(QStringLiteral("group_by"),
                       QStringLiteral("line_item"));

    if (!m_projectId.isEmpty()) {
        query.addQueryItem(QStringLiteral("project_ids"), m_projectId);
    }

    url.setQuery(query);
    startPagination(RequestKind::DailyCosts, url);
}

void OpenAIProvider::onUsageReply(QNetworkReply *reply) { handlePage(RequestKind::Usage, reply); }

void OpenAIProvider::onCostsReply(QNetworkReply *reply) { handlePage(RequestKind::DailyCosts, reply); }

void OpenAIProvider::fetchMonthlyCosts()
{
    // Query costs from the start of the current month
    const QDateTime now = currentDateTimeUtc();
    const QDate today = now.date();
    const QDate monthStart(today.year(), today.month(), 1);
    const QDateTime monthStartDt(monthStart.startOfDay(QTimeZone::UTC));

    QUrl url(QStringLiteral("%1/organization/costs").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(monthStartDt.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(COST_BUCKET_LIMIT));
    query.addQueryItem(QStringLiteral("group_by"),
                       QStringLiteral("project_id"));
    query.addQueryItem(QStringLiteral("group_by"),
                       QStringLiteral("line_item"));

    if (!m_projectId.isEmpty()) {
        query.addQueryItem(QStringLiteral("project_ids"), m_projectId);
    }

    url.setQuery(query);
    startPagination(RequestKind::MonthlyCosts, url);
}

void OpenAIProvider::onMonthlyCostsReply(QNetworkReply *reply) { handlePage(RequestKind::MonthlyCosts, reply); }

void OpenAIProvider::startPagination(RequestKind kind, const QUrl &url)
{
    PaginationState &state = paginationState(kind);
    state = { };
    state.baseUrl = url;
    state.generation = currentGeneration();
    state.pending = true;
    ++m_pendingRequests;
    requestPage(kind);
}

void OpenAIProvider::requestPage(RequestKind kind, const QString &cursor)
{
    PaginationState &state = paginationState(kind);
    if (!state.pending || !isCurrentGeneration(state.generation)) {
        return;
    }
    if (m_logicalPageRequests >= MAX_LOGICAL_PAGE_REQUESTS) {
        finishPagination(
            kind, false, i18n("OpenAI pagination exceeded the request safety limit"), ProviderErrorKind::Schema);
        return;
    }

    QUrl url = state.baseUrl;
    QUrlQuery query(url);
    query.removeAllQueryItems(QStringLiteral("page"));
    if (!cursor.isEmpty()) {
        query.addQueryItem(QStringLiteral("page"), cursor);
    }
    url.setQuery(query);

    ++m_logicalPageRequests;
    QNetworkReply *reply = networkManager()->get(createRequest(url));
    trackReply(reply);
    const int generation = state.generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind, generation]() {
        if (!isCurrentGeneration(generation)) {
            reply->deleteLater();
            return;
        }
        handlePage(kind, reply);
    });
}

void OpenAIProvider::handlePage(RequestKind kind, QNetworkReply *reply, bool retriesExhausted)
{
    PaginationState &state = paginationState(kind);
    if (!state.pending || !isCurrentGeneration(state.generation)) {
        reply->deleteLater();
        return;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        if (isRetryableStatus(httpStatus) && !retriesExhausted) {
            const QUrl url = reply->url();
            retryRequest(reply, url, QByteArray(),
                [this, kind](QNetworkReply *retryReply) { handlePage(kind, retryReply, true); });
            return;
        }

        const ProviderErrorKind errorKind = errorKindForNetworkReply(reply);
        const QDateTime retryAfter = retryAfterForReply(reply);
        const QString diagnostic = httpStatus == 401 ? i18n("Authentication failed. Ensure you're using "
                                                            "an Admin API key.")
            : httpStatus == 403                      ? i18n("The OpenAI Admin API key lacks the required permission")
            : kind == RequestKind::Usage             ? i18n("OpenAI usage data is unavailable")
                                                     : i18n("OpenAI billing data is unavailable");
        reply->deleteLater();
        finishPagination(kind, false, diagnostic, errorKind, httpStatus, retryAfter);
        return;
    }

    parseRateLimitHeaders(reply);
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finishPagination(
            kind, false, i18n("The OpenAI response is not valid JSON"), ProviderErrorKind::Schema, httpStatus);
        return;
    }

    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("data")).isArray()) {
        finishPagination(
            kind, false, i18n("The OpenAI response is missing bucket data"), ProviderErrorKind::Schema, httpStatus);
        return;
    }

    const QByteArray pagePayload = QCryptographicHash::hash(
        QJsonDocument(root.value(QStringLiteral("data")).toArray()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256);
    if (!state.pagePayloads.contains(pagePayload)) {
        QString diagnostic;
        if (!appendUniqueBuckets(state, root.value(QStringLiteral("data")).toArray(), &diagnostic)) {
            finishPagination(kind, false, diagnostic, ProviderErrorKind::Schema, httpStatus);
            return;
        }
        state.pagePayloads.insert(pagePayload);
    }

    const QJsonValue hasMoreValue = root.value(QStringLiteral("has_more"));
    const QJsonValue nextPageValue = root.value(QStringLiteral("next_page"));
    if (!hasMoreValue.isUndefined() && !hasMoreValue.isBool()) {
        finishPagination(kind, false, i18n("The OpenAI response has invalid pagination state"),
            ProviderErrorKind::Schema, httpStatus);
        return;
    }
    if (!nextPageValue.isUndefined() && !nextPageValue.isNull() && !nextPageValue.isString()) {
        finishPagination(kind, false, i18n("The OpenAI response has an invalid next-page cursor"),
            ProviderErrorKind::Schema, httpStatus);
        return;
    }

    const QString nextPage = nextPageValue.toString();
    const bool hasMore = hasMoreValue.isBool() ? hasMoreValue.toBool() : !nextPage.isEmpty();
    if (!hasMore) {
        finishPagination(kind, true);
        return;
    }
    if (nextPage.isEmpty()) {
        finishPagination(kind, false, i18n("OpenAI pagination did not provide a next-page cursor"),
            ProviderErrorKind::Schema, httpStatus);
        return;
    }
    if (state.requestedCursors.contains(nextPage)) {
        finishPagination(
            kind, false, i18n("OpenAI pagination repeated a cursor"), ProviderErrorKind::Schema, httpStatus);
        return;
    }

    state.requestedCursors.insert(nextPage);
    requestPage(kind, nextPage);
}

void OpenAIProvider::finishPagination(RequestKind kind, bool complete, const QString &diagnostic,
    ProviderErrorKind errorKind, int httpStatus, const QDateTime &retryAfter)
{
    PaginationState &state = paginationState(kind);
    if (!state.pending) {
        return;
    }
    state.pending = false;
    --m_pendingRequests;

    if (complete) {
        switch (kind) {
        case RequestKind::Usage:
            publishUsage(state.buckets);
            m_hasSuccessfulUsage = true;
            break;
        case RequestKind::DailyCosts:
            publishDailyCosts(state.buckets);
            m_hasSuccessfulDailyCosts = true;
            break;
        case RequestKind::MonthlyCosts:
            publishMonthlyCosts(state.buckets);
            m_hasSuccessfulMonthlyCosts = true;
            break;
        }
        setCapabilityStatus(capabilityName(kind), QStringLiteral("available"));
    } else {
        const QString status = hasSuccessfulValue(kind)
            ? QStringLiteral("stale")
            : (state.buckets.isEmpty() ? QStringLiteral("failed") : QStringLiteral("partial"));
        setCapabilityStatus(capabilityName(kind), status, diagnostic);
        if (errorKind != ProviderErrorKind::None) {
            setErrorDetails(diagnostic, errorKind, httpStatus, retryAfter);
        }
    }

    checkAllDone();
}

bool OpenAIProvider::appendUniqueBuckets(PaginationState &state, const QJsonArray &buckets, QString *diagnostic)
{
    for (const QJsonValue &value : buckets) {
        if (!value.isObject()) {
            *diagnostic = i18n("The OpenAI response contains an invalid bucket");
            return false;
        }
        const QJsonObject bucket = value.toObject();
        const bool hasStart = bucket.value(QStringLiteral("start_time")).isDouble();
        const bool hasEnd = bucket.value(QStringLiteral("end_time")).isDouble();
        if (hasStart != hasEnd) {
            *diagnostic = i18n("The OpenAI response contains incomplete bucket boundaries");
            return false;
        }

        const QByteArray payload = jsonDigest(value);
        const QString key = hasStart ? QStringLiteral("%1:%2")
                                           .arg(bucket.value(QStringLiteral("start_time")).toInteger())
                                           .arg(bucket.value(QStringLiteral("end_time")).toInteger())
                                     : QStringLiteral("payload:%1").arg(QString::fromLatin1(payload.toHex()));
        const auto existing = state.bucketPayloads.constFind(key);
        if (existing != state.bucketPayloads.constEnd()) {
            if (*existing != payload) {
                *diagnostic = i18n("OpenAI pagination returned conflicting duplicate buckets");
                return false;
            }
            continue;
        }

        state.bucketPayloads.insert(key, payload);
        state.buckets.append(bucket);
    }
    return true;
}

void OpenAIProvider::publishUsage(const QJsonArray &buckets)
{
    struct UsageTotals {
        qint64 input = 0;
        qint64 output = 0;
        int requests = 0;
        QString model;
        QString project;
    };
    qint64 totalInput = 0;
    qint64 totalOutput = 0;
    int totalRequests = 0;
    QMap<QString, UsageTotals> scopedTotals;
    for (const QJsonValue &bucket : buckets) {
        for (const QJsonValue &result : bucketResults(bucket.toObject())) {
            const QJsonObject row = result.toObject();
            const qint64 input =
                row.value(QStringLiteral("input_tokens")).toInteger(0);
            const qint64 output =
                row.value(QStringLiteral("output_tokens")).toInteger(0);
            const int requests =
                row.value(QStringLiteral("num_model_requests")).toInt(0);
            totalInput += input;
            totalOutput += output;
            totalRequests += requests;

            const QString model =
                row.value(QStringLiteral("model")).toString();
            const QString project =
                row.value(QStringLiteral("project_id")).toString();
            const QString key =
                QStringList{model, project}.join(QChar(0x1f));
            UsageTotals &totals = scopedTotals[key];
            totals.input += input;
            totals.output += output;
            totals.requests += requests;
            totals.model = model;
            totals.project = project;
        }
    }
    const BucketRange range = bucketRange(buckets);
    removeProviderMetrics(
        MetricSource::UsageApi, QStringLiteral("day"),
        {MetricKind::InputTokens, MetricKind::OutputTokens,
         MetricKind::Requests});
    setActualUsage(totalInput, totalOutput, totalRequests);
    setProviderMetric(MetricKind::InputTokens, totalInput,
                      QStringLiteral("token"), QString(),
                      QStringLiteral("organization"), QStringLiteral("day"),
                      MetricSource::UsageApi, QStringLiteral("actual"), {},
                      range.start, range.end);
    setProviderMetric(MetricKind::OutputTokens, totalOutput,
                      QStringLiteral("token"), QString(),
                      QStringLiteral("organization"), QStringLiteral("day"),
                      MetricSource::UsageApi, QStringLiteral("actual"), {},
                      range.start, range.end);
    setProviderMetric(MetricKind::Requests, totalRequests,
                      QStringLiteral("request"), QString(),
                      QStringLiteral("organization"), QStringLiteral("day"),
                      MetricSource::UsageApi, QStringLiteral("actual"), {},
                      range.start, range.end);
    for (const UsageTotals &totals : std::as_const(scopedTotals)) {
        setProviderMetric(
            MetricKind::InputTokens, totals.input, QStringLiteral("token"),
            QString(), QStringLiteral("organization_scoped"),
            QStringLiteral("day"), MetricSource::UsageApi,
            QStringLiteral("actual"), {}, range.start, range.end, totals.model,
            totals.project);
        setProviderMetric(
            MetricKind::OutputTokens, totals.output, QStringLiteral("token"),
            QString(), QStringLiteral("organization_scoped"),
            QStringLiteral("day"), MetricSource::UsageApi,
            QStringLiteral("actual"), {}, range.start, range.end, totals.model,
            totals.project);
        setProviderMetric(
            MetricKind::Requests, totals.requests, QStringLiteral("request"),
            QString(), QStringLiteral("organization_scoped"),
            QStringLiteral("day"), MetricSource::UsageApi,
            QStringLiteral("actual"), {}, range.start, range.end, totals.model,
            totals.project);
    }
    setUsageSource(QStringLiteral("actual_api"));
    setDataQuality(QStringLiteral("actual_usage"));
    setConnected(true);
}

void OpenAIProvider::publishDailyCosts(const QJsonArray &buckets)
{
    struct CostTotals {
        double value = 0.0;
        QString project;
        QString lineItem;
    };
    double totalCost = 0.0;
    QMap<QString, CostTotals> scopedTotals;
    for (const QJsonValue &bucket : buckets) {
        for (const QJsonValue &result : bucketResults(bucket.toObject())) {
            const QJsonObject row = result.toObject();
            const double value = parseOpenAiCostAmountUsd(row);
            totalCost += value;
            const QString project =
                row.value(QStringLiteral("project_id")).toString();
            const QString lineItem =
                row.value(QStringLiteral("line_item")).toString();
            const QString key =
                QStringList{project, lineItem}.join(QChar(0x1f));
            CostTotals &totals = scopedTotals[key];
            totals.value += value;
            totals.project = project;
            totals.lineItem = lineItem;
        }
    }

    const QDateTime observedAt = currentDateTimeUtc();
    const BucketRange range = bucketRange(buckets);
    removeProviderMetrics(MetricSource::BillingApi, QStringLiteral("day"),
                          {MetricKind::Cost});
    setCurrency(QStringLiteral("USD"));
    setCost(totalCost);
    setCostSource(QStringLiteral("billing_api"));
    setDataQuality(QStringLiteral("actual_billing"));
    setDailyCost(totalCost);
    setProviderMetric(MetricKind::Cost, totalCost, QStringLiteral("USD"), QStringLiteral("USD"),
        QStringLiteral("organization"), QStringLiteral("day"), MetricSource::BillingApi, QStringLiteral("actual"), { },
        range.start.isValid() ? range.start : observedAt.addDays(-1),
        range.end.isValid() ? range.end : observedAt);
    for (const CostTotals &totals : std::as_const(scopedTotals)) {
        setProviderMetric(
            MetricKind::Cost, totals.value, QStringLiteral("USD"),
            QStringLiteral("USD"), costScope(totals.lineItem),
            QStringLiteral("day"), MetricSource::BillingApi,
            QStringLiteral("actual"), {},
            range.start.isValid() ? range.start : observedAt.addDays(-1),
            range.end.isValid() ? range.end : observedAt, QString(),
            totals.project, QString(), totals.lineItem);
    }
    setConnected(true);
}

void OpenAIProvider::publishMonthlyCosts(const QJsonArray &buckets)
{
    struct CostTotals {
        double value = 0.0;
        QString project;
        QString lineItem;
    };
    double totalCost = 0.0;
    QMap<QString, CostTotals> scopedTotals;
    for (const QJsonValue &bucket : buckets) {
        for (const QJsonValue &result : bucketResults(bucket.toObject())) {
            const QJsonObject row = result.toObject();
            const double value = parseOpenAiCostAmountUsd(row);
            totalCost += value;
            const QString project =
                row.value(QStringLiteral("project_id")).toString();
            const QString lineItem =
                row.value(QStringLiteral("line_item")).toString();
            const QString key =
                QStringList{project, lineItem}.join(QChar(0x1f));
            CostTotals &totals = scopedTotals[key];
            totals.value += value;
            totals.project = project;
            totals.lineItem = lineItem;
        }
    }

    const QDateTime observedAt = currentDateTimeUtc();
    const QDate monthStart(observedAt.date().year(), observedAt.date().month(), 1);
    const BucketRange range = bucketRange(buckets);
    removeProviderMetrics(MetricSource::BillingApi, QStringLiteral("month"),
                          {MetricKind::Cost});
    setMonthlyCost(totalCost);
    setCostSource(QStringLiteral("billing_api"));
    setCurrency(QStringLiteral("USD"));
    setDataQuality(QStringLiteral("actual_billing"));
    setProviderMetric(MetricKind::Cost, totalCost, QStringLiteral("USD"), QStringLiteral("USD"),
        QStringLiteral("organization"), QStringLiteral("month"), MetricSource::BillingApi, QStringLiteral("actual"),
        { },
        range.start.isValid() ? range.start
                              : monthStart.startOfDay(QTimeZone::UTC),
        range.end.isValid() ? range.end : observedAt);
    for (const CostTotals &totals : std::as_const(scopedTotals)) {
        setProviderMetric(
            MetricKind::Cost, totals.value, QStringLiteral("USD"),
            QStringLiteral("USD"), costScope(totals.lineItem),
            QStringLiteral("month"), MetricSource::BillingApi,
            QStringLiteral("actual"), {},
            range.start.isValid()
                ? range.start
                : monthStart.startOfDay(QTimeZone::UTC),
            range.end.isValid() ? range.end : observedAt, QString(),
            totals.project, QString(), totals.lineItem);
    }
    setConnected(true);
}

OpenAIProvider::PaginationState &OpenAIProvider::paginationState(RequestKind kind)
{
    switch (kind) {
    case RequestKind::Usage:
        return m_usagePagination;
    case RequestKind::DailyCosts:
        return m_dailyCostsPagination;
    case RequestKind::MonthlyCosts:
        return m_monthlyCostsPagination;
    }
    Q_UNREACHABLE_RETURN(m_usagePagination);
}

QString OpenAIProvider::capabilityName(RequestKind kind) const
{
    switch (kind) {
    case RequestKind::Usage:
        return QStringLiteral("usage");
    case RequestKind::DailyCosts:
        return QStringLiteral("daily_billing");
    case RequestKind::MonthlyCosts:
        return QStringLiteral("monthly_billing");
    }
    return { };
}

bool OpenAIProvider::hasSuccessfulValue(RequestKind kind) const
{
    switch (kind) {
    case RequestKind::Usage:
        return m_hasSuccessfulUsage;
    case RequestKind::DailyCosts:
        return m_hasSuccessfulDailyCosts;
    case RequestKind::MonthlyCosts:
        return m_hasSuccessfulMonthlyCosts;
    }
    return false;
}

void OpenAIProvider::checkAllDone()
{
    if (m_pendingRequests <= 0) {
        setLoading(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();
    }
}
