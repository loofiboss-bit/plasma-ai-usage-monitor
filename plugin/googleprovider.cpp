#include "googleprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>
#include <QSettings>
#include "providerpricingcatalog.h"

GoogleProvider::GoogleProvider(QObject *parent)
    : ProviderBackend(parent)
{
    registerCatalogPricing(QStringLiteral("google"));
    setPricingModel(m_model);
    loadDiscoveryCache();
}

QString GoogleProvider::model() const { return m_model; }
void GoogleProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        setPricingModel(m_model);
        Q_EMIT modelChanged();
        Q_EMIT discoveredModelsChanged();
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

void GoogleProvider::refreshImpl()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();
    if (!m_discoveredModels.isEmpty() && selectedModelAvailable() && m_modelsLastDiscovered.isValid()
        && m_modelsLastDiscovered.secsTo(QDateTime::currentDateTimeUtc()) < 24 * 60 * 60) {
        setConnected(true);
        setUsageSource(QStringLiteral("model_discovery_api"));
        setDataQuality(QStringLiteral("connectivity_only"));
        setLoading(false);
        updateLastRefreshed();
        Q_EMIT dataUpdated();
        return;
    }
    fetchModels();
}

bool GoogleProvider::selectedModelAvailable() const
{
    for (const QVariant &entry : m_discoveredModels) {
        if (entry.toMap().value(QStringLiteral("id")).toString() == m_model) return true;
    }
    return false;
}

QString GoogleProvider::selectedModelWarning() const
{
    for (const QVariant &entry : m_discoveredModels) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("id")).toString() != m_model) continue;
        const QString status = row.value(QStringLiteral("lifecycle")).toMap()
                                   .value(QStringLiteral("status")).toString();
        if (status == QLatin1String("deprecated")) return i18n("The pinned model is deprecated");
        if (m_model.contains(QLatin1String("preview"), Qt::CaseInsensitive))
            return i18n("The pinned model is a preview model and may change");
        return {};
    }
    return m_model.isEmpty() ? QString() : i18n("The pinned model was not returned by live discovery");
}

void GoogleProvider::refreshModelsNow()
{
    m_modelsLastDiscovered = QDateTime();
    refresh();
}

void GoogleProvider::fetchModels(const QString &pageToken)
{
    if (pageToken.isEmpty()) {
        m_pendingLiveModels.clear();
        m_seenModelIds.clear();
        m_discoveryPageCount = 0;
    }
    if (++m_discoveryPageCount > 20) {
        setErrorDetails(i18n("Gemini model discovery exceeded the pagination budget"), ProviderErrorKind::Schema);
        setLoading(false);
        setConnected(false);
        return;
    }
    QUrl url(QStringLiteral("%1/models").arg(effectiveBaseUrl(BASE_URL)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), apiKey());
    query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("1000"));
    if (!pageToken.isEmpty()) query.addQueryItem(QStringLiteral("pageToken"), pageToken);
    url.setQuery(query);
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", QByteArray());
    const int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onModelsReply(reply);
    });
}

void GoogleProvider::countTokensDiagnostic()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        return;
    }
    beginRefresh();
    setLoading(true);
    clearError();
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

void GoogleProvider::onModelsReply(QNetworkReply *reply)
{
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (httpStatus == 401 || httpStatus == 403) {
            setErrorDetails(i18n("Gemini Developer API authentication failed"), ProviderErrorKind::Authentication, httpStatus);
        } else if (httpStatus == 429) {
            setErrorDetails(i18n("Rate limited"), ProviderErrorKind::RateLimit, httpStatus, retryAfterForReply(reply));
        } else {
            setNetworkError(reply, i18n("Gemini models API unavailable: %1", reply->errorString()));
        }
        setConnected(false);
    } else {
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            setErrorDetails(i18n("Unexpected Gemini models response"), ProviderErrorKind::Schema);
            setConnected(false);
        } else {
            const QJsonArray models = doc.object().value(QStringLiteral("models")).toArray();
            for (const QJsonValue &entry : models) {
                const QJsonObject object = entry.toObject();
                QVariantMap row = object.toVariantMap();
                QString id = object.value(QStringLiteral("name")).toString();
                id.remove(QStringLiteral("models/"));
                row.insert(QStringLiteral("id"), id);
                if (id.isEmpty() || m_seenModelIds.contains(id)) continue;
                m_seenModelIds.append(id);
                row.insert(QStringLiteral("discoverySource"), QStringLiteral("live_api"));
                m_pendingLiveModels.append(row);
            }
            const QString nextPageToken = doc.object().value(QStringLiteral("nextPageToken")).toString();
            if (!nextPageToken.isEmpty()) {
                reply->deleteLater();
                fetchModels(nextPageToken);
                return;
            }
            finalizeModelDiscovery();
        }
    }
    reply->deleteLater();
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();
}

void GoogleProvider::finalizeModelDiscovery()
{
    QVariantList merged = m_pendingLiveModels;
    const QVariantList fallback = ProviderPricingCatalog::instance()->selectableModelsForProvider(QStringLiteral("google"));
    for (const QVariant &entry : fallback) {
        QVariantMap row = entry.toMap();
        const QString id = row.value(QStringLiteral("id")).toString();
        if (m_seenModelIds.contains(id)) continue;
        row.insert(QStringLiteral("discoverySource"), QStringLiteral("shipped_catalog"));
        merged.append(row);
    }
    m_discoveredModels = merged;
    m_modelsLastDiscovered = QDateTime::currentDateTimeUtc();
    saveDiscoveryCache();
    Q_EMIT discoveredModelsChanged();
    setConnected(true);
    setUsageSource(QStringLiteral("model_discovery_api"));
    setCostSource(QStringLiteral("unknown"));
    setDataQuality(QStringLiteral("connectivity_only"));
    setCapabilityStatus(QStringLiteral("model_discovery"), QStringLiteral("available"));
}

void GoogleProvider::loadDiscoveryCache()
{
    QSettings settings(QStringLiteral("loofi"), QStringLiteral("plasma-ai-usage-monitor"));
    settings.beginGroup(QStringLiteral("providerDiscovery/google"));
    m_modelsLastDiscovered = settings.value(QStringLiteral("observedAt")).toDateTime();
    const QJsonDocument cached = QJsonDocument::fromJson(settings.value(QStringLiteral("models")).toByteArray());
    if (cached.isArray()) m_discoveredModels = cached.array().toVariantList();
    settings.endGroup();
    if (m_discoveredModels.isEmpty()) {
        m_discoveredModels = ProviderPricingCatalog::instance()->selectableModelsForProvider(QStringLiteral("google"));
        for (QVariant &entry : m_discoveredModels) {
            QVariantMap row = entry.toMap();
            row.insert(QStringLiteral("discoverySource"), QStringLiteral("shipped_catalog"));
            entry = row;
        }
    }
}

void GoogleProvider::saveDiscoveryCache() const
{
    QSettings settings(QStringLiteral("loofi"), QStringLiteral("plasma-ai-usage-monitor"));
    settings.beginGroup(QStringLiteral("providerDiscovery/google"));
    settings.setValue(QStringLiteral("observedAt"), m_modelsLastDiscovered);
    settings.setValue(QStringLiteral("models"), QJsonDocument(QJsonArray::fromVariantList(m_discoveredModels)).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

void GoogleProvider::onCountTokensReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 400) {
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

    setConnected(true);
    setUsageSource(QStringLiteral("connectivity_probe"));
    setCostSource(QStringLiteral("connectivity_probe"));
    setDataQuality(QStringLiteral("rate_limit_only"));
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();
}
