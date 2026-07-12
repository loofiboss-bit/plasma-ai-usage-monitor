#include "bedrockprovider.h"

#include "awssigv4signer.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

BedrockProvider::BedrockProvider(QObject *parent)
    : ProviderBackend(parent)
{
}

QString BedrockProvider::region() const
{
    return m_region;
}

void BedrockProvider::setRegion(const QString &region)
{
    const QString normalized = region.trimmed().toLower();
    if (m_region != normalized && !normalized.isEmpty()) {
        m_region = normalized;
        Q_EMIT regionChanged();
    }
}

QString BedrockProvider::model() const
{
    return m_model;
}

void BedrockProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model.trimmed();
        Q_EMIT modelChanged();
    }
}

QString BedrockProvider::accessKeyId() const
{
    return apiKey();
}

void BedrockProvider::setAccessKeyId(const QString &accessKeyId)
{
    setApiKey(accessKeyId.trimmed());
    Q_EMIT credentialsChanged();
}

QString BedrockProvider::secretAccessKey() const
{
    return m_secretAccessKey;
}

void BedrockProvider::setSecretAccessKey(const QString &secret)
{
    if (m_secretAccessKey != secret) {
        m_secretAccessKey = secret;
        Q_EMIT credentialsChanged();
    }
}

QString BedrockProvider::sessionToken() const
{
    return m_sessionToken;
}

void BedrockProvider::setSessionToken(const QString &token)
{
    if (m_sessionToken != token) {
        m_sessionToken = token;
        Q_EMIT credentialsChanged();
    }
}

void BedrockProvider::refreshImpl()
{
    if (m_region.isEmpty()) {
        setErrorDetails(i18n("AWS region missing (e.g., us-east-1)"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    if (!hasApiKey() || m_secretAccessKey.isEmpty()) {
        setErrorDetails(i18n("AWS access key ID or secret access key missing"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();

    const QString endpoint = endpointBase();
    QUrl url(QStringLiteral("%1/foundation-models").arg(endpoint));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("byOutputModality"), QStringLiteral("TEXT"));
    url.setQuery(query);

    QList<QPair<QByteArray, QByteArray>> headers = {
        {QByteArrayLiteral("accept"), QByteArrayLiteral("application/json")},
        {QByteArrayLiteral("host"), url.host().toUtf8()}
    };
    const auto signedHeaders = AwsSigV4Signer::sign(
        accessKeyId(),
        m_secretAccessKey,
        m_sessionToken,
        m_region,
        QStringLiteral("bedrock"),
        QStringLiteral("GET"),
        url.path(),
        url.query(QUrl::FullyEncoded),
        headers,
        QByteArray());

    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", signedHeaders.authorizationHeader);
    request.setRawHeader("X-Amz-Date", signedHeaders.amzDate);
    request.setRawHeader("X-Amz-Content-Sha256", signedHeaders.payloadHash);
    if (!m_sessionToken.isEmpty()) {
        request.setRawHeader("X-Amz-Security-Token", m_sessionToken.toUtf8());
    }

    const int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) {
            reply->deleteLater();
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 401 || httpStatus == 403) {
                setErrorDetails(i18n("AWS credentials rejected or missing IAM permissions for Bedrock foundation-models:List. Check region %1.", m_region),
                                httpStatus == 401 ? ProviderErrorKind::Authentication : ProviderErrorKind::Permission,
                                httpStatus);
            } else {
                setNetworkError(reply, i18n("Bedrock API error: %1 (HTTP %2)",
                                            reply->errorString(),
                                            QString::number(httpStatus)));
            }
            setConnected(false);
            setLoading(false);
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();

        if (doc.isNull() || !doc.isObject()) {
            setErrorDetails(i18n("Invalid Bedrock API response"), ProviderErrorKind::Schema);
            setConnected(false);
            setLoading(false);
            return;
        }

        const QJsonArray summaries = doc.object().value(QStringLiteral("modelSummaries")).toArray();
        bool matchedConfiguredModel = m_model.isEmpty();
        for (const QJsonValue &value : summaries) {
            const QJsonObject summary = value.toObject();
            if (summary.value(QStringLiteral("modelId")).toString() == m_model) {
                matchedConfiguredModel = true;
                break;
            }
        }

        setProbeUsage(probeInputTokens(), probeOutputTokens(), probeRequestCount() + 1);
        setUsageSource(QStringLiteral("connectivity_probe"));
        setCostSource(QStringLiteral("connectivity_probe"));
        setDataQuality(QStringLiteral("rate_limit_only"));
        setConnected(true);
        setLoading(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();

        if (!matchedConfiguredModel) {
            setErrorDetails(i18n("Configured model not returned by Bedrock in region %1", m_region),
                            ProviderErrorKind::Unsupported);
        } else {
            clearError();
        }
    });
}

QString BedrockProvider::endpointBase() const
{
    if (!customBaseUrl().trimmed().isEmpty()) {
        return customBaseUrl().trimmed();
    }
    return QStringLiteral("https://bedrock.%1.amazonaws.com").arg(m_region);
}
