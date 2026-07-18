#ifndef PROVIDERBACKEND_H
#define PROVIDERBACKEND_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHash>
#include <QList>
#include <QTimer>
#include <QJsonObject>
#include <QVariantList>
#include <functional>

/**
 * Abstract base class for AI provider backends.
 * Exposes usage, rate limits, cost data, and budget tracking to QML.
 * Each provider subclass implements its own API-specific logic.
 *
 * Includes a token-based cost estimation system for providers without
 * billing APIs. Subclasses can register model pricing via registerModelPricing().
 */
class ProviderBackend : public QObject
{
    Q_OBJECT

    // Identity
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString iconName READ iconName CONSTANT)

    // State
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ errorString NOTIFY errorChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY errorChanged)
    Q_PROPERTY(int consecutiveErrors READ consecutiveErrors NOTIFY errorChanged)
    Q_PROPERTY(ProviderState providerState READ providerState NOTIFY stateChanged)
    Q_PROPERTY(ProviderErrorKind errorKind READ errorKind NOTIFY errorChanged)
    Q_PROPERTY(int httpStatus READ httpStatus NOTIFY errorChanged)
    Q_PROPERTY(bool retryable READ isRetryable NOTIFY errorChanged)
    Q_PROPERTY(QDateTime retryAfter READ retryAfter NOTIFY errorChanged)
    Q_PROPERTY(QDateTime nextRetry READ retryAfter NOTIFY errorChanged)
    Q_PROPERTY(RefreshReason lastRefreshReason READ lastRefreshReason NOTIFY stateChanged)
    Q_PROPERTY(QDateTime lastAttempt READ lastAttempt NOTIFY stateChanged)
    Q_PROPERTY(QDateTime lastSuccess READ lastSuccess NOTIFY stateChanged)
    Q_PROPERTY(Freshness freshness READ freshness NOTIFY stateChanged)
    Q_PROPERTY(QDateTime nextScheduledRefresh READ nextScheduledRefresh NOTIFY diagnosticsChanged)
    Q_PROPERTY(int coalescedRefreshCount READ coalescedRefreshCount NOTIFY diagnosticsChanged)
    Q_PROPERTY(int cancellationCount READ cancellationCount NOTIFY diagnosticsChanged)

    // Usage data
    Q_PROPERTY(qint64 inputTokens READ inputTokens NOTIFY dataUpdated)
    Q_PROPERTY(qint64 outputTokens READ outputTokens NOTIFY dataUpdated)
    Q_PROPERTY(qint64 totalTokens READ totalTokens NOTIFY dataUpdated)
    Q_PROPERTY(int requestCount READ requestCount NOTIFY dataUpdated)
    Q_PROPERTY(double cost READ cost NOTIFY dataUpdated)
    Q_PROPERTY(bool isEstimatedCost READ isEstimatedCost NOTIFY dataUpdated)
    Q_PROPERTY(qint64 actualInputTokens READ actualInputTokens NOTIFY dataUpdated)
    Q_PROPERTY(qint64 actualOutputTokens READ actualOutputTokens NOTIFY dataUpdated)
    Q_PROPERTY(qint64 actualTotalTokens READ actualTotalTokens NOTIFY dataUpdated)
    Q_PROPERTY(int actualRequestCount READ actualRequestCount NOTIFY dataUpdated)
    Q_PROPERTY(qint64 probeInputTokens READ probeInputTokens NOTIFY dataUpdated)
    Q_PROPERTY(qint64 probeOutputTokens READ probeOutputTokens NOTIFY dataUpdated)
    Q_PROPERTY(qint64 probeTotalTokens READ probeTotalTokens NOTIFY dataUpdated)
    Q_PROPERTY(int probeRequestCount READ probeRequestCount NOTIFY dataUpdated)
    Q_PROPERTY(QString costSource READ costSource NOTIFY dataUpdated)
    Q_PROPERTY(QString usageSource READ usageSource NOTIFY dataUpdated)
    Q_PROPERTY(QString currency READ currency NOTIFY dataUpdated)
    Q_PROPERTY(QString dataQuality READ dataQuality NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList metrics READ metrics NOTIFY metricsChanged)
    Q_PROPERTY(QVariantMap capabilityStatus READ capabilityStatus NOTIFY capabilityStatusChanged)

    // Rate limits
    Q_PROPERTY(int rateLimitRequests READ rateLimitRequests NOTIFY dataUpdated)
    Q_PROPERTY(int rateLimitTokens READ rateLimitTokens NOTIFY dataUpdated)
    Q_PROPERTY(int rateLimitRequestsRemaining READ rateLimitRequestsRemaining NOTIFY dataUpdated)
    Q_PROPERTY(int rateLimitTokensRemaining READ rateLimitTokensRemaining NOTIFY dataUpdated)
    Q_PROPERTY(QString rateLimitResetTime READ rateLimitResetTime NOTIFY dataUpdated)

    // Budget tracking
    Q_PROPERTY(double dailyBudget READ dailyBudget WRITE setDailyBudget NOTIFY budgetChanged)
    Q_PROPERTY(double monthlyBudget READ monthlyBudget WRITE setMonthlyBudget NOTIFY budgetChanged)
    Q_PROPERTY(double dailyCost READ dailyCost NOTIFY dataUpdated)
    Q_PROPERTY(double monthlyCost READ monthlyCost NOTIFY dataUpdated)
    Q_PROPERTY(double estimatedMonthlyCost READ estimatedMonthlyCost NOTIFY dataUpdated)
    Q_PROPERTY(int budgetWarningPercent READ budgetWarningPercent WRITE setBudgetWarningPercent NOTIFY budgetChanged)
    Q_PROPERTY(QString budgetCurrency READ budgetCurrency WRITE setBudgetCurrency NOTIFY budgetChanged)
    Q_PROPERTY(bool budgetCurrencyMismatch READ budgetCurrencyMismatch NOTIFY budgetChanged)

    // Custom base URL (proxy support)
    Q_PROPERTY(QString customBaseUrl READ customBaseUrl WRITE setCustomBaseUrl NOTIFY customBaseUrlChanged)

    // Metadata
    Q_PROPERTY(QDateTime lastRefreshed READ lastRefreshed NOTIFY dataUpdated)
    Q_PROPERTY(int refreshCount READ refreshCount NOTIFY dataUpdated)

public:
    enum class ProviderState {
        Disabled,
        Unconfigured,
        Idle,
        Refreshing,
        Healthy,
        Stale,
        Degraded,
        Failed
    };
    Q_ENUM(ProviderState)

    enum class ProviderErrorKind {
        None,
        Configuration,
        Authentication,
        Permission,
        RateLimit,
        Timeout,
        Network,
        Server,
        Schema,
        Unsupported,
        Cancelled
    };
    Q_ENUM(ProviderErrorKind)

    enum class RefreshReason {
        Startup,
        Scheduled,
        PopupOpened,
        Manual,
        ConfigurationChanged,
        CredentialChanged,
        Retry
    };
    Q_ENUM(RefreshReason)

    enum class MetricKind {
        InputTokens,
        OutputTokens,
        Requests,
        Cost,
        CreditBalance,
        RequestLimit,
        RequestRemaining,
        TokenLimit,
        TokenRemaining,
        Latency,
        ErrorRate
    };
    Q_ENUM(MetricKind)

    enum class MetricSource {
        BillingApi,
        UsageApi,
        MetricsApi,
        ResponseHeaders,
        PublishedDocumentation,
        LocalObservation,
        EstimatedPricing,
        ConnectivityProbe,
        Estimate = EstimatedPricing,
        StaticDocumentation = PublishedDocumentation,
        SelfTracked = 8,
        BrowserSync = 9
    };
    Q_ENUM(MetricSource)

    enum class Freshness { Fresh, Aging, Stale, Never };
    Q_ENUM(Freshness)

    enum class ProviderId {
        Unknown = 0,
        OpenAI,
        Anthropic,
        Google,
        Mistral,
        DeepSeek,
        Groq,
        XAI,
        OllamaCloud,
        OpenRouter,
        Together,
        Cohere,
        GoogleVeo,
        AzureOpenAI,
        Bedrock
    };
    Q_ENUM(ProviderId)

    struct ProviderConfig {
        ProviderId providerId = ProviderId::Unknown;
        QString providerKey;
        QString baseUrl;
        QString modelId;
        QString deploymentId;
        QString authToken;
        QString authKeySlot;
    };

    struct NormalizedUsageCost {
        bool parsed = false;
        qint64 inputTokens = 0;
        qint64 outputTokens = 0;
        int requestCount = 0;
        double cost = 0.0;
        double dailyCost = 0.0;
        double monthlyCost = 0.0;
    };

    explicit ProviderBackend(QObject *parent = nullptr);
    ~ProviderBackend() override;

    static ProviderId providerIdFromKey(const QString &providerKey);
    static QString providerKeyFromId(ProviderId providerId);
    static QString defaultAuthKeySlotForProvider(ProviderId providerId);
    static ProviderConfig makeProviderConfig(const QString &providerKey,
                                             const QString &baseUrl,
                                             const QString &modelId,
                                             const QString &deploymentId,
                                             const QString &authToken,
                                             const QString &authKeySlot = QString());
    static NormalizedUsageCost normalizeUsageCost(ProviderId providerId, const QJsonObject &payload);

    // Identity
    virtual QString name() const = 0;
    virtual QString iconName() const = 0;

    // State
    bool isConnected() const;
    bool isLoading() const;
    QString errorString() const;
    int errorCount() const;
    int consecutiveErrors() const;
    ProviderState providerState() const;
    ProviderErrorKind errorKind() const;
    int httpStatus() const;
    bool isRetryable() const;
    QDateTime retryAfter() const;
    RefreshReason lastRefreshReason() const;
    QDateTime lastAttempt() const;
    QDateTime lastSuccess() const;
    Freshness freshness() const;
    QDateTime nextScheduledRefresh() const;
    int coalescedRefreshCount() const;
    int cancellationCount() const;

    // Usage data
    qint64 inputTokens() const;
    qint64 outputTokens() const;
    qint64 totalTokens() const;
    int requestCount() const;
    double cost() const;
    bool isEstimatedCost() const;
    qint64 actualInputTokens() const;
    qint64 actualOutputTokens() const;
    qint64 actualTotalTokens() const;
    int actualRequestCount() const;
    qint64 probeInputTokens() const;
    qint64 probeOutputTokens() const;
    qint64 probeTotalTokens() const;
    int probeRequestCount() const;
    QString costSource() const;
    QString usageSource() const;
    QString currency() const;
    QString dataQuality() const;
    QVariantList metrics() const;
    QVariantMap capabilityStatus() const;
    Q_INVOKABLE QVariantMap metric(const QString &kind,
                                   const QString &scope = QString(),
                                   const QString &window = QString()) const;

    // Rate limits
    int rateLimitRequests() const;
    int rateLimitTokens() const;
    int rateLimitRequestsRemaining() const;
    int rateLimitTokensRemaining() const;
    QString rateLimitResetTime() const;

    // Budget
    double dailyBudget() const;
    double monthlyBudget() const;
    void setDailyBudget(double budget);
    void setMonthlyBudget(double budget);
    int budgetWarningPercent() const;
    void setBudgetWarningPercent(int percent);
    double dailyCost() const;
    double monthlyCost() const;
    double estimatedMonthlyCost() const;
    QString budgetCurrency() const;
    void setBudgetCurrency(const QString &currency);
    bool budgetCurrencyMismatch() const;

    // Custom URL
    QString customBaseUrl() const;
    void setCustomBaseUrl(const QString &url);

    // Metadata
    QDateTime lastRefreshed() const;
    int refreshCount() const;

    // API key management
    Q_INVOKABLE void setApiKey(const QString &key);
    Q_INVOKABLE bool hasApiKey() const;

    // Data fetching
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool requestRefresh(RefreshReason reason = RefreshReason::Manual);
    Q_INVOKABLE void cancelRefresh();
    Q_INVOKABLE void setNextScheduledRefresh(const QDateTime &when);

    /// Current request generation. Incremented on each refresh().
    /// Reply handlers should discard results if the generation has advanced.
    Q_INVOKABLE int currentGeneration() const;

Q_SIGNALS:
    void connectedChanged();
    void loadingChanged();
    void errorChanged();
    void dataUpdated();
    void quotaWarning(const QString &provider, int percentUsed);
    void budgetChanged();
    void budgetWarning(const QString &provider, const QString &period, double spent, double budget, const QString &currency);
    void budgetExceeded(const QString &provider, const QString &period, double spent, double budget, const QString &currency);
    void customBaseUrlChanged();
    void stateChanged();
    void diagnosticsChanged();
    void metricsChanged();
    void capabilityStatusChanged();
    void providerDisconnected(const QString &provider);
    void providerReconnected(const QString &provider);

protected:
    virtual void refreshImpl() = 0;

    void setConnected(bool connected);
    void setLoading(bool loading);
    void setError(const QString &error);
    void setErrorDetails(const QString &error,
                         ProviderErrorKind kind,
                         int httpStatus = 0,
                         const QDateTime &retryAfter = QDateTime());
    void setNetworkError(QNetworkReply *reply, const QString &fallbackMessage = QString());
    void clearError();

    QNetworkAccessManager *networkManager() const;
    QString apiKey() const;
    QString effectiveBaseUrl(const char *defaultUrl) const;

    /// Create a QNetworkRequest with standard headers, timeout, and optional auth.
    /// Subclasses can override authStyle for provider-specific headers.
    QNetworkRequest createRequest(const QUrl &url) const;

    /// Parse standard x-ratelimit-* headers from a reply.
    /// @param prefix  Header prefix (e.g. "x-ratelimit-" or "anthropic-ratelimit-")
    void parseRateLimitHeaders(QNetworkReply *reply, const char *prefix = "x-ratelimit-");

    /// Advance the generation counter and abort any in-flight replies.
    /// Call this at the start of refresh() implementations.
    void beginRefresh();

    /// Check if a reply belongs to the current generation.
    /// Returns false if the reply is stale and should be discarded.
    bool isCurrentGeneration(int generation) const;

    /// Check if an HTTP status code is retryable (429, 500, 502, 503).
    static bool isRetryableStatus(int httpStatus);
    static ProviderErrorKind errorKindForNetworkReply(QNetworkReply *reply);
    static QDateTime retryAfterForReply(QNetworkReply *reply, const QDateTime &now = QDateTime::currentDateTimeUtc());

    /// Register a QNetworkReply for tracking. Tracked replies are aborted
    /// by beginRefresh() when a new refresh cycle starts.
    void trackReply(QNetworkReply *reply);

    /// Retry a request with exponential backoff.
    /// @param reply     The failed reply (will be deleteLater'd)
    /// @param url       The URL to retry
    /// @param postBody  If non-empty, sends a POST; otherwise GET
    /// @param callback  Function to call with the new reply
    /// @param attempt   Current attempt number (starts at 1)
    /// @param maxRetries Maximum retry attempts (default 2)
    void retryRequest(QNetworkReply *reply,
                      const QUrl &url,
                      const QByteArray &postBody,
                      std::function<void(QNetworkReply *)> callback,
                      int attempt = 1,
                      int maxRetries = 2);

    // Data setters for subclasses
    void setInputTokens(qint64 tokens);
    void setOutputTokens(qint64 tokens);
    void setRequestCount(int count);
    void setActualUsage(qint64 inputTokens, qint64 outputTokens, int requestCount);
    void setProbeUsage(qint64 inputTokens, qint64 outputTokens, int requestCount);
    void setCostSource(const QString &source);
    void setUsageSource(const QString &source);
    void setCurrency(const QString &currency);
    void setDataQuality(const QString &quality);
    void setCost(double cost);
    void setEstimatedCost(double cost);
    void setDailyCost(double cost);
    void setMonthlyCost(double cost);
    void setRateLimitRequests(int limit);
    void setRateLimitTokens(int limit);
    void setRateLimitRequestsRemaining(int remaining);
    void setRateLimitTokensRemaining(int remaining);
    void setRateLimitResetTime(const QString &time);
    void setProviderMetric(MetricKind kind,
                           const QVariant &value,
                           const QString &unit,
                           const QString &currency,
                           const QString &scope,
                           const QString &window,
                           MetricSource source,
                           const QString &quality,
                           const QDateTime &resetAt = QDateTime(),
                           const QDateTime &periodStart = QDateTime(),
                           const QDateTime &periodEnd = QDateTime());
    void clearProviderMetric(MetricKind kind, const QString &scope = QString(), const QString &window = QString());
    void setCapabilityStatus(const QString &capability,
                             const QString &status,
                             const QString &diagnostic = QString());
    void updateLastRefreshed(const QDateTime &when = QDateTime());

    // Budget checking after cost update
    void checkBudgetLimits();

    // Token-based cost estimation
    struct ModelPricing {
        double inputPricePerMToken;   // $ per 1M input tokens
        double outputPricePerMToken;  // $ per 1M output tokens
    };

    /// Register pricing for a model name. Used for cost estimation when no billing API is available.
    void registerModelPricing(const QString &modelName, double inputPricePerMToken, double outputPricePerMToken);

    /// Register all token pricing rows for a provider from the shipped local catalog.
    void registerCatalogPricing(const QString &providerKey);

    /// Calculate and set estimated cost from accumulated tokens using registered pricing.
    /// Call this after updating token counts. Only sets cost if no real cost has been set.
    void updateEstimatedCost(const QString &currentModel);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_apiKey;
    QString m_customBaseUrl;
    QVariantMap m_capabilityStatus;

    bool m_connected = false;
    bool m_loading = false;
    QString m_error;
    int m_errorCount = 0;
    int m_consecutiveErrors = 0;
    ProviderState m_providerState = ProviderState::Unconfigured;
    ProviderErrorKind m_errorKind = ProviderErrorKind::None;
    int m_httpStatus = 0;
    QDateTime m_retryAfter;
    RefreshReason m_lastRefreshReason = RefreshReason::Startup;
    RefreshReason m_pendingRefreshReason = RefreshReason::Startup;
    QDateTime m_lastAttempt;
    QDateTime m_lastSuccess;
    QDateTime m_nextScheduledRefresh;
    int m_coalescedRefreshCount = 0;
    int m_cancellationCount = 0;

    qint64 m_inputTokens = 0;
    qint64 m_outputTokens = 0;
    int m_requestCount = 0;
    qint64 m_actualInputTokens = 0;
    qint64 m_actualOutputTokens = 0;
    int m_actualRequestCount = 0;
    qint64 m_probeInputTokens = 0;
    qint64 m_probeOutputTokens = 0;
    int m_probeRequestCount = 0;
    double m_cost = 0.0;
    double m_dailyCost = 0.0;
    double m_monthlyCost = 0.0;
    QString m_costSource = QStringLiteral("unknown");
    QString m_usageSource = QStringLiteral("unknown");
    QString m_currency = QStringLiteral("USD");
    QString m_dataQuality = QStringLiteral("unknown");
    QVariantList m_metrics;

    double m_dailyBudget = 0.0;
    double m_monthlyBudget = 0.0;
    int m_budgetWarningPercent = 80;
    QString m_budgetCurrency = QStringLiteral("USD");

    int m_rateLimitRequests = 0;
    int m_rateLimitTokens = 0;
    int m_rateLimitRequestsRemaining = 0;
    int m_rateLimitTokensRemaining = 0;
    QString m_rateLimitResetTime;

    QDateTime m_lastRefreshed;
    int m_refreshCount = 0;
    bool m_wasConnected = false; // for disconnect/reconnect tracking
    bool m_isEstimatedCost = false;

    int m_generation = 0; // incremented on each refresh() to discard stale replies
    QList<QNetworkReply *> m_activeReplies; // tracked for cancellation

    // Budget notification dedup — avoid repeating same alert within a period
    bool m_dailyWarningEmitted = false;
    bool m_dailyExceededEmitted = false;
    bool m_monthlyWarningEmitted = false;
    bool m_monthlyExceededEmitted = false;

    QHash<QString, ModelPricing> m_modelPricing;

    static constexpr int REQUEST_TIMEOUT_MS = 30000; // 30 seconds
};

#endif // PROVIDERBACKEND_H
