#include "googleveoprovider.h"
#include "providerpricingcatalog.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

namespace {
double extractDurationSeconds(const QJsonObject &payload)
{
    const QJsonObject usage = payload.value(QStringLiteral("usage")).toObject();
    const QJsonObject metadata = payload.value(QStringLiteral("metadata")).toObject();

    const auto readPositive = [](const QJsonObject &obj, const char *key) -> double {
        if (obj.isEmpty()) {
            return 0.0;
        }
        return qMax(0.0, obj.value(QLatin1String(key)).toDouble(0.0));
    };

    const double usageDuration = qMax(
        readPositive(usage, "video_duration_seconds"),
        qMax(readPositive(usage, "duration_seconds"), readPositive(usage, "generated_seconds")));
    if (usageDuration > 0.0) {
        return usageDuration;
    }

    const double payloadDuration = qMax(
        readPositive(payload, "video_duration_seconds"),
        qMax(readPositive(payload, "duration_seconds"), readPositive(payload, "generated_seconds")));
    if (payloadDuration > 0.0) {
        return payloadDuration;
    }

    return qMax(
        readPositive(metadata, "video_duration_seconds"),
        qMax(readPositive(metadata, "duration_seconds"), readPositive(metadata, "generated_seconds")));
}

double modelCostPerSecond(const QString &model)
{
    return ProviderPricingCatalog::instance()->amountForModelUnit(QStringLiteral("googleveo"), model, QStringLiteral("video_second"));
}
} // namespace

GoogleVeoProvider::GoogleVeoProvider(QObject *parent)
    : ProviderBackend(parent)
{
}

QString GoogleVeoProvider::model() const { return m_model; }
void GoogleVeoProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        Q_EMIT modelChanged();
    }
}

QString GoogleVeoProvider::tier() const { return m_tier; }
void GoogleVeoProvider::setTier(const QString &tier)
{
    if (m_tier != tier) {
        m_tier = tier;
        Q_EMIT tierChanged();
    }
}

void GoogleVeoProvider::refreshImpl()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();
    fetchModelInfo();
}

void GoogleVeoProvider::fetchModelInfo()
{
    // GET /v1beta/models/{model} as a lightweight connectivity check
    QUrl url(QStringLiteral("%1/models/%2")
                 .arg(effectiveBaseUrl(BASE_URL), m_model));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), apiKey());
    url.setQuery(query);

    // Use createRequest for timeout, then clear Bearer auth (Google uses query-param auth)
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", QByteArray());

    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onModelInfoReply(reply);
    });
}

void GoogleVeoProvider::onModelInfoReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 400 || httpStatus == 404) {
            setErrorDetails(i18n("Invalid API key or model name"), ProviderErrorKind::Configuration, httpStatus);
        } else if (httpStatus == 429) {
            setErrorDetails(i18n("Rate limited"), ProviderErrorKind::RateLimit, httpStatus,
                            retryAfterForReply(reply));
        } else {
            setNetworkError(reply, i18n("API error: %1 (HTTP %2)",
                                        reply->errorString(),
                                        QString::number(httpStatus)));
        }
        setLoading(false);
        setConnected(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        setErrorDetails(i18n("Unexpected API response format"), ProviderErrorKind::Schema);
        setLoading(false);
        setConnected(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();
        return;
    }

    // Reset limits first so partial headers cannot leave stale values from prior refreshes.
    setRateLimitRequests(0);
    setRateLimitRequestsRemaining(0);
    setRateLimitTokens(0);
    setRateLimitTokensRemaining(0);
    setRateLimitResetTime(QString());

    // Prefer provider-provided rate-limit headers when both request-limit and remaining are present.
    const bool hasRequestLimitHeader = !reply->rawHeader("x-ratelimit-limit-requests").isEmpty();
    const bool hasRequestRemainingHeader = !reply->rawHeader("x-ratelimit-remaining-requests").isEmpty();
    parseRateLimitHeaders(reply);
    Q_UNUSED(hasRequestLimitHeader);
    Q_UNUSED(hasRequestRemainingHeader);

    const ProviderBackend::NormalizedUsageCost normalized =
        ProviderBackend::normalizeUsageCost(ProviderBackend::ProviderId::GoogleVeo, doc.object());

    if (normalized.parsed) {
        setActualUsage(normalized.inputTokens,
                       normalized.outputTokens,
                       qMax(1, normalized.requestCount));
        setUsageSource(QStringLiteral("actual_api"));

        if (normalized.cost > 0.0) {
            setCost(normalized.cost);
            setCostSource(QStringLiteral("actual_api"));
            setDailyCost(normalized.dailyCost > 0.0 ? normalized.dailyCost : normalized.cost);
            setMonthlyCost(normalized.monthlyCost > 0.0 ? normalized.monthlyCost : normalized.cost);
            setDataQuality(QStringLiteral("actual_usage"));
        } else {
            const double durationSeconds = extractDurationSeconds(doc.object());
            if (durationSeconds > 0.0) {
                setEstimatedCost(durationSeconds * modelCostPerSecond(m_model));
                setDataQuality(QStringLiteral("estimated"));
            } else {
                clearProviderMetric(MetricKind::Cost, QStringLiteral("api_key"), QStringLiteral("current"));
                setCostSource(QStringLiteral("unknown"));
                setDailyCost(cost());
                setMonthlyCost(cost());
                setDataQuality(QStringLiteral("actual_usage"));
            }
        }
    } else {
        // Connectivity-only path for model-info endpoint.
        setProbeUsage(probeInputTokens(), probeOutputTokens(), probeRequestCount() + 1);
        setUsageSource(QStringLiteral("connectivity_probe"));
        clearProviderMetric(MetricKind::Cost, QStringLiteral("api_key"), QStringLiteral("current"));
        setCostSource(QStringLiteral("connectivity_probe"));
        setDataQuality(QStringLiteral("probe_only"));
        setDailyCost(0.0);
        setMonthlyCost(0.0);
    }

    setConnected(true);
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();
}
