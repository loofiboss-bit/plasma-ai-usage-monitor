#include "descriptorprovider.h"

#include <KLocalizedString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QMap>
#include <QSet>
#include <QUrlQuery>

DescriptorProvider::DescriptorProvider(const QVariantMap &descriptor, QObject *parent)
    : ProviderBackend(parent), m_descriptor(descriptor)
{
    const QVariantList models = descriptor.value(QStringLiteral("models")).toList();
    if (!models.isEmpty()) m_model = models.first().toMap().value(QStringLiteral("id")).toString();
    for (const QVariant &model : models) {
        QVariantMap row = model.toMap();
        row.insert(QStringLiteral("discoverySource"), QStringLiteral("shipped_catalog"));
        m_discoveredModels.append(row);
    }
}

QString DescriptorProvider::name() const { return m_descriptor.value(QStringLiteral("displayName")).toString(); }
QString DescriptorProvider::iconName() const { return m_descriptor.value(QStringLiteral("icon")).toString(); }
QString DescriptorProvider::providerId() const { return m_descriptor.value(QStringLiteral("stableId")).toString(); }
QString DescriptorProvider::model() const { return m_model; }
QString DescriptorProvider::monitoringLevel() const { return m_descriptor.value(QStringLiteral("monitoringLevel")).toString(); }
bool DescriptorProvider::manualProbeOnly() const { return m_descriptor.value(QStringLiteral("probePolicy")).toString() == QLatin1String("manual_only"); }
bool DescriptorProvider::selectedModelAvailable() const
{
    for (const QVariant &entry : m_discoveredModels) {
        if (entry.toMap().value(QStringLiteral("id")).toString() == m_model) return true;
    }
    return false;
}

void DescriptorProvider::setModel(const QString &model)
{
    if (m_model == model) return;
    m_model = model;
    Q_EMIT modelChanged();
}

void DescriptorProvider::refreshImpl()
{
    if (manualProbeOnly()) {
        setErrorDetails(i18n("No scheduled request: this provider requires a manual connection test"),
                        ProviderErrorKind::Unsupported);
        setConnected(false);
        return;
    }
    const QString auth = m_descriptor.value(QStringLiteral("auth")).toMap().value(QStringLiteral("scheme")).toString();
    if (auth != QLatin1String("none") && !hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    const QVariantMap endpoint = m_descriptor.value(QStringLiteral("endpoint")).toMap();
    const QVariantMap safeRefresh = m_descriptor.value(QStringLiteral("safeRefresh")).toMap();
    QString base = customBaseUrl().trimmed();
    if (base.isEmpty()) base = endpoint.value(QStringLiteral("default")).toString();
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    const QString path = safeRefresh.value(QStringLiteral("path")).toString();
    if (base.isEmpty() || path.isEmpty()) {
        setErrorDetails(i18n("No safe read-only endpoint is configured"), ProviderErrorKind::Unsupported);
        return;
    }

    beginRefresh();
    setLoading(true);
    clearError();
    m_pendingDiscoveredModels.clear();
    m_pendingModelIds.clear();
    m_pageRequests = 0;
    requestPage(QUrl(base + path), currentGeneration());
}

void DescriptorProvider::requestPage(const QUrl &url, int generation)
{
    const QString auth = m_descriptor.value(QStringLiteral("auth")).toMap().value(QStringLiteral("scheme")).toString();
    QNetworkRequest request = createRequest(url);
    if (auth == QLatin1String("api-key")) {
        request.setRawHeader("Authorization", QByteArray());
        request.setRawHeader("x-api-key", apiKey().toUtf8());
    }
    else if (auth != QLatin1String("none")) request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey().toUtf8());
    else request.setRawHeader("Authorization", QByteArray());

    ++m_pageRequests;
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        if (!isCurrentGeneration(generation)) { reply->deleteLater(); return; }
        handleReply(reply);
    });
}

void DescriptorProvider::finalizeModelDiscovery(bool partial, const QString &reason)
{
    for (const QVariant &fallback : m_descriptor.value(QStringLiteral("models")).toList()) {
        QVariantMap row = fallback.toMap();
        const QString id = row.value(QStringLiteral("id")).toString();
        if (m_pendingModelIds.contains(id)) continue;
        m_pendingModelIds.insert(id);
        row.insert(QStringLiteral("discoverySource"), QStringLiteral("shipped_catalog"));
        m_pendingDiscoveredModels.append(row);
    }
    m_discoveredModels = m_pendingDiscoveredModels;
    m_modelsLastDiscovered = QDateTime::currentDateTimeUtc();
    Q_EMIT discoveredModelsChanged();
    setUsageSource(QStringLiteral("model_discovery_api"));
    setCostSource(QStringLiteral("unknown"));
    setDataQuality(partial ? QStringLiteral("connectivity_partial") : QStringLiteral("connectivity_only"));
    setCapabilityStatus(QStringLiteral("model_discovery"), partial ? QStringLiteral("partial") : QStringLiteral("available"), reason);
}

void DescriptorProvider::handleReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (status == 401) setErrorDetails(i18n("Invalid API key"), ProviderErrorKind::Authentication, status);
        else if (status == 403) setErrorDetails(i18n("Insufficient read-only permissions"), ProviderErrorKind::Permission, status);
        else if (status == 429) setErrorDetails(i18n("Rate limited"), ProviderErrorKind::RateLimit, status, retryAfterForReply(reply));
        else setNetworkError(reply, i18n("Provider refresh failed: %1", reply->errorString()));
        setConnected(false);
        setCapabilityStatus(QStringLiteral("scheduled_refresh"), QStringLiteral("failed"),
                            i18n("The read-only provider endpoint is unavailable"));
    } else {
        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (!document.isObject() && !document.isArray()) {
            setErrorDetails(i18n("Unexpected provider response"), ProviderErrorKind::Schema);
            setConnected(false);
        } else {
            QJsonObject root = document.object();
            const QString profile = m_descriptor.value(QStringLiteral("adapterType")).toString();
            if (profile == QLatin1String("gateway_usage")) {
                const auto numericWhenPresent = [](const QJsonObject &object, const QString &key) {
                    return !object.contains(key) || object.value(key).isDouble();
                };
                bool validGatewayShape = document.isArray();
                if (document.isObject()) {
                    validGatewayShape = root.contains(QStringLiteral("summary"))
                        || root.contains(QStringLiteral("spend"))
                        || root.contains(QStringLiteral("spend_by_currency"))
                        || root.contains(QStringLiteral("prompt_tokens"))
                        || root.contains(QStringLiteral("completion_tokens"))
                        || root.contains(QStringLiteral("requests"));
                    const QJsonObject summary = root.value(QStringLiteral("summary")).toObject();
                    validGatewayShape = validGatewayShape
                        && (!root.contains(QStringLiteral("summary")) || root.value(QStringLiteral("summary")).isObject())
                        && (!root.contains(QStringLiteral("spend_by_currency")) || root.value(QStringLiteral("spend_by_currency")).isArray())
                        && numericWhenPresent(root, QStringLiteral("spend"))
                        && numericWhenPresent(root, QStringLiteral("prompt_tokens"))
                        && numericWhenPresent(root, QStringLiteral("completion_tokens"))
                        && numericWhenPresent(root, QStringLiteral("requests"))
                        && numericWhenPresent(summary, QStringLiteral("spend"))
                        && numericWhenPresent(summary, QStringLiteral("prompt_tokens"))
                        && numericWhenPresent(summary, QStringLiteral("completion_tokens"))
                        && numericWhenPresent(summary, QStringLiteral("requests"));
                }
                if (document.isArray()) {
                    for (const QJsonValue &entry : document.array()) {
                        if (!entry.isObject()
                            || !numericWhenPresent(entry.toObject(), QStringLiteral("spend"))
                            || !numericWhenPresent(entry.toObject(), QStringLiteral("prompt_tokens"))
                            || !numericWhenPresent(entry.toObject(), QStringLiteral("completion_tokens"))) {
                            validGatewayShape = false;
                            break;
                        }
                    }
                }
                if (root.value(QStringLiteral("spend_by_currency")).isArray()) {
                    for (const QJsonValue &entry : root.value(QStringLiteral("spend_by_currency")).toArray()) {
                        const QJsonObject total = entry.toObject();
                        if (!entry.isObject() || !total.value(QStringLiteral("amount")).isDouble()
                            || !total.value(QStringLiteral("currency")).isString()
                            || total.value(QStringLiteral("currency")).toString().isEmpty()) {
                            validGatewayShape = false;
                            break;
                        }
                    }
                }
                if (!validGatewayShape) {
                    setErrorDetails(i18n("The gateway usage response has an unexpected schema"), ProviderErrorKind::Schema);
                    setConnected(false);
                    setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("failed"),
                                        i18n("Schema drift in the read-only usage response"));
                    reply->deleteLater();
                    setLoading(false);
                    updateLastRefreshed();
                    Q_EMIT dataUpdated();
                    return;
                }
                if (document.isArray()) {
                    QMap<QString, double> spendByCurrency;
                    qint64 input = 0; qint64 output = 0; int requests = 0;
                    for (const QJsonValue &entry : document.array()) {
                        const QJsonObject row = entry.toObject();
                        const QString currency = row.value(QStringLiteral("currency")).toString(QStringLiteral("USD")).toUpper();
                        spendByCurrency[currency] += row.value(QStringLiteral("spend")).toDouble();
                        input += row.value(QStringLiteral("prompt_tokens")).toInteger();
                        output += row.value(QStringLiteral("completion_tokens")).toInteger();
                        ++requests;
                    }
                    root.insert(QStringLiteral("prompt_tokens"), input);
                    root.insert(QStringLiteral("completion_tokens"), output);
                    root.insert(QStringLiteral("requests"), requests);
                    QJsonArray totals;
                    for (auto it = spendByCurrency.cbegin(); it != spendByCurrency.cend(); ++it) {
                        totals.append(QJsonObject{{QStringLiteral("currency"), it.key()},
                                                 {QStringLiteral("amount"), it.value()}});
                    }
                    root.insert(QStringLiteral("spend_by_currency"), totals);
                }
                const QJsonObject summary = root.value(QStringLiteral("summary")).toObject();
                const qint64 input = root.value(QStringLiteral("prompt_tokens")).toInteger(summary.value(QStringLiteral("prompt_tokens")).toInteger());
                const qint64 output = root.value(QStringLiteral("completion_tokens")).toInteger(summary.value(QStringLiteral("completion_tokens")).toInteger());
                setActualUsage(input, output, root.value(QStringLiteral("requests")).toInt(summary.value(QStringLiteral("requests")).toInt()));
                QJsonArray totals = root.value(QStringLiteral("spend_by_currency")).toArray();
                if (totals.isEmpty()) {
                    const QJsonValue spend = root.contains(QStringLiteral("spend"))
                        ? root.value(QStringLiteral("spend")) : summary.value(QStringLiteral("spend"));
                    if (spend.isDouble()) {
                        totals.append(QJsonObject{
                            {QStringLiteral("currency"), root.value(QStringLiteral("currency")).toString(QStringLiteral("USD")).toUpper()},
                            {QStringLiteral("amount"), spend.toDouble()}});
                    }
                }
                for (const QJsonValue &entry : totals) {
                    const QJsonObject total = entry.toObject();
                    setProviderMetric(MetricKind::Cost, total.value(QStringLiteral("amount")).toDouble(),
                                      total.value(QStringLiteral("currency")).toString(),
                                      total.value(QStringLiteral("currency")).toString(),
                                      QStringLiteral("gateway"), QStringLiteral("current"),
                                      MetricSource::UsageApi, QStringLiteral("actual"));
                }
                if (totals.size() == 1) {
                    const QJsonObject total = totals.first().toObject();
                    setCurrency(total.value(QStringLiteral("currency")).toString(QStringLiteral("USD")));
                    setCost(total.value(QStringLiteral("amount")).toDouble());
                    setDailyCost(total.value(QStringLiteral("amount")).toDouble());
                    setMonthlyCost(total.value(QStringLiteral("amount")).toDouble());
                    setDataQuality(QStringLiteral("actual"));
                } else {
                    setDataQuality(QStringLiteral("actual_mixed_currency"));
                }
                setCostSource(QStringLiteral("usage_api")); setUsageSource(QStringLiteral("usage_api"));
                setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("available"));
            } else {
                QJsonArray models;
                if (document.isArray()) models = document.array();
                else if (root.value(QStringLiteral("data")).isArray()) models = root.value(QStringLiteral("data")).toArray();
                else if (root.value(QStringLiteral("models")).isArray()) models = root.value(QStringLiteral("models")).toArray();
                if (models.isEmpty()) {
                    setErrorDetails(i18n("The model discovery response contains no model list"), ProviderErrorKind::Schema);
                    setConnected(false);
                    setCapabilityStatus(QStringLiteral("model_discovery"), QStringLiteral("failed"),
                                        i18n("Schema drift or an empty model list"));
                    reply->deleteLater();
                    setLoading(false);
                    updateLastRefreshed();
                    Q_EMIT dataUpdated();
                    return;
                }
                int validModelRows = 0;
                for (const QJsonValue &entry : models) {
                    QVariantMap row = entry.toObject().toVariantMap();
                    QString id = row.value(QStringLiteral("id")).toString();
                    if (id.isEmpty()) id = row.value(QStringLiteral("name")).toString();
                    if (id.isEmpty()) id = row.value(QStringLiteral("model")).toString();
                    if (id.isEmpty()) continue;
                    ++validModelRows;
                    if (m_pendingModelIds.contains(id)) continue;
                    m_pendingModelIds.insert(id);
                    row.insert(QStringLiteral("id"), id);
                    row.insert(QStringLiteral("discoverySource"), QStringLiteral("live_api"));
                    m_pendingDiscoveredModels.append(row);
                }
                if (validModelRows == 0) {
                    setErrorDetails(i18n("The model discovery response contains no valid model identifiers"), ProviderErrorKind::Schema);
                    setConnected(false);
                    setCapabilityStatus(QStringLiteral("model_discovery"), QStringLiteral("failed"),
                                        i18n("Schema drift in the read-only model response"));
                    reply->deleteLater();
                    setLoading(false);
                    updateLastRefreshed();
                    Q_EMIT dataUpdated();
                    return;
                }

                const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString();
                const int requestBudget = m_descriptor.value(QStringLiteral("safeRefresh")).toMap()
                                              .value(QStringLiteral("requestBudget"), 1).toInt();
                if (!nextPageToken.isEmpty() && m_pageRequests < qMax(1, requestBudget)) {
                    QUrl nextUrl = reply->url();
                    QUrlQuery query(nextUrl);
                    query.removeAllQueryItems(QStringLiteral("pageToken"));
                    query.addQueryItem(QStringLiteral("pageToken"), nextPageToken);
                    nextUrl.setQuery(query);
                    const int generation = currentGeneration();
                    reply->deleteLater();
                    requestPage(nextUrl, generation);
                    return;
                }
                const bool truncated = !nextPageToken.isEmpty();
                finalizeModelDiscovery(truncated,
                    truncated ? i18n("Model discovery stopped at the configured request budget") : QString());
            }
            parseRateLimitHeaders(reply);
            setConnected(true);
            setCapabilityStatus(QStringLiteral("scheduled_refresh"), QStringLiteral("available"));
        }
    }
    reply->deleteLater();
    setLoading(false);
    updateLastRefreshed();
    Q_EMIT dataUpdated();
}
