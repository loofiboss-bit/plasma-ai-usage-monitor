#include "openrouterprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QDebug>

OpenRouterProvider::OpenRouterProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("openai/gpt-5.4-pro"));
    registerCatalogPricing(QStringLiteral("openrouter"));
}

double OpenRouterProvider::credits() const { return m_credits; }

void OpenRouterProvider::refreshImpl()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    // Call parent's refresh (chat completion for rate limits + usage)
    OpenAICompatibleProvider::refreshImpl();

    // Additionally fetch credits balance
    fetchCredits();
}

void OpenRouterProvider::fetchCredits()
{
    // OpenRouter credits endpoint: GET /api/v1/auth/key
    QUrl url(QStringLiteral("%1/auth/key").arg(effectiveBaseUrl(defaultBaseUrl())));

    QNetworkRequest request = createRequest(url);

    addPendingRequest();
    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onCreditsReply(reply);
    });
}

void OpenRouterProvider::onCreditsReply(QNetworkReply *reply)
{
    reply->deleteLater();
    decrementPendingRequest();

    if (reply->error() != QNetworkReply::NoError) {
        // Non-fatal: rate limit data may still be available
        setNetworkError(reply, i18n("Credits API unavailable: %1", reply->errorString()));
        onAllRequestsDone();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        QJsonObject root = doc.object();
        // OpenRouter response: { "data": { "label": "...", "usage": 0.5, "limit": 10.0, ... } }
        QJsonObject dataObj = root.value(QStringLiteral("data")).toObject();
        double usage = dataObj.value(QStringLiteral("usage")).toDouble();
        double limit = dataObj.value(QStringLiteral("limit")).toDouble();
        // Credits remaining = limit - usage (if limit > 0)
        m_credits = (limit > 0) ? (limit - usage) : 0.0;
        Q_EMIT creditsChanged();
    }

    onAllRequestsDone();
}
