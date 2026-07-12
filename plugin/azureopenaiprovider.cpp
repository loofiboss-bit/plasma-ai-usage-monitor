#include "azureopenaiprovider.h"

#include <KLocalizedString>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>

AzureOpenAIProvider::AzureOpenAIProvider(QObject *parent)
    : ProviderBackend(parent)
{
    registerCatalogPricing(QStringLiteral("azure"));
}

QString AzureOpenAIProvider::model() const
{
    return m_model;
}

void AzureOpenAIProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        Q_EMIT modelChanged();
    }
}

QString AzureOpenAIProvider::deploymentId() const
{
    return m_deploymentId;
}

void AzureOpenAIProvider::setDeploymentId(const QString &deploymentId)
{
    if (m_deploymentId != deploymentId) {
        m_deploymentId = deploymentId.trimmed();
        Q_EMIT deploymentIdChanged();
    }
}

QString AzureOpenAIProvider::apiVersion() const
{
    return m_apiVersion;
}

void AzureOpenAIProvider::setApiVersion(const QString &apiVersion)
{
    const QString trimmed = apiVersion.trimmed();
    if (!trimmed.isEmpty() && m_apiVersion != trimmed) {
        m_apiVersion = trimmed;
        Q_EMIT apiVersionChanged();
    }
}

QString AzureOpenAIProvider::endpointBaseUrl() const
{
    if (qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO")) {
        QString demoUrl = QString::fromLocal8Bit(qgetenv("PLASMA_AI_MONITOR_DEMO_BASE_URL")).trimmed();
        if (demoUrl.isEmpty()) {
            demoUrl = QStringLiteral("http://localhost:8080");
        }
        while (demoUrl.endsWith(QLatin1Char('/'))) {
            demoUrl.chop(1);
        }
        return demoUrl;
    }

    QString endpoint = customBaseUrl().trimmed();
    while (endpoint.endsWith(QLatin1Char('/'))) {
        endpoint.chop(1);
    }
    return endpoint;
}

QUrl AzureOpenAIProvider::completionUrl() const
{
    const QString endpoint = endpointBaseUrl();
    const QString encodedDeployment = QString::fromUtf8(QUrl::toPercentEncoding(m_deploymentId));
    QUrl url(QStringLiteral("%1/openai/deployments/%2/chat/completions")
             .arg(endpoint, encodedDeployment));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api-version"), m_apiVersion);
    url.setQuery(query);

    return url;
}

void AzureOpenAIProvider::refreshImpl()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    if (endpointBaseUrl().isEmpty()) {
        setErrorDetails(i18n("No Azure endpoint configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    if (m_deploymentId.isEmpty()) {
        setErrorDetails(i18n("No Azure deployment ID configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();

    QUrl url = completionUrl();
    QNetworkRequest request(url);
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("api-key", apiKey().toUtf8());

    QJsonObject payload;
    payload.insert(QStringLiteral("max_tokens"), 1);
    payload.insert(QStringLiteral("temperature"), 0);

    if (!m_model.isEmpty()) {
        payload.insert(QStringLiteral("model"), m_model);
    }

    QJsonArray messages;
    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));
    message.insert(QStringLiteral("content"), QStringLiteral("hi"));
    messages.append(message);
    payload.insert(QStringLiteral("messages"), messages);

    m_lastRequestBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->post(request, m_lastRequestBody);
    trackReply(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) {
            reply->deleteLater();
            return;
        }
        onCompletionReply(reply);
    });
}

void AzureOpenAIProvider::onCompletionReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (isRetryableStatus(httpStatus)) {
            retryRequest(reply, reply->url(), m_lastRequestBody,
                         [this](QNetworkReply *r) { onCompletionReply(r); });
            return;
        }

        if (httpStatus == 401 || httpStatus == 403) {
            setErrorDetails(i18n("Authentication failed. Check Azure API key and endpoint."),
                            httpStatus == 401 ? ProviderErrorKind::Authentication : ProviderErrorKind::Permission,
                            httpStatus);
        } else if (httpStatus == 404) {
            setErrorDetails(i18n("Azure deployment not found. Check deployment ID and endpoint."),
                            ProviderErrorKind::Configuration, httpStatus);
        } else {
            setNetworkError(reply, i18n("Azure OpenAI API error: %1 (HTTP %2)",
                                        reply->errorString(),
                                        QString::number(httpStatus)));
        }

        reply->deleteLater();
        setLoading(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();
        return;
    }

    parseRateLimitHeaders(reply);

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        setErrorDetails(i18n("Failed to parse Azure OpenAI response"), ProviderErrorKind::Schema);
        setConnected(false);
        setLoading(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();
        return;
    }

    const QJsonObject root = doc.object();
    const ProviderBackend::NormalizedUsageCost normalized =
        ProviderBackend::normalizeUsageCost(ProviderBackend::ProviderId::AzureOpenAI, root);

    if (normalized.parsed) {
        m_sessionInputTokens += normalized.inputTokens;
        m_sessionOutputTokens += normalized.outputTokens;
        m_sessionRequestCount += qMax(1, normalized.requestCount);
    } else {
        const QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
        const qint64 promptTokens = usage.value(QStringLiteral("prompt_tokens")).toInteger(0);
        const qint64 completionTokens = usage.value(QStringLiteral("completion_tokens")).toInteger(0);
        const qint64 totalTokens = usage.value(QStringLiteral("total_tokens")).toInteger(promptTokens + completionTokens);

        m_sessionInputTokens += promptTokens;
        if (completionTokens > 0) {
            m_sessionOutputTokens += completionTokens;
        } else {
            m_sessionOutputTokens += qMax<qint64>(0, totalTokens - promptTokens);
        }
        m_sessionRequestCount += 1;
    }

    setProbeUsage(m_sessionInputTokens, m_sessionOutputTokens, m_sessionRequestCount);
    setUsageSource(QStringLiteral("connectivity_probe"));
    setCost(0.0);
    setCostSource(QStringLiteral("connectivity_probe"));
    setDataQuality(QStringLiteral("probe_only"));
    setDailyCost(0.0);
    setMonthlyCost(0.0);

    setConnected(true);
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();
}
