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

    // The native key endpoint is the scheduled source of truth.  Avoid an
    // extra model-list request: a usage monitor should spend the smallest
    // possible read-only request budget.
    beginRefresh();
    setLoading(true);
    clearError();
    fetchCredits();
}

void OpenRouterProvider::fetchCredits()
{
    QUrl url(QStringLiteral("%1/key").arg(effectiveBaseUrl(defaultBaseUrl())));

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
        setCapabilityStatus(QStringLiteral("key_usage"), QStringLiteral("failed"),
                            i18n("OpenRouter key usage is unavailable"));
        setNetworkError(reply, i18n("Credits API unavailable: %1", reply->errorString()));
        setConnected(false);
        onAllRequestsDone();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        QJsonObject root = doc.object();
        QJsonObject dataObj = root.value(QStringLiteral("data")).toObject();
        const auto numericValue = [&dataObj](const QString &key) -> QVariant {
            const QJsonValue value = dataObj.value(key);
            return value.isDouble() ? QVariant(value.toDouble()) : QVariant();
        };
        const QVariant creditsValue = numericValue(QStringLiteral("limit_remaining"));
        const QVariant dailyValue = numericValue(QStringLiteral("usage_daily"));
        const QVariant weeklyValue = numericValue(QStringLiteral("usage_weekly"));
        const QVariant monthlyValue = numericValue(QStringLiteral("usage_monthly"));
        const QVariant allTimeValue = numericValue(QStringLiteral("usage"));

        m_creditsAvailable = creditsValue.isValid();
        if (m_creditsAvailable) m_credits = creditsValue.toDouble();
        if (dailyValue.isValid()) { m_usageDaily = dailyValue.toDouble(); setDailyCost(m_usageDaily); }
        if (weeklyValue.isValid()) m_usageWeekly = weeklyValue.toDouble();
        if (monthlyValue.isValid()) { m_usageMonthly = monthlyValue.toDouble(); setMonthlyCost(m_usageMonthly); }
        if (allTimeValue.isValid()) setCost(allTimeValue.toDouble());
        setCostSource(QStringLiteral("usage_api"));
        setUsageSource(QStringLiteral("usage_api"));
        setCurrency(QStringLiteral("USD"));
        setDataQuality(QStringLiteral("actual"));
        setProviderMetric(MetricKind::CreditBalance, creditsValue, QStringLiteral("USD"), QStringLiteral("USD"),
                          QStringLiteral("api_key"), QStringLiteral("current"), MetricSource::UsageApi, QStringLiteral("actual"));
        setProviderMetric(MetricKind::Cost, dailyValue, QStringLiteral("USD"), QStringLiteral("USD"),
                          QStringLiteral("api_key"), QStringLiteral("day"), MetricSource::UsageApi, QStringLiteral("actual"));
        setProviderMetric(MetricKind::Cost, weeklyValue, QStringLiteral("USD"), QStringLiteral("USD"),
                          QStringLiteral("api_key"), QStringLiteral("week"), MetricSource::UsageApi, QStringLiteral("actual"));
        setProviderMetric(MetricKind::Cost, monthlyValue, QStringLiteral("USD"), QStringLiteral("USD"),
                          QStringLiteral("api_key"), QStringLiteral("month"), MetricSource::UsageApi, QStringLiteral("actual"));
        setProviderMetric(MetricKind::Cost, allTimeValue, QStringLiteral("USD"), QStringLiteral("USD"),
                          QStringLiteral("api_key"), QStringLiteral("all_time"), MetricSource::UsageApi, QStringLiteral("actual"));
        setRateLimitResetTime(dataObj.value(QStringLiteral("limit_reset")).toString());
        setCapabilityStatus(QStringLiteral("key_usage"), QStringLiteral("available"));
        setConnected(true);
        Q_EMIT creditsChanged();
        Q_EMIT usageWindowsChanged();
    }

    onAllRequestsDone();
}
