#include "googleprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

GoogleProvider::GoogleProvider(QObject *parent)
    : ProviderBackend(parent)
{
    registerCatalogPricing(QStringLiteral("google"));
}

QString GoogleProvider::model() const { return m_model; }
void GoogleProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        Q_EMIT modelChanged();
    }
}

QString GoogleProvider::tier() const { return m_tier; }
void GoogleProvider::setTier(const QString &tier)
{
    if (m_tier != tier) {
        m_tier = tier;
        Q_EMIT tierChanged();
    }
}

void GoogleProvider::refresh()
{
    if (!hasApiKey()) {
        setError(i18n("No API key configured"));
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();
    fetchStatus();
}

void GoogleProvider::fetchStatus()
{
    // Use countTokens as a lightweight connectivity check
    QUrl url(QStringLiteral("%1/models/%2:countTokens")
                 .arg(effectiveBaseUrl(BASE_URL), m_model));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), apiKey());
    url.setQuery(query);

    // Use createRequest for timeout, then clear Bearer auth (Google uses query-param auth)
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", QByteArray());

    // Minimal payload
    QJsonObject payload;
    QJsonArray contents;
    QJsonObject content;
    QJsonArray parts;
    QJsonObject part;
    part.insert(QStringLiteral("text"), QStringLiteral("hi"));
    parts.append(part);
    content.insert(QStringLiteral("parts"), parts);
    contents.append(content);
    payload.insert(QStringLiteral("contents"), contents);

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->post(request, body);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onCountTokensReply(reply);
    });
}

void GoogleProvider::onCountTokensReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 400) {
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

    // Parse response for basic verification
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        QJsonObject root = doc.object();
        // The countTokens response has totalTokens -- we don't use it,
        // but a successful response confirms the key works.
        int totalTokens = root.value(QStringLiteral("totalTokens")).toInt(0);
        Q_UNUSED(totalTokens);
    }

    // Google doesn't expose rate limit headers on the Gemini API,
    // so we apply known documentation limits.
    applyKnownLimits();

    setConnected(true);
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();
}

void GoogleProvider::applyKnownLimits()
{
    // Known rate limits for Gemini API (as of 2025)
    // Limits depend on model and pricing tier.
    bool isPaid = (m_tier == QStringLiteral("paid"));

    if (m_model.contains(QStringLiteral("flash"))) {
        if (isPaid) {
            setRateLimitRequests(2000);  // RPM for paid tier
            setRateLimitRequestsRemaining(2000);
            setRateLimitTokens(4000000); // 4M TPM
            setRateLimitTokensRemaining(4000000);
        } else {
            setRateLimitRequests(15);  // RPM for free tier
            setRateLimitRequestsRemaining(15);
            setRateLimitTokens(1000000); // 1M TPM
            setRateLimitTokensRemaining(1000000);
        }
    } else if (m_model.contains(QStringLiteral("pro"))) {
        if (isPaid) {
            setRateLimitRequests(1000);
            setRateLimitRequestsRemaining(1000);
            setRateLimitTokens(4000000);
            setRateLimitTokensRemaining(4000000);
        } else {
            setRateLimitRequests(2);
            setRateLimitRequestsRemaining(2);
            setRateLimitTokens(32000);
            setRateLimitTokensRemaining(32000);
        }
    } else {
        // Generic defaults
        if (isPaid) {
            setRateLimitRequests(1000);
            setRateLimitRequestsRemaining(1000);
            setRateLimitTokens(4000000);
            setRateLimitTokensRemaining(4000000);
        } else {
            setRateLimitRequests(15);
            setRateLimitRequestsRemaining(15);
            setRateLimitTokens(1000000);
            setRateLimitTokensRemaining(1000000);
        }
    }

    setRateLimitResetTime(isPaid ? QStringLiteral("N/A (paid tier limits)")
                                 : QStringLiteral("N/A (free tier limits)"));
}
