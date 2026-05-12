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

void GoogleVeoProvider::refresh()
{
    if (!hasApiKey()) {
        setError(i18n("No API key configured"));
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
            setError(i18n("Invalid API key or model name"));
        } else if (httpStatus == 429) {
            setError(i18n("Rate limited"));
        } else {
            setError(i18n("API error: %1 (HTTP %2)",
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
        setError(i18n("Unexpected API response format"));
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
    if (!(hasRequestLimitHeader && hasRequestRemainingHeader) || rateLimitRequests() <= 0) {
        applyKnownLimits();
    }

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
                setCost(0.0);
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
        setCost(0.0);
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

void GoogleVeoProvider::applyKnownLimits()
{
    // Known rate limits for Veo models (as of 2025-2026)
    bool isPaid = (m_tier == QStringLiteral("paid"));

    if (m_model.contains(QStringLiteral("veo-2"))) {
        setRateLimitRequests(isPaid ? 200 : 10);
        setRateLimitRequestsRemaining(isPaid ? 200 : 10);
    } else {
        // veo-3 and all other Veo models share the same limits
        setRateLimitRequests(isPaid ? 100 : 5);
        setRateLimitRequestsRemaining(isPaid ? 100 : 5);
    }

    // Veo APIs are not token-centric in the same way text LLM APIs are.
    setRateLimitTokens(0);
    setRateLimitTokensRemaining(0);

    setRateLimitResetTime(isPaid ? QStringLiteral("N/A (paid tier limits)")
                                 : QStringLiteral("N/A (free tier limits)"));
}
