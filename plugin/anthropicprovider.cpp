#include "anthropicprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QDebug>

AnthropicProvider::AnthropicProvider(QObject *parent)
    : ProviderBackend(parent)
{
    registerCatalogPricing(QStringLiteral("anthropic"));
}

QString AnthropicProvider::model() const { return m_model; }
void AnthropicProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        Q_EMIT modelChanged();
    }
}

void AnthropicProvider::refresh()
{
    if (!hasApiKey()) {
        setError(i18n("No API key configured"));
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();
    fetchRateLimits();
}

void AnthropicProvider::fetchRateLimits()
{
    // Use the count_tokens endpoint as a lightweight way to get rate limit headers.
    // This is a minimal request that counts tokens for a tiny message.
    QUrl url(QStringLiteral("%1/messages/count_tokens").arg(effectiveBaseUrl(BASE_URL)));

    QNetworkRequest request = createRequest(url);
    // Anthropic uses x-api-key instead of Bearer auth
    request.setRawHeader("Authorization", QByteArray()); // clear default Bearer
    request.setRawHeader("x-api-key", apiKey().toUtf8());
    request.setRawHeader("anthropic-version", API_VERSION);

    // Minimal payload for count_tokens
    QJsonObject payload;
    payload.insert(QStringLiteral("model"), m_model);

    QJsonArray messages;
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("user"));
    msg.insert(QStringLiteral("content"), QStringLiteral("hi"));
    messages.append(msg);
    payload.insert(QStringLiteral("messages"), messages);

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->post(request, body);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onCountTokensReply(reply);
    });
}

void AnthropicProvider::onCountTokensReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 401) {
            setError(i18n("Invalid API key"));
        } else if (httpStatus == 429) {
            setError(i18n("Rate limited"));
            // Still parse headers -- they're returned on 429 too
        } else {
            setError(i18n("API error: %1 (HTTP %2)",
                         reply->errorString(),
                         QString::number(httpStatus)));
            setLoading(false);
            return;
        }
    }

    // Parse Anthropic's detailed rate limit headers
    auto readHeader = [&](const char *name) -> int {
        QByteArray val = reply->rawHeader(name);
        return val.isEmpty() ? 0 : val.toInt();
    };

    // Request limits — use centralized parser for request limits
    int reqLimit = readHeader("anthropic-ratelimit-requests-limit");
    int reqRemaining = readHeader("anthropic-ratelimit-requests-remaining");
    QString reqReset = QString::fromUtf8(reply->rawHeader("anthropic-ratelimit-requests-reset"));

    if (reqLimit > 0) {
        setRateLimitRequests(reqLimit);
        setRateLimitRequestsRemaining(reqRemaining);
    }

    // Input + output token limits (combined for display)
    int inputLimit = readHeader("anthropic-ratelimit-input-tokens-limit");
    int inputRemaining = readHeader("anthropic-ratelimit-input-tokens-remaining");
    int outputLimit = readHeader("anthropic-ratelimit-output-tokens-limit");
    int outputRemaining = readHeader("anthropic-ratelimit-output-tokens-remaining");
    int tokenLimit = inputLimit + outputLimit;
    int tokenRemaining = inputRemaining + outputRemaining;
    if (tokenLimit > 0) {
        setRateLimitTokens(tokenLimit);
        setRateLimitTokensRemaining(tokenRemaining);
    }

    if (!reqReset.isEmpty()) {
        // Parse RFC 3339 timestamp to a readable time
        QDateTime resetDt = QDateTime::fromString(reqReset, Qt::ISODate);
        if (resetDt.isValid()) {
            setRateLimitResetTime(resetDt.toLocalTime().toString(QStringLiteral("hh:mm:ss")));
        } else {
            setRateLimitResetTime(reqReset);
        }
    }

    setConnected(true);
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();

    // Quota warning check
    if (reqLimit > 0) {
        int usedPercent = 100 - (reqRemaining * 100 / reqLimit);
        if (usedPercent >= 80) {
            Q_EMIT quotaWarning(name(), usedPercent);
        }
    }
}
