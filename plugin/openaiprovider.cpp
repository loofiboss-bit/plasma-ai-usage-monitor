#include "openaiprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimeZone>
#include <QDebug>

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
} // namespace

OpenAIProvider::OpenAIProvider(QObject *parent)
    : ProviderBackend(parent)
{
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
    setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("pending"));
    setCapabilityStatus(QStringLiteral("daily_billing"), QStringLiteral("pending"));
    setCapabilityStatus(QStringLiteral("monthly_billing"), QStringLiteral("pending"));

    fetchUsage();
    fetchCosts();
    fetchMonthlyCosts();
}

void OpenAIProvider::fetchUsage()
{
    // Query the last 24 hours of completion usage
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime dayAgo = now.addDays(-1);

    QUrl url(QStringLiteral("%1/organization/usage/completions").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(dayAgo.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));

    // Add filters
    if (!m_model.isEmpty()) {
        query.addQueryItem(QStringLiteral("models"), m_model);
    }
    if (!m_projectId.isEmpty()) {
        query.addQueryItem(QStringLiteral("project_ids"), m_projectId);
    }

    url.setQuery(query);

    QNetworkRequest request = createRequest(url);

    m_pendingRequests++;
    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onUsageReply(reply);
    });
}

void OpenAIProvider::fetchCosts()
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime dayAgo = now.addDays(-1);

    QUrl url(QStringLiteral("%1/organization/costs").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(dayAgo.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));

    if (!m_projectId.isEmpty()) {
        query.addQueryItem(QStringLiteral("project_ids"), m_projectId);
    }

    url.setQuery(query);

    QNetworkRequest request = createRequest(url);

    m_pendingRequests++;
    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onCostsReply(reply);
    });
}

void OpenAIProvider::onUsageReply(QNetworkReply *reply)
{
    m_pendingRequests--;

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Retry transient errors (retryRequest takes ownership of reply)
        if (isRetryableStatus(httpStatus)) {
            m_pendingRequests++; // re-increment since retry is pending
            retryRequest(reply, reply->url(), QByteArray(),
                         [this](QNetworkReply *r) { onUsageReply(r); });
            return;
        }

        if (httpStatus == 401 || httpStatus == 403) {
            setErrorDetails(i18n("Authentication failed. Ensure you're using an Admin API key."),
                            httpStatus == 401 ? ProviderErrorKind::Authentication : ProviderErrorKind::Permission,
                            httpStatus);
        } else {
            setNetworkError(reply, i18n("Usage API error: %1 (HTTP %2)",
                                        reply->errorString(),
                                        QString::number(httpStatus)));
        }
        setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("failed"),
                            i18n("Usage data is unavailable"));
        reply->deleteLater();
        checkAllDone();
        return;
    }

    reply->deleteLater();

    // Parse rate limit headers from the response
    parseRateLimitHeaders(reply);

    // Parse JSON body
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        setErrorDetails(i18n("Failed to parse usage response"), ProviderErrorKind::Schema);
        setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("failed"),
                            i18n("Usage response schema is invalid"));
        checkAllDone();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray buckets = root.value(QStringLiteral("data")).toArray();

    qint64 totalInput = 0;
    qint64 totalOutput = 0;
    int totalRequests = 0;

    for (const QJsonValue &bucket : buckets) {
        QJsonArray results = bucketResults(bucket.toObject());
        for (const QJsonValue &result : results) {
            QJsonObject r = result.toObject();
            totalInput += r.value(QStringLiteral("input_tokens")).toInteger(0);
            totalOutput += r.value(QStringLiteral("output_tokens")).toInteger(0);
            totalRequests += r.value(QStringLiteral("num_model_requests")).toInt(0);
        }
    }

    setActualUsage(totalInput, totalOutput, totalRequests);
    setUsageSource(QStringLiteral("actual_api"));
    setDataQuality(QStringLiteral("actual_usage"));
    setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("available"));
    setConnected(true);

    checkAllDone();
}

void OpenAIProvider::onCostsReply(QNetworkReply *reply)
{
    m_pendingRequests--;

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Retry transient errors (retryRequest takes ownership of reply)
        if (isRetryableStatus(httpStatus)) {
            m_pendingRequests++; // re-increment since retry is pending
            retryRequest(reply, reply->url(), QByteArray(),
                         [this](QNetworkReply *r) { onCostsReply(r); });
            return;
        }

        // Non-fatal: usage data may still be available
        setNetworkError(reply, i18n("Costs API unavailable: %1", reply->errorString()));
        setCapabilityStatus(QStringLiteral("daily_billing"), QStringLiteral("failed"),
                            i18n("Daily billing data is unavailable"));
        reply->deleteLater();
        checkAllDone();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        setErrorDetails(i18n("Failed to parse costs response"), ProviderErrorKind::Schema);
        setCapabilityStatus(QStringLiteral("daily_billing"), QStringLiteral("failed"),
                            i18n("Daily billing response schema is invalid"));
        checkAllDone();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray buckets = root.value(QStringLiteral("data")).toArray();

    double totalCost = 0.0;
    for (const QJsonValue &bucket : buckets) {
        QJsonArray results = bucketResults(bucket.toObject());
        for (const QJsonValue &result : results) {
            QJsonObject r = result.toObject();
            totalCost += parseOpenAiCostAmountUsd(r);
        }
    }

    setCost(totalCost);
    setCostSource(QStringLiteral("billing_api"));
    setCurrency(QStringLiteral("USD"));
    setDataQuality(QStringLiteral("actual_billing"));
    setDailyCost(totalCost); // 24h window = daily cost
    setCapabilityStatus(QStringLiteral("daily_billing"), QStringLiteral("available"));
    checkAllDone();
}

void OpenAIProvider::fetchMonthlyCosts()
{
    // Query costs from the start of the current month
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDate today = now.date();
    QDate monthStart(today.year(), today.month(), 1);
    QDateTime monthStartDt(monthStart.startOfDay(QTimeZone::UTC));

    QUrl url(QStringLiteral("%1/organization/costs").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(monthStartDt.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));

    if (!m_projectId.isEmpty()) {
        query.addQueryItem(QStringLiteral("project_ids"), m_projectId);
    }

    url.setQuery(query);

    QNetworkRequest request = createRequest(url);

    m_pendingRequests++;
    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onMonthlyCostsReply(reply);
    });
}

void OpenAIProvider::onMonthlyCostsReply(QNetworkReply *reply)
{
    m_pendingRequests--;

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Retry transient errors (retryRequest takes ownership of reply)
        if (isRetryableStatus(httpStatus)) {
            m_pendingRequests++; // re-increment since retry is pending
            retryRequest(reply, reply->url(), QByteArray(),
                         [this](QNetworkReply *r) { onMonthlyCostsReply(r); });
            return;
        }

        // Non-fatal: daily cost data may still be available
        setNetworkError(reply, i18n("Monthly costs API unavailable: %1", reply->errorString()));
        setCapabilityStatus(QStringLiteral("monthly_billing"), QStringLiteral("failed"),
                            i18n("Monthly billing data is unavailable"));
        reply->deleteLater();
        checkAllDone();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        setErrorDetails(i18n("Failed to parse monthly costs response"), ProviderErrorKind::Schema);
        setCapabilityStatus(QStringLiteral("monthly_billing"), QStringLiteral("failed"),
                            i18n("Monthly billing response schema is invalid"));
        checkAllDone();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray buckets = root.value(QStringLiteral("data")).toArray();

    double totalCost = 0.0;
    for (const QJsonValue &bucket : buckets) {
        QJsonArray results = bucketResults(bucket.toObject());
        for (const QJsonValue &result : results) {
            QJsonObject r = result.toObject();
            totalCost += parseOpenAiCostAmountUsd(r);
        }
    }

    setMonthlyCost(totalCost);
    setCostSource(QStringLiteral("billing_api"));
    setCurrency(QStringLiteral("USD"));
    setDataQuality(QStringLiteral("actual_billing"));
    setCapabilityStatus(QStringLiteral("monthly_billing"), QStringLiteral("available"));
    checkAllDone();
}

void OpenAIProvider::checkAllDone()
{
    if (m_pendingRequests <= 0) {
        setLoading(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();

    }
}
