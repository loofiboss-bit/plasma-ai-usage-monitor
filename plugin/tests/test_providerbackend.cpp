#include <QtTest>
#include <QSignalSpy>
#include <QTimeZone>

#include "providerbackend.h"

/**
 * Minimal concrete subclass for testing ProviderBackend's non-virtual logic.
 * Exposes protected methods via public wrappers.
 */
class TestProvider : public ProviderBackend
{
    Q_OBJECT
public:
    explicit TestProvider(QObject *parent = nullptr)
        : ProviderBackend(parent)
    {}

    QString name() const override { return QStringLiteral("TestProvider"); }
    QString iconName() const override { return QStringLiteral("test-icon"); }
    void refreshImpl() override
    {
        ++refreshCalls;
        beginRefresh();
        setLoading(true);
    }

    int refreshCalls = 0;

    // Public wrappers for protected methods
    using ProviderBackend::setConnected;
    using ProviderBackend::setLoading;
    using ProviderBackend::setError;
    using ProviderBackend::setErrorDetails;
    using ProviderBackend::clearError;
    using ProviderBackend::setInputTokens;
    using ProviderBackend::setOutputTokens;
    using ProviderBackend::setRequestCount;
    using ProviderBackend::setCost;
    using ProviderBackend::setCurrency;
    using ProviderBackend::setDailyCost;
    using ProviderBackend::setMonthlyCost;
    using ProviderBackend::effectiveBaseUrl;
    using ProviderBackend::beginRefresh;
    using ProviderBackend::isCurrentGeneration;
    using ProviderBackend::registerModelPricing;
    using ProviderBackend::registerCatalogPricing;
    using ProviderBackend::setPricingModel;
    using ProviderBackend::updateEstimatedCost;
    using ProviderBackend::checkBudgetLimits;
    using ProviderBackend::isRetryableStatus;
    using ProviderBackend::errorKindForNetworkReply;
    using ProviderBackend::retryAfterForReply;
    using ProviderBackend::setProviderMetric;
    using ProviderBackend::clearProviderMetric;
    using ProviderBackend::setCapabilityStatus;
};

class ProviderBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testProviderKeyEnumConversionAzure();
    void testProviderKeyEnumConversionAzureAliases();
    void testProviderKeyEnumConversionOllamaCloud();
    void testProviderConfigFallbackUnknownDeterministic();
    void testGoogleVeoNormalizationUsesOpenAiLikeUsage();
    void testExistingProviderMappingsUnchanged();
    void testBudgetWarningSignal();
    void testBudgetExceededSignal();
    void testBudgetDedupFlags();
    void testMonthlyBudgetSignals();
    void testBudgetCurrencyMismatchDisablesAlerts();
    void testCostEstimation();
    void testCostEstimationRequiresExactModel();
    void testActualBillingClearsEstimatedMetric();
    void testGenerationCounter();
    void testDisconnectReconnectSignals();
    void testNoSignalOnSameState();
    void testReconnectRequiresPriorConnection();
    void testErrorCountAndConsecutiveErrors();
    void testClearError();
    void testIsRetryableStatus();
    void testTypedErrorRetryability();
    void testNetworkErrorClassification();
    void testRefreshCoalescingAndManualSupersede();
    void testRetryAfterNumericAndHttpDate();
    void testEffectiveBaseUrl();
    void testEffectiveBaseUrlTrailingSlash();
    void testTotalTokens();
    void testNullableMetricContract();
    void testMetricSourcePeriodAndCapabilityContract();
    void testUnsafeCustomEndpointRejected();
};

void ProviderBackendTest::testMetricSourcePeriodAndCapabilityContract()
{
    TestProvider p;
    const QDateTime start(QDate(2026, 7, 1), QTime(0, 0), QTimeZone::UTC);
    const QDateTime end = start.addDays(1);
    p.setProviderMetric(ProviderBackend::MetricKind::Cost, 1.25, QStringLiteral("USD"),
                        QStringLiteral("USD"), QStringLiteral("project"), QStringLiteral("day"),
                        ProviderBackend::MetricSource::BillingApi, QStringLiteral("actual"),
                        end, start, end, QStringLiteral("claude-opus-4-8"),
                        QStringLiteral("workspace-a"));
    const QVariantMap metric = p.metric(QStringLiteral("cost"), QStringLiteral("project"), QStringLiteral("day"));
    QCOMPARE(metric.value(QStringLiteral("source")).toString(), QStringLiteral("billing_api"));
    QCOMPARE(metric.value(QStringLiteral("periodStart")).toDateTime(), start);
    QCOMPARE(metric.value(QStringLiteral("periodEnd")).toDateTime(), end);
    QCOMPARE(metric.value(QStringLiteral("resetAt")).toDateTime(), end);
    QCOMPARE(metric.value(QStringLiteral("modelScope")).toString(), QStringLiteral("claude-opus-4-8"));
    QCOMPARE(metric.value(QStringLiteral("projectScope")).toString(), QStringLiteral("workspace-a"));

    p.setProviderMetric(ProviderBackend::MetricKind::CacheReadInputTokens, 42,
                        QStringLiteral("token"), QString(), QStringLiteral("organization"),
                        QStringLiteral("day"), ProviderBackend::MetricSource::UsageApi,
                        QStringLiteral("actual"), {}, start, end,
                        QStringLiteral("claude-opus-4-8"), QStringLiteral("workspace-a"));
    p.setProviderMetric(ProviderBackend::MetricKind::CacheCreationInputTokens, 7,
                        QStringLiteral("token"), QString(), QStringLiteral("organization"),
                        QStringLiteral("day"), ProviderBackend::MetricSource::UsageApi,
                        QStringLiteral("actual"), {}, start, end,
                        QStringLiteral("claude-opus-4-8"), QStringLiteral("workspace-a"));
    QCOMPARE(p.metric(QStringLiteral("cache_read_input_tokens"),
                      QStringLiteral("organization"), QStringLiteral("day"))
                 .value(QStringLiteral("value")).toInt(), 42);
    QCOMPARE(p.metric(QStringLiteral("cache_creation_input_tokens"),
                      QStringLiteral("organization"), QStringLiteral("day"))
                 .value(QStringLiteral("value")).toInt(), 7);

    p.setProviderMetric(ProviderBackend::MetricKind::RequestLimit, 1000,
                        QStringLiteral("request"), QString(), QStringLiteral("project"),
                        QStringLiteral("minute"), ProviderBackend::MetricSource::PublishedDocumentation,
                        QStringLiteral("published_cap"));
    p.clearProviderMetric(ProviderBackend::MetricKind::RequestRemaining,
                          QStringLiteral("project"), QStringLiteral("minute"));
    const QVariantMap published = p.metric(QStringLiteral("request_limit"), QStringLiteral("project"), QStringLiteral("minute"));
    const QVariantMap remaining = p.metric(QStringLiteral("request_remaining"), QStringLiteral("project"), QStringLiteral("minute"));
    QCOMPARE(published.value(QStringLiteral("source")).toString(), QStringLiteral("published_documentation"));
    QCOMPARE(published.value(QStringLiteral("quality")).toString(), QStringLiteral("published_cap"));
    QVERIFY(published.value(QStringLiteral("available")).toBool());
    QVERIFY(!remaining.value(QStringLiteral("available")).toBool());

    p.setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("available"));
    p.setCapabilityStatus(QStringLiteral("billing"), QStringLiteral("failed"), QStringLiteral("permission denied"));
    QCOMPARE(p.capabilityStatus().value(QStringLiteral("usage")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    QCOMPARE(p.capabilityStatus().value(QStringLiteral("billing")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
}

void ProviderBackendTest::testUnsafeCustomEndpointRejected()
{
    TestProvider p;
    p.setCustomBaseUrl(QStringLiteral("http://account:secret@example.com/path?api_key=secret"));
    QVERIFY(p.customBaseUrl().isEmpty());
    p.setCustomBaseUrl(QStringLiteral("ftp://example.com/provider"));
    QVERIFY(p.customBaseUrl().isEmpty());
    p.setCustomBaseUrl(QStringLiteral("http://127.0.0.1:8080"));
    QCOMPARE(p.customBaseUrl(), QStringLiteral("http://127.0.0.1:8080"));
}

void ProviderBackendTest::testNullableMetricContract()
{
    TestProvider p;
    p.clearProviderMetric(ProviderBackend::MetricKind::TokenRemaining,
                          QStringLiteral("project"), QStringLiteral("minute"));
    QCOMPARE(p.metrics().size(), 1);
    QVariantMap metric = p.metrics().first().toMap();
    QCOMPARE(metric.value(QStringLiteral("kind")).toString(), QStringLiteral("token_remaining"));
    QCOMPARE(metric.value(QStringLiteral("available")).toBool(), false);
    QCOMPARE(metric.value(QStringLiteral("scope")).toString(), QStringLiteral("project"));
    QCOMPARE(metric.value(QStringLiteral("window")).toString(), QStringLiteral("minute"));

    p.setProviderMetric(ProviderBackend::MetricKind::TokenRemaining, 0,
                        QStringLiteral("token"), QString(), QStringLiteral("project"),
                        QStringLiteral("minute"), ProviderBackend::MetricSource::ResponseHeaders,
                        QStringLiteral("actual"));
    metric = p.metrics().first().toMap();
    QCOMPARE(metric.value(QStringLiteral("available")).toBool(), true);
    QCOMPARE(metric.value(QStringLiteral("value")).toInt(), 0);
}

void ProviderBackendTest::testBudgetWarningSignal()
{
    TestProvider p;
    p.setDailyBudget(10.0);
    p.setBudgetWarningPercent(80);

    QSignalSpy warningSpy(&p, &ProviderBackend::budgetWarning);

    // Set daily cost to 80% of budget — should trigger warning
    p.setDailyCost(8.0);
    QCOMPARE(warningSpy.count(), 1);

    QList<QVariant> args = warningSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("TestProvider"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("daily"));
    QVERIFY(qAbs(args.at(2).toDouble() - 8.0) < 0.01);
    QVERIFY(qAbs(args.at(3).toDouble() - 10.0) < 0.01);
}

void ProviderBackendTest::testProviderKeyEnumConversionAzure()
{
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("azure")),
             ProviderBackend::ProviderId::AzureOpenAI);
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("azure-openai")),
             ProviderBackend::ProviderId::AzureOpenAI);
    QCOMPARE(ProviderBackend::providerKeyFromId(ProviderBackend::ProviderId::AzureOpenAI),
             QStringLiteral("azure-openai"));

    const ProviderBackend::ProviderConfig azureConfig = ProviderBackend::makeProviderConfig(
        QStringLiteral("azure-openai"),
        QStringLiteral("https://example.openai.azure.com"),
        QStringLiteral("gpt-5.4-pro"),
        QStringLiteral("my-deployment"),
        QStringLiteral("secret"));

    QCOMPARE(azureConfig.providerId, ProviderBackend::ProviderId::AzureOpenAI);
    QCOMPARE(azureConfig.providerKey, QStringLiteral("azure-openai"));
    QCOMPARE(azureConfig.authKeySlot, QStringLiteral("azure_openai_api_key"));
}

void ProviderBackendTest::testProviderKeyEnumConversionAzureAliases()
{
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("AZURE_OPENAI")),
             ProviderBackend::ProviderId::AzureOpenAI);
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral(" Azure ")),
             ProviderBackend::ProviderId::AzureOpenAI);
    QCOMPARE(ProviderBackend::defaultAuthKeySlotForProvider(ProviderBackend::ProviderId::AzureOpenAI),
             QStringLiteral("azure_openai_api_key"));
}

void ProviderBackendTest::testProviderKeyEnumConversionOllamaCloud()
{
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("ollama")),
             ProviderBackend::ProviderId::OllamaCloud);
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("ollama-cloud")),
             ProviderBackend::ProviderId::OllamaCloud);
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral(" OLLAMA_CLOUD ")),
             ProviderBackend::ProviderId::OllamaCloud);
    QCOMPARE(ProviderBackend::providerKeyFromId(ProviderBackend::ProviderId::OllamaCloud),
             QStringLiteral("ollama"));
    QCOMPARE(ProviderBackend::defaultAuthKeySlotForProvider(ProviderBackend::ProviderId::OllamaCloud),
             QStringLiteral("ollama_api_key"));

    const ProviderBackend::ProviderConfig ollamaConfig = ProviderBackend::makeProviderConfig(
        QStringLiteral("ollama"),
        QStringLiteral("https://ollama.com/v1"),
        QStringLiteral("gpt-oss:120b"),
        QString(),
        QStringLiteral("secret"));

    QCOMPARE(ollamaConfig.providerId, ProviderBackend::ProviderId::OllamaCloud);
    QCOMPARE(ollamaConfig.providerKey, QStringLiteral("ollama"));
    QCOMPARE(ollamaConfig.authKeySlot, QStringLiteral("ollama_api_key"));
}

void ProviderBackendTest::testProviderConfigFallbackUnknownDeterministic()
{
    const ProviderBackend::ProviderConfig unknownConfig = ProviderBackend::makeProviderConfig(
        QStringLiteral("some-future-provider"),
        QStringLiteral("https://example.invalid"),
        QStringLiteral("model-x"),
        QStringLiteral("deployment-x"),
        QStringLiteral("secret"));

    QCOMPARE(unknownConfig.providerId, ProviderBackend::ProviderId::Unknown);
    QCOMPARE(unknownConfig.providerKey, QStringLiteral("unknown"));
    QCOMPARE(unknownConfig.authKeySlot, QStringLiteral("unknown_api_key"));

    const ProviderBackend::NormalizedUsageCost normalized =
        ProviderBackend::normalizeUsageCost(ProviderBackend::ProviderId::Unknown, QJsonObject{});
    QVERIFY(!normalized.parsed);
    QCOMPARE(normalized.inputTokens, 0);
    QCOMPARE(normalized.outputTokens, 0);
    QCOMPARE(normalized.requestCount, 0);
    QCOMPARE(normalized.cost, 0.0);
}

void ProviderBackendTest::testGoogleVeoNormalizationUsesOpenAiLikeUsage()
{
    QJsonObject usage;
    usage.insert(QStringLiteral("prompt_tokens"), 111);
    usage.insert(QStringLiteral("completion_tokens"), 22);
    usage.insert(QStringLiteral("total_tokens"), 133);

    QJsonObject payload;
    payload.insert(QStringLiteral("usage"), usage);

    const ProviderBackend::NormalizedUsageCost normalized =
        ProviderBackend::normalizeUsageCost(ProviderBackend::ProviderId::GoogleVeo, payload);

    QVERIFY(normalized.parsed);
    QCOMPARE(normalized.inputTokens, 111);
    QCOMPARE(normalized.outputTokens, 22);
    QCOMPARE(normalized.requestCount, 1);
    QCOMPARE(normalized.cost, 0.0);
}

void ProviderBackendTest::testExistingProviderMappingsUnchanged()
{
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("openai")),
             ProviderBackend::ProviderId::OpenAI);
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("google")),
             ProviderBackend::ProviderId::Google);
    QCOMPARE(ProviderBackend::providerIdFromKey(QStringLiteral("xai")),
             ProviderBackend::ProviderId::XAI);
    QCOMPARE(ProviderBackend::providerKeyFromId(ProviderBackend::ProviderId::OpenAI),
             QStringLiteral("openai"));
    QCOMPARE(ProviderBackend::providerKeyFromId(ProviderBackend::ProviderId::Google),
             QStringLiteral("google"));
    QCOMPARE(ProviderBackend::providerKeyFromId(ProviderBackend::ProviderId::XAI),
             QStringLiteral("xai"));
}

void ProviderBackendTest::testBudgetExceededSignal()
{
    TestProvider p;
    p.setDailyBudget(10.0);

    QSignalSpy exceededSpy(&p, &ProviderBackend::budgetExceeded);

    // Set daily cost at 100% — should trigger exceeded
    p.setDailyCost(10.0);
    QCOMPARE(exceededSpy.count(), 1);

    QList<QVariant> args = exceededSpy.takeFirst();
    QCOMPARE(args.at(1).toString(), QStringLiteral("daily"));
}

void ProviderBackendTest::testBudgetDedupFlags()
{
    TestProvider p;
    p.setDailyBudget(10.0);

    QSignalSpy exceededSpy(&p, &ProviderBackend::budgetExceeded);

    // Exceed budget twice — should only emit once (dedup)
    p.setDailyCost(10.0);
    QCOMPARE(exceededSpy.count(), 1);
    p.setDailyCost(11.0);
    QCOMPARE(exceededSpy.count(), 1); // still 1, deduped

    // Reset by dropping below warning threshold
    p.setDailyCost(1.0);
    p.setDailyCost(10.0);
    QCOMPARE(exceededSpy.count(), 2); // now emits again
}

void ProviderBackendTest::testMonthlyBudgetSignals()
{
    TestProvider p;
    p.setMonthlyBudget(100.0);
    p.setBudgetWarningPercent(80);

    QSignalSpy warningSpy(&p, &ProviderBackend::budgetWarning);
    QSignalSpy exceededSpy(&p, &ProviderBackend::budgetExceeded);

    p.setMonthlyCost(80.0);
    QCOMPARE(warningSpy.count(), 1);
    QCOMPARE(warningSpy.first().at(1).toString(), QStringLiteral("monthly"));

    p.setMonthlyCost(100.0);
    QCOMPARE(exceededSpy.count(), 1);
    QCOMPARE(exceededSpy.first().at(1).toString(), QStringLiteral("monthly"));
}

void ProviderBackendTest::testBudgetCurrencyMismatchDisablesAlerts()
{
    TestProvider p;
    p.setBudgetCurrency(QStringLiteral("USD"));
    p.setCurrency(QStringLiteral("EUR"));
    p.setDailyBudget(10.0);
    QSignalSpy warningSpy(&p, &ProviderBackend::budgetWarning);
    QSignalSpy exceededSpy(&p, &ProviderBackend::budgetExceeded);
    p.setDailyCost(20.0);
    QVERIFY(p.budgetCurrencyMismatch());
    QCOMPARE(warningSpy.count(), 0);
    QCOMPARE(exceededSpy.count(), 0);
}

void ProviderBackendTest::testCostEstimation()
{
    TestProvider p;
    // Register model: $3/M input, $15/M output
    p.registerModelPricing(QStringLiteral("test-model"), 3.0, 15.0);

    p.setInputTokens(1000000);  // 1M input tokens
    p.setOutputTokens(500000);  // 0.5M output tokens

    p.updateEstimatedCost(QStringLiteral("test-model"));

    // Expected: (1M/1M)*3 + (0.5M/1M)*15 = 3.0 + 7.5 = 10.5
    QVERIFY(qAbs(p.cost() - 10.5) < 0.01);
    QVERIFY(p.isEstimatedCost());
}

void ProviderBackendTest::testCostEstimationRequiresExactModel()
{
    TestProvider p;
    p.registerModelPricing(QStringLiteral("mistral-large-latest"), 2.0, 6.0);

    p.setInputTokens(2000000);
    p.setOutputTokens(1000000);

    p.updateEstimatedCost(QStringLiteral("mistral-large-latest"));

    // Expected: (2M/1M)*2 + (1M/1M)*6 = 4.0 + 6.0 = 10.0
    QVERIFY(qAbs(p.cost() - 10.0) < 0.01);
    QVERIFY(p.isEstimatedCost());
}

void ProviderBackendTest::testActualBillingClearsEstimatedMetric()
{
    TestProvider p;
    p.registerCatalogPricing(QStringLiteral("openai"));
    p.setPricingModel(QStringLiteral("gpt-5.4"));
    p.setInputTokens(1000000);
    p.setOutputTokens(100000);
    p.updateEstimatedCost(QStringLiteral("gpt-5.4"));

    bool hasEstimate = false;
    for (const QVariant &entry : p.metrics()) {
        const QVariantMap metric = entry.toMap();
        hasEstimate = hasEstimate
            || metric.value(QStringLiteral("source")).toString() == QLatin1String("estimated_pricing");
    }
    QVERIFY(hasEstimate);

    p.setCurrency(QStringLiteral("USD"));
    p.setCost(1.25);
    for (const QVariant &entry : p.metrics()) {
        const QVariantMap metric = entry.toMap();
        QVERIFY(metric.value(QStringLiteral("source")).toString()
                != QLatin1String("estimated_pricing"));
    }
}

void ProviderBackendTest::testGenerationCounter()
{
    TestProvider p;
    QCOMPARE(p.currentGeneration(), 0);

    int gen0 = p.currentGeneration();
    p.beginRefresh();
    QCOMPARE(p.currentGeneration(), 1);
    QVERIFY(!p.isCurrentGeneration(gen0));
    QVERIFY(p.isCurrentGeneration(1));

    p.beginRefresh();
    QCOMPARE(p.currentGeneration(), 2);
    QVERIFY(!p.isCurrentGeneration(1));
}

void ProviderBackendTest::testTypedErrorRetryability()
{
    TestProvider p;
    p.setErrorDetails(QStringLiteral("Too many requests"),
                      ProviderBackend::ProviderErrorKind::RateLimit,
                      429,
                      QDateTime::currentDateTimeUtc().addSecs(30));
    QCOMPARE(p.errorKind(), ProviderBackend::ProviderErrorKind::RateLimit);
    QCOMPARE(p.httpStatus(), 429);
    QVERIFY(p.isRetryable());
    QVERIFY(p.retryAfter().isValid());

    p.setErrorDetails(QStringLiteral("Unauthorized"),
                      ProviderBackend::ProviderErrorKind::Authentication,
                      401);
    QVERIFY(!p.isRetryable());
}

void ProviderBackendTest::testRefreshCoalescingAndManualSupersede()
{
    TestProvider p;
    QVERIFY(p.requestRefresh(ProviderBackend::RefreshReason::Startup));
    QCOMPARE(p.refreshCalls, 1);
    QCOMPARE(p.lastRefreshReason(), ProviderBackend::RefreshReason::Startup);

    QVERIFY(!p.requestRefresh(ProviderBackend::RefreshReason::Scheduled));
    QCOMPARE(p.refreshCalls, 1);
    QCOMPARE(p.coalescedRefreshCount(), 1);

    QVERIFY(p.requestRefresh(ProviderBackend::RefreshReason::Manual));
    QCOMPARE(p.refreshCalls, 2);
    QCOMPARE(p.cancellationCount(), 1);
    QCOMPARE(p.lastRefreshReason(), ProviderBackend::RefreshReason::Manual);
}

void ProviderBackendTest::testRetryAfterNumericAndHttpDate()
{
    class Reply final : public QNetworkReply {
    public:
        Reply()
        {
            setOpenMode(QIODevice::ReadOnly);
            setFinished(true);
        }
        void abort() override {}
        qint64 bytesAvailable() const override { return 0; }
        void setRetryAfter(const QByteArray &value) { setRawHeader("Retry-After", value); }
    protected:
        qint64 readData(char *, qint64) override { return -1; }
    } reply;

    const QDateTime now(QDate(2026, 7, 13), QTime(12, 0), QTimeZone::UTC);
    reply.setRetryAfter("42");
    TestProvider provider;
    QCOMPARE(provider.retryAfterForReply(&reply, now), now.addSecs(42));

    reply.setRetryAfter("Mon, 13 Jul 2026 12:02:00 GMT");
    QCOMPARE(provider.retryAfterForReply(&reply, now), now.addSecs(120));

    reply.setRetryAfter("not-a-reset-time");
    QVERIFY(!provider.retryAfterForReply(&reply, now).isValid());
}

void ProviderBackendTest::testNetworkErrorClassification()
{
    class Reply final : public QNetworkReply {
    public:
        Reply()
        {
            setOpenMode(QIODevice::ReadOnly);
            setFinished(true);
        }
        void abort() override {}
        qint64 bytesAvailable() const override { return 0; }
        void setNetworkError(QNetworkReply::NetworkError error) { setError(error, QStringLiteral("test error")); }
        void setHttpStatus(int status) { setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status); }
    protected:
        qint64 readData(char *, qint64) override { return -1; }
    } reply;

    reply.setNetworkError(QNetworkReply::TimeoutError);
    QCOMPARE(TestProvider::errorKindForNetworkReply(&reply), ProviderBackend::ProviderErrorKind::Timeout);

    reply.setNetworkError(QNetworkReply::HostNotFoundError);
    QCOMPARE(TestProvider::errorKindForNetworkReply(&reply), ProviderBackend::ProviderErrorKind::Network);

    reply.setHttpStatus(503);
    QCOMPARE(TestProvider::errorKindForNetworkReply(&reply), ProviderBackend::ProviderErrorKind::Server);
}

void ProviderBackendTest::testDisconnectReconnectSignals()
{
    TestProvider p;
    QSignalSpy disconnectSpy(&p, &ProviderBackend::providerDisconnected);
    QSignalSpy reconnectSpy(&p, &ProviderBackend::providerReconnected);

    // First connection — no disconnect/reconnect signals
    p.setConnected(true);
    QCOMPARE(disconnectSpy.count(), 0);
    QCOMPARE(reconnectSpy.count(), 0);

    // Disconnect
    p.setConnected(false);
    QCOMPARE(disconnectSpy.count(), 1);
    QCOMPARE(disconnectSpy.first().at(0).toString(), QStringLiteral("TestProvider"));

    // Reconnect
    p.setConnected(true);
    QCOMPARE(reconnectSpy.count(), 1);
    QCOMPARE(reconnectSpy.first().at(0).toString(), QStringLiteral("TestProvider"));
}

void ProviderBackendTest::testNoSignalOnSameState()
{
    TestProvider p;
    QSignalSpy connSpy(&p, &ProviderBackend::connectedChanged);

    p.setConnected(true);
    QCOMPARE(connSpy.count(), 1);

    // Setting same state again — no signal
    p.setConnected(true);
    QCOMPARE(connSpy.count(), 1);
}

void ProviderBackendTest::testReconnectRequiresPriorConnection()
{
    TestProvider p;
    QSignalSpy reconnectSpy(&p, &ProviderBackend::providerReconnected);

    // Never connected → set to false → set to true
    // Should NOT emit reconnected because it was never connected before
    p.setConnected(false); // no change, starts false
    p.setConnected(true);
    QCOMPARE(reconnectSpy.count(), 0);
}

void ProviderBackendTest::testErrorCountAndConsecutiveErrors()
{
    TestProvider p;
    QCOMPARE(p.errorCount(), 0);
    QCOMPARE(p.consecutiveErrors(), 0);

    p.setError(QStringLiteral("Error 1"));
    QCOMPARE(p.errorCount(), 1);
    QCOMPARE(p.consecutiveErrors(), 1);
    QCOMPARE(p.errorString(), QStringLiteral("Error 1"));

    p.setError(QStringLiteral("Error 2"));
    QCOMPARE(p.errorCount(), 2);
    QCOMPARE(p.consecutiveErrors(), 2);
}

void ProviderBackendTest::testClearError()
{
    TestProvider p;
    p.setError(QStringLiteral("Some error"));
    QCOMPARE(p.consecutiveErrors(), 1);

    QSignalSpy errorSpy(&p, &ProviderBackend::errorChanged);
    p.clearError();

    QVERIFY(p.errorString().isEmpty());
    QCOMPARE(p.consecutiveErrors(), 0);
    QCOMPARE(p.errorCount(), 1); // total count persists
    QCOMPARE(errorSpy.count(), 1);

    // Clearing already clear error — no signal
    p.clearError();
    QCOMPARE(errorSpy.count(), 1);
}

void ProviderBackendTest::testIsRetryableStatus()
{
    QVERIFY(TestProvider::isRetryableStatus(429));
    QVERIFY(TestProvider::isRetryableStatus(408));
    QVERIFY(TestProvider::isRetryableStatus(425));
    QVERIFY(TestProvider::isRetryableStatus(500));
    QVERIFY(TestProvider::isRetryableStatus(502));
    QVERIFY(TestProvider::isRetryableStatus(503));

    QVERIFY(!TestProvider::isRetryableStatus(200));
    QVERIFY(!TestProvider::isRetryableStatus(400));
    QVERIFY(!TestProvider::isRetryableStatus(401));
    QVERIFY(!TestProvider::isRetryableStatus(403));
    QVERIFY(!TestProvider::isRetryableStatus(404));
    QVERIFY(TestProvider::isRetryableStatus(504));
}

void ProviderBackendTest::testEffectiveBaseUrl()
{
    TestProvider p;

    // No custom URL — should return default
    QCOMPARE(p.effectiveBaseUrl("https://api.example.com"), QStringLiteral("https://api.example.com"));

    // Set custom URL — should override
    p.setCustomBaseUrl(QStringLiteral("https://proxy.example.com"));
    QCOMPARE(p.effectiveBaseUrl("https://api.example.com"), QStringLiteral("https://proxy.example.com"));

    // Clear custom URL — back to default
    p.setCustomBaseUrl(QString());
    QCOMPARE(p.effectiveBaseUrl("https://api.example.com"), QStringLiteral("https://api.example.com"));
}

void ProviderBackendTest::testEffectiveBaseUrlTrailingSlash()
{
    TestProvider p;
    p.setCustomBaseUrl(QStringLiteral("https://proxy.example.com///"));
    QCOMPARE(p.effectiveBaseUrl("https://api.example.com"), QStringLiteral("https://proxy.example.com"));
}

void ProviderBackendTest::testTotalTokens()
{
    TestProvider p;
    p.setInputTokens(100);
    p.setOutputTokens(50);
    QCOMPARE(p.totalTokens(), 150);
}

QTEST_MAIN(ProviderBackendTest)
#include "test_providerbackend.moc"
