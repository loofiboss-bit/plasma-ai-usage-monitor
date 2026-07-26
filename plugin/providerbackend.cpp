#include "providerbackend.h"
#include "providerpricingcatalog.h"
#include <QDate>
#include <QUrl>
#include <QRandomGenerator>
#include <QSet>
#include <QLocale>
#include <QTimeZone>
#include <QMetaEnum>
#include <QRegularExpression>

namespace {
ProviderBackend::NormalizedUsageCost normalizeOpenAiLikeUsage(const QJsonObject &payload)
{
    ProviderBackend::NormalizedUsageCost normalized;

    const QJsonObject usage = payload.value(QStringLiteral("usage")).toObject();
    if (usage.isEmpty()) {
        return normalized;
    }

    const qint64 promptTokens = usage.value(QStringLiteral("prompt_tokens")).toInteger(0);
    const qint64 completionTokens = usage.value(QStringLiteral("completion_tokens")).toInteger(0);
    const qint64 totalTokens = usage.value(QStringLiteral("total_tokens")).toInteger(promptTokens + completionTokens);

    normalized.parsed = true;
    normalized.inputTokens = promptTokens;
    normalized.outputTokens = completionTokens > 0 ? completionTokens : qMax<qint64>(0, totalTokens - promptTokens);
    normalized.requestCount = 1;

    const QJsonObject cost = payload.value(QStringLiteral("cost")).toObject();
    if (!cost.isEmpty()) {
        normalized.cost = cost.value(QStringLiteral("total_cost")).toDouble(normalized.cost);
        normalized.dailyCost = cost.value(QStringLiteral("daily_cost")).toDouble(normalized.dailyCost);
        normalized.monthlyCost = cost.value(QStringLiteral("monthly_cost")).toDouble(normalized.monthlyCost);
    }

    if (qFuzzyIsNull(normalized.dailyCost)) {
        normalized.dailyCost = normalized.cost;
    }
    if (qFuzzyIsNull(normalized.monthlyCost)) {
        normalized.monthlyCost = normalized.cost;
    }

    return normalized;
}
} // namespace

ProviderBackend::ProviderBackend(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

ProviderBackend::~ProviderBackend() = default;

ProviderBackend::ProviderId ProviderBackend::providerIdFromKey(const QString &providerKey)
{
    const QString normalized = providerKey.trimmed().toLower();

    if (normalized == QLatin1String("openai")) return ProviderId::OpenAI;
    if (normalized == QLatin1String("anthropic")) return ProviderId::Anthropic;
    if (normalized == QLatin1String("google") || normalized == QLatin1String("google-gemini")) return ProviderId::Google;
    if (normalized == QLatin1String("mistral")) return ProviderId::Mistral;
    if (normalized == QLatin1String("deepseek")) return ProviderId::DeepSeek;
    if (normalized == QLatin1String("groq")) return ProviderId::Groq;
    if (normalized == QLatin1String("xai") || normalized == QLatin1String("x-ai")) return ProviderId::XAI;
    if (normalized == QLatin1String("ollama") || normalized == QLatin1String("ollama-cloud")
        || normalized == QLatin1String("ollama_cloud")) {
        return ProviderId::OllamaCloud;
    }
    if (normalized == QLatin1String("openrouter")) return ProviderId::OpenRouter;
    if (normalized == QLatin1String("together")) return ProviderId::Together;
    if (normalized == QLatin1String("cohere")) return ProviderId::Cohere;
    if (normalized == QLatin1String("google-veo") || normalized == QLatin1String("veo")) return ProviderId::GoogleVeo;
    if (normalized == QLatin1String("azure") || normalized == QLatin1String("azure-openai")
        || normalized == QLatin1String("azure_openai")) {
        return ProviderId::AzureOpenAI;
    }
    if (normalized == QLatin1String("bedrock") || normalized == QLatin1String("aws-bedrock")
        || normalized == QLatin1String("aws_bedrock")) {
        return ProviderId::Bedrock;
    }

    return ProviderId::Unknown;
}

QString ProviderBackend::providerKeyFromId(ProviderId providerId)
{
    switch (providerId) {
    case ProviderId::OpenAI: return QStringLiteral("openai");
    case ProviderId::Anthropic: return QStringLiteral("anthropic");
    case ProviderId::Google: return QStringLiteral("google");
    case ProviderId::Mistral: return QStringLiteral("mistral");
    case ProviderId::DeepSeek: return QStringLiteral("deepseek");
    case ProviderId::Groq: return QStringLiteral("groq");
    case ProviderId::XAI: return QStringLiteral("xai");
    case ProviderId::OllamaCloud: return QStringLiteral("ollama");
    case ProviderId::OpenRouter: return QStringLiteral("openrouter");
    case ProviderId::Together: return QStringLiteral("together");
    case ProviderId::Cohere: return QStringLiteral("cohere");
    case ProviderId::GoogleVeo: return QStringLiteral("google-veo");
    case ProviderId::AzureOpenAI: return QStringLiteral("azure-openai");
    case ProviderId::Bedrock: return QStringLiteral("bedrock");
    case ProviderId::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

QString ProviderBackend::defaultAuthKeySlotForProvider(ProviderId providerId)
{
    if (providerId == ProviderId::AzureOpenAI) {
        return QStringLiteral("azure_openai_api_key");
    }
    return providerKeyFromId(providerId) + QStringLiteral("_api_key");
}

ProviderBackend::ProviderConfig ProviderBackend::makeProviderConfig(const QString &providerKey,
                                                                    const QString &baseUrl,
                                                                    const QString &modelId,
                                                                    const QString &deploymentId,
                                                                    const QString &authToken,
                                                                    const QString &authKeySlot)
{
    ProviderConfig config;
    config.providerId = providerIdFromKey(providerKey);
    config.providerKey = providerKeyFromId(config.providerId);
    config.baseUrl = baseUrl.trimmed();
    config.modelId = modelId.trimmed();
    config.deploymentId = deploymentId.trimmed();
    config.authToken = authToken;
    config.authKeySlot = authKeySlot.trimmed();

    if (config.authKeySlot.isEmpty()) {
        config.authKeySlot = defaultAuthKeySlotForProvider(config.providerId);
    }

    return config;
}

ProviderBackend::NormalizedUsageCost ProviderBackend::normalizeUsageCost(ProviderId providerId, const QJsonObject &payload)
{
    switch (providerId) {
    case ProviderId::OpenAI:
    case ProviderId::Mistral:
    case ProviderId::DeepSeek:
    case ProviderId::Groq:
    case ProviderId::XAI:
    case ProviderId::OllamaCloud:
    case ProviderId::OpenRouter:
    case ProviderId::Together:
    case ProviderId::Cohere:
    case ProviderId::GoogleVeo:
    case ProviderId::AzureOpenAI:
    case ProviderId::Bedrock:
        return normalizeOpenAiLikeUsage(payload);
    case ProviderId::Anthropic:
    case ProviderId::Google:
    case ProviderId::Unknown:
    default:
        return NormalizedUsageCost{};
    }
}

// --- State ---

bool ProviderBackend::isConnected() const { return m_connected; }
bool ProviderBackend::isLoading() const { return m_loading; }
QString ProviderBackend::errorString() const { return m_error; }
int ProviderBackend::errorCount() const { return m_errorCount; }
int ProviderBackend::consecutiveErrors() const { return m_consecutiveErrors; }
ProviderBackend::ProviderState ProviderBackend::providerState() const { return m_providerState; }
ProviderBackend::ProviderErrorKind ProviderBackend::errorKind() const { return m_errorKind; }
int ProviderBackend::httpStatus() const { return m_httpStatus; }
QDateTime ProviderBackend::retryAfter() const { return m_retryAfter; }
ProviderBackend::RefreshReason ProviderBackend::lastRefreshReason() const { return m_lastRefreshReason; }
QDateTime ProviderBackend::lastAttempt() const { return m_lastAttempt; }
QDateTime ProviderBackend::lastSuccess() const { return m_lastSuccess; }
ProviderBackend::Freshness ProviderBackend::freshness() const
{
    if (!m_lastSuccess.isValid()) return Freshness::Never;
    const qint64 age = m_lastSuccess.secsTo(QDateTime::currentDateTimeUtc());
    if (age < 5 * 60) return Freshness::Fresh;
    if (age < 15 * 60) return Freshness::Aging;
    return Freshness::Stale;
}
QDateTime ProviderBackend::nextScheduledRefresh() const { return m_nextScheduledRefresh; }
int ProviderBackend::coalescedRefreshCount() const { return m_coalescedRefreshCount; }
int ProviderBackend::cancellationCount() const { return m_cancellationCount; }

bool ProviderBackend::isRetryable() const
{
    return m_errorKind == ProviderErrorKind::RateLimit
        || m_errorKind == ProviderErrorKind::Timeout
        || m_errorKind == ProviderErrorKind::Network
        || m_errorKind == ProviderErrorKind::Server
        || isRetryableStatus(m_httpStatus);
}

void ProviderBackend::setConnected(bool connected)
{
    if (m_connected != connected) {
        bool wasConnected = m_connected;
        m_connected = connected;
        Q_EMIT connectedChanged();

        // Track disconnect/reconnect events
        if (wasConnected && !connected) {
            Q_EMIT providerDisconnected(name());
        } else if (!wasConnected && connected && m_wasConnected) {
            Q_EMIT providerReconnected(name());
        }
        m_wasConnected = m_wasConnected || connected;
    }
}

void ProviderBackend::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        const ProviderState nextState = loading
            ? ProviderState::Refreshing
            : (m_connected ? (m_error.isEmpty() ? ProviderState::Healthy : ProviderState::Degraded)
                           : (m_error.isEmpty() ? ProviderState::Idle : ProviderState::Failed));
        if (m_providerState != nextState) {
            m_providerState = nextState;
            Q_EMIT stateChanged();
        }
        Q_EMIT loadingChanged();
    }
}

void ProviderBackend::setError(const QString &error)
{
    setErrorDetails(error, ProviderErrorKind::Server);
}

void ProviderBackend::setErrorDetails(const QString &error,
                                      ProviderErrorKind kind,
                                      int httpStatus,
                                      const QDateTime &retryAfter)
{
    m_error = error;
    m_errorKind = kind;
    m_httpStatus = httpStatus;
    m_retryAfter = retryAfter;
    m_errorCount++;
    m_consecutiveErrors++;
    const ProviderState nextState = m_connected ? ProviderState::Degraded : ProviderState::Failed;
    if (m_providerState != nextState) {
        m_providerState = nextState;
        Q_EMIT stateChanged();
    }
    Q_EMIT errorChanged();
}

void ProviderBackend::setNetworkError(QNetworkReply *reply, const QString &fallbackMessage)
{
    const int status = reply
        ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
    QString message = fallbackMessage;
    if (message.isEmpty() && reply) {
        message = reply->errorString();
    }
    if (message.isEmpty()) {
        message = QStringLiteral("Provider request failed");
    }
    setErrorDetails(message, errorKindForNetworkReply(reply), status,
                    retryAfterForReply(reply));
}

void ProviderBackend::clearError()
{
    if (!m_error.isEmpty()) {
        m_error.clear();
        m_errorKind = ProviderErrorKind::None;
        m_httpStatus = 0;
        m_retryAfter = QDateTime();
        m_consecutiveErrors = 0;
        if (m_connected) {
            m_lastSuccess = QDateTime::currentDateTimeUtc();
            if (m_providerState != ProviderState::Healthy) {
                m_providerState = ProviderState::Healthy;
                Q_EMIT stateChanged();
            }
        }
        Q_EMIT errorChanged();
    }
}

// --- Usage Data ---

qint64 ProviderBackend::inputTokens() const { return m_inputTokens; }
qint64 ProviderBackend::outputTokens() const { return m_outputTokens; }
qint64 ProviderBackend::totalTokens() const { return m_inputTokens + m_outputTokens; }
int ProviderBackend::requestCount() const { return m_requestCount; }
double ProviderBackend::cost() const { return m_cost; }
bool ProviderBackend::isEstimatedCost() const { return m_isEstimatedCost; }
qint64 ProviderBackend::actualInputTokens() const { return m_actualInputTokens; }
qint64 ProviderBackend::actualOutputTokens() const { return m_actualOutputTokens; }
qint64 ProviderBackend::actualTotalTokens() const { return m_actualInputTokens + m_actualOutputTokens; }
int ProviderBackend::actualRequestCount() const { return m_actualRequestCount; }
qint64 ProviderBackend::probeInputTokens() const { return m_probeInputTokens; }
qint64 ProviderBackend::probeOutputTokens() const { return m_probeOutputTokens; }
qint64 ProviderBackend::probeTotalTokens() const { return m_probeInputTokens + m_probeOutputTokens; }
int ProviderBackend::probeRequestCount() const { return m_probeRequestCount; }
QString ProviderBackend::costSource() const { return m_costSource; }
QString ProviderBackend::usageSource() const { return m_usageSource; }
QString ProviderBackend::currency() const { return m_currency; }
QString ProviderBackend::dataQuality() const { return m_dataQuality; }
QVariantList ProviderBackend::metrics() const { return m_metrics; }
QVariantMap ProviderBackend::capabilityStatus() const { return m_capabilityStatus; }

QVariantMap ProviderBackend::metric(const QString &kind,
                                    const QString &scope,
                                    const QString &window) const
{
    for (auto it = m_metrics.crbegin(); it != m_metrics.crend(); ++it) {
        const QVariantMap candidate = it->toMap();
        if (candidate.value(QStringLiteral("kind")).toString() != kind) continue;
        if (!scope.isEmpty() && candidate.value(QStringLiteral("scope")).toString() != scope) continue;
        if (!window.isEmpty() && candidate.value(QStringLiteral("window")).toString() != window) continue;
        return candidate;
    }
    return {};
}

void ProviderBackend::setInputTokens(qint64 tokens)
{
    m_inputTokens = tokens;
    m_actualInputTokens = tokens;
}

void ProviderBackend::setOutputTokens(qint64 tokens)
{
    m_outputTokens = tokens;
    m_actualOutputTokens = tokens;
}

void ProviderBackend::setRequestCount(int count)
{
    m_requestCount = count;
    m_actualRequestCount = count;
}

void ProviderBackend::setActualUsage(qint64 inputTokens, qint64 outputTokens, int requestCount)
{
    m_actualInputTokens = qMax<qint64>(0, inputTokens);
    m_actualOutputTokens = qMax<qint64>(0, outputTokens);
    m_actualRequestCount = qMax(0, requestCount);
    m_inputTokens = m_actualInputTokens;
    m_outputTokens = m_actualOutputTokens;
    m_requestCount = m_actualRequestCount;
    setProviderMetric(MetricKind::InputTokens, m_actualInputTokens, QStringLiteral("token"), QString(),
                      QStringLiteral("api_key"), QStringLiteral("current"), MetricSource::UsageApi, QStringLiteral("actual"));
    setProviderMetric(MetricKind::OutputTokens, m_actualOutputTokens, QStringLiteral("token"), QString(),
                      QStringLiteral("api_key"), QStringLiteral("current"), MetricSource::UsageApi, QStringLiteral("actual"));
    setProviderMetric(MetricKind::Requests, m_actualRequestCount, QStringLiteral("request"), QString(),
                      QStringLiteral("api_key"), QStringLiteral("current"), MetricSource::UsageApi, QStringLiteral("actual"));
}

void ProviderBackend::setProbeUsage(qint64 inputTokens, qint64 outputTokens, int requestCount)
{
    m_probeInputTokens = qMax<qint64>(0, inputTokens);
    m_probeOutputTokens = qMax<qint64>(0, outputTokens);
    m_probeRequestCount = qMax(0, requestCount);
}

void ProviderBackend::setCostSource(const QString &source)
{
    static const QSet<QString> allowedSources = {
        QStringLiteral("actual_api"),
        QStringLiteral("billing_api"),
        QStringLiteral("usage_api"),
        QStringLiteral("estimated_from_usage"),
        QStringLiteral("connectivity_probe"),
        QStringLiteral("self_tracked"),
        QStringLiteral("browser_sync"),
        QStringLiteral("unknown"),
    };
    const QString normalized = source.trimmed().isEmpty() ? QStringLiteral("unknown") : source.trimmed();
    m_costSource = allowedSources.contains(normalized) ? normalized : QStringLiteral("unknown");
    const QString typedSource = m_costSource == QLatin1String("actual_api") ? QStringLiteral("usage_api")
        : m_costSource == QLatin1String("estimated_from_usage") ? QStringLiteral("estimated_pricing")
        : m_costSource;
    bool changed = false;
    for (QVariant &entry : m_metrics) {
        QVariantMap metric = entry.toMap();
        if (metric.value(QStringLiteral("kind")).toString() != QLatin1String("cost")) continue;
        if (metric.value(QStringLiteral("source")).toString() == typedSource) continue;
        metric.insert(QStringLiteral("source"), typedSource);
        entry = metric;
        changed = true;
    }
    if (changed) Q_EMIT metricsChanged();
}

void ProviderBackend::setUsageSource(const QString &source)
{
    static const QSet<QString> allowedSources = {
        QStringLiteral("actual_api"),
        QStringLiteral("billing_api"),
        QStringLiteral("usage_api"),
        QStringLiteral("model_discovery_api"),
        QStringLiteral("connectivity_read_only"),
        QStringLiteral("estimated_from_usage"),
        QStringLiteral("connectivity_probe"),
        QStringLiteral("self_tracked"),
        QStringLiteral("browser_sync"),
        QStringLiteral("unknown"),
    };
    const QString normalized = source.trimmed().isEmpty() ? QStringLiteral("unknown") : source.trimmed();
    m_usageSource = allowedSources.contains(normalized) ? normalized : QStringLiteral("unknown");
    const QString typedSource = m_usageSource == QLatin1String("actual_api") ? QStringLiteral("usage_api")
        : m_usageSource == QLatin1String("model_discovery_api") ? QStringLiteral("connectivity_probe")
        : m_usageSource == QLatin1String("connectivity_read_only") ? QStringLiteral("connectivity_probe")
        : m_usageSource == QLatin1String("estimated_from_usage") ? QStringLiteral("estimated_pricing")
        : m_usageSource;
    bool changed = false;
    for (QVariant &entry : m_metrics) {
        QVariantMap metric = entry.toMap();
        const QString kind = metric.value(QStringLiteral("kind")).toString();
        if (kind != QLatin1String("input_tokens") && kind != QLatin1String("output_tokens")
            && kind != QLatin1String("requests")) continue;
        if (metric.value(QStringLiteral("source")).toString() == typedSource) continue;
        metric.insert(QStringLiteral("source"), typedSource);
        entry = metric;
        changed = true;
    }
    if (changed) Q_EMIT metricsChanged();
}

void ProviderBackend::setCurrency(const QString &currency)
{
    const QString normalized = currency.trimmed().toUpper();
    const QString next = normalized.isEmpty() ? QStringLiteral("USD") : normalized;
    const bool mismatchBefore = budgetCurrencyMismatch();
    m_currency = next;
    if (mismatchBefore != budgetCurrencyMismatch()) {
        Q_EMIT budgetChanged();
    }
}

void ProviderBackend::setDataQuality(const QString &quality)
{
    const QString normalized = quality.trimmed();
    m_dataQuality = normalized.isEmpty() ? QStringLiteral("unknown") : normalized;
}

void ProviderBackend::setCost(double cost) {
    m_cost = cost;
    m_isEstimatedCost = false;
    setCostSource(QStringLiteral("actual_api"));
    setProviderMetric(MetricKind::Cost, cost, m_currency, m_currency, QStringLiteral("api_key"),
                      QStringLiteral("current"), MetricSource::UsageApi, QStringLiteral("actual"));
    checkBudgetLimits();
}

// --- Budget ---

double ProviderBackend::dailyBudget() const { return m_dailyBudget; }
double ProviderBackend::monthlyBudget() const { return m_monthlyBudget; }
double ProviderBackend::dailyCost() const { return m_dailyCost; }
double ProviderBackend::monthlyCost() const { return m_monthlyCost; }

double ProviderBackend::estimatedMonthlyCost() const
{
    if (m_dailyCost <= 0 && m_monthlyCost <= 0) return 0.0;
    int dayOfMonth = QDate::currentDate().day();
    int daysInMonth = QDate::currentDate().daysInMonth();
    if (dayOfMonth == 0) return 0.0;

    // If we have real monthly cost data (e.g. OpenAI billing API), project it
    if (m_monthlyCost > 0) {
        return (m_monthlyCost / dayOfMonth) * daysInMonth;
    }
    // Fallback for estimated-cost providers: project daily cost to full month
    return m_dailyCost * daysInMonth;
}

QString ProviderBackend::budgetCurrency() const { return m_budgetCurrency; }
bool ProviderBackend::budgetCurrencyMismatch() const
{
    return !m_currency.isEmpty() && !m_budgetCurrency.isEmpty()
        && m_currency.compare(m_budgetCurrency, Qt::CaseInsensitive) != 0;
}

void ProviderBackend::setBudgetCurrency(const QString &currency)
{
    const QString normalized = currency.trimmed().toUpper();
    if (!normalized.isEmpty() && m_budgetCurrency != normalized) {
        m_budgetCurrency = normalized;
        Q_EMIT budgetChanged();
    }
}

void ProviderBackend::setDailyBudget(double budget)
{
    if (m_dailyBudget != budget) {
        m_dailyBudget = budget;
        Q_EMIT budgetChanged();
    }
}

void ProviderBackend::setMonthlyBudget(double budget)
{
    if (m_monthlyBudget != budget) {
        m_monthlyBudget = budget;
        Q_EMIT budgetChanged();
    }
}

int ProviderBackend::budgetWarningPercent() const { return m_budgetWarningPercent; }
void ProviderBackend::setBudgetWarningPercent(int percent)
{
    if (m_budgetWarningPercent != percent) {
        m_budgetWarningPercent = percent;
        Q_EMIT budgetChanged();
    }
}

void ProviderBackend::setDailyCost(double cost) {
    m_dailyCost = cost;
    checkBudgetLimits();
}
void ProviderBackend::setMonthlyCost(double cost) {
    m_monthlyCost = cost;
    checkBudgetLimits();
}

void ProviderBackend::checkBudgetLimits()
{
    if (budgetCurrencyMismatch()) {
        return;
    }
    double warningFraction = m_budgetWarningPercent / 100.0;

    // Daily budget checks
    if (m_dailyBudget > 0) {
        if (m_dailyCost >= m_dailyBudget && !m_dailyExceededEmitted) {
            m_dailyExceededEmitted = true;
            Q_EMIT budgetExceeded(name(), QStringLiteral("daily"), m_dailyCost, m_dailyBudget, m_currency);
        } else if (m_dailyCost >= m_dailyBudget * warningFraction && !m_dailyWarningEmitted) {
            m_dailyWarningEmitted = true;
            Q_EMIT budgetWarning(name(), QStringLiteral("daily"), m_dailyCost, m_dailyBudget, m_currency);
        }
        // Reset flags when cost drops (new billing period)
        if (m_dailyCost < m_dailyBudget * warningFraction) {
            m_dailyWarningEmitted = false;
            m_dailyExceededEmitted = false;
        }
    }

    // Monthly budget checks
    if (m_monthlyBudget > 0) {
        if (m_monthlyCost >= m_monthlyBudget && !m_monthlyExceededEmitted) {
            m_monthlyExceededEmitted = true;
            Q_EMIT budgetExceeded(name(), QStringLiteral("monthly"), m_monthlyCost, m_monthlyBudget, m_currency);
        } else if (m_monthlyCost >= m_monthlyBudget * warningFraction && !m_monthlyWarningEmitted) {
            m_monthlyWarningEmitted = true;
            Q_EMIT budgetWarning(name(), QStringLiteral("monthly"), m_monthlyCost, m_monthlyBudget, m_currency);
        }
        // Reset flags when cost drops (new billing period)
        if (m_monthlyCost < m_monthlyBudget * warningFraction) {
            m_monthlyWarningEmitted = false;
            m_monthlyExceededEmitted = false;
        }
    }
}

// --- Rate Limits ---

int ProviderBackend::rateLimitRequests() const { return m_rateLimitRequests; }
int ProviderBackend::rateLimitTokens() const { return m_rateLimitTokens; }
int ProviderBackend::rateLimitRequestsRemaining() const { return m_rateLimitRequestsRemaining; }
int ProviderBackend::rateLimitTokensRemaining() const { return m_rateLimitTokensRemaining; }
QString ProviderBackend::rateLimitResetTime() const { return m_rateLimitResetTime; }

void ProviderBackend::setRateLimitRequests(int limit) {
    m_rateLimitRequests = qMax(0, limit);
    if (limit > 0) setProviderMetric(MetricKind::RequestLimit, limit, QStringLiteral("request"), QString(), QStringLiteral("api_key"), QStringLiteral("minute"), MetricSource::ResponseHeaders, QStringLiteral("actual"));
    else clearProviderMetric(MetricKind::RequestLimit, QStringLiteral("api_key"), QStringLiteral("minute"));
}
void ProviderBackend::setRateLimitTokens(int limit) {
    m_rateLimitTokens = qMax(0, limit);
    if (limit > 0) setProviderMetric(MetricKind::TokenLimit, limit, QStringLiteral("token"), QString(), QStringLiteral("api_key"), QStringLiteral("minute"), MetricSource::ResponseHeaders, QStringLiteral("actual"));
    else clearProviderMetric(MetricKind::TokenLimit, QStringLiteral("api_key"), QStringLiteral("minute"));
}
void ProviderBackend::setRateLimitRequestsRemaining(int remaining) {
    m_rateLimitRequestsRemaining = qMax(0, remaining);
    if (m_rateLimitRequests > 0) setProviderMetric(MetricKind::RequestRemaining, m_rateLimitRequestsRemaining, QStringLiteral("request"), QString(), QStringLiteral("api_key"), QStringLiteral("minute"), MetricSource::ResponseHeaders, QStringLiteral("actual"));
    else clearProviderMetric(MetricKind::RequestRemaining, QStringLiteral("api_key"), QStringLiteral("minute"));
}
void ProviderBackend::setRateLimitTokensRemaining(int remaining) {
    m_rateLimitTokensRemaining = qMax(0, remaining);
    if (m_rateLimitTokens > 0) setProviderMetric(MetricKind::TokenRemaining, m_rateLimitTokensRemaining, QStringLiteral("token"), QString(), QStringLiteral("api_key"), QStringLiteral("minute"), MetricSource::ResponseHeaders, QStringLiteral("actual"));
    else clearProviderMetric(MetricKind::TokenRemaining, QStringLiteral("api_key"), QStringLiteral("minute"));
}
void ProviderBackend::setRateLimitResetTime(const QString &time)
{
    m_rateLimitResetTime = time;
    QDateTime reset = QDateTime::fromString(time, Qt::ISODate);
    if (!reset.isValid()) {
        static const QRegularExpression relative(QStringLiteral("^([0-9]+)(ms|s|m|h)$"));
        const QRegularExpressionMatch match = relative.match(time.trimmed());
        if (match.hasMatch()) {
            const qint64 amount = match.captured(1).toLongLong();
            const QString unit = match.captured(2);
            qint64 milliseconds = amount;
            if (unit == QLatin1String("s")) milliseconds *= 1000;
            else if (unit == QLatin1String("m")) milliseconds *= 60 * 1000;
            else if (unit == QLatin1String("h")) milliseconds *= 60 * 60 * 1000;
            reset = QDateTime::currentDateTimeUtc().addMSecs(milliseconds);
        }
    }
    if (!reset.isValid()) return;
    bool changed = false;
    for (QVariant &entry : m_metrics) {
        QVariantMap metric = entry.toMap();
        const QString kind = metric.value(QStringLiteral("kind")).toString();
        if (kind != QLatin1String("request_limit") && kind != QLatin1String("request_remaining")
            && kind != QLatin1String("token_limit") && kind != QLatin1String("token_remaining")) continue;
        metric.insert(QStringLiteral("resetAt"), reset.toUTC());
        entry = metric;
        changed = true;
    }
    if (changed) Q_EMIT metricsChanged();
}

void ProviderBackend::setProviderMetric(MetricKind kind,
                                        const QVariant &value,
                                        const QString &unit,
                                        const QString &currency,
                                        const QString &scope,
                                        const QString &window,
                                        MetricSource source,
                                        const QString &quality,
                                        const QDateTime &resetAt,
                                        const QDateTime &periodStart,
                                        const QDateTime &periodEnd,
                                        const QString &modelScope,
                                        const QString &projectScope)
{
    const QString kindName = [kind]() {
        switch (kind) {
        case MetricKind::InputTokens: return QStringLiteral("input_tokens");
        case MetricKind::CacheReadInputTokens: return QStringLiteral("cache_read_input_tokens");
        case MetricKind::CacheCreationInputTokens: return QStringLiteral("cache_creation_input_tokens");
        case MetricKind::OutputTokens: return QStringLiteral("output_tokens");
        case MetricKind::Requests: return QStringLiteral("requests");
        case MetricKind::Cost: return QStringLiteral("cost");
        case MetricKind::CreditBalance: return QStringLiteral("credit_balance");
        case MetricKind::RequestLimit: return QStringLiteral("request_limit");
        case MetricKind::RequestRemaining: return QStringLiteral("request_remaining");
        case MetricKind::TokenLimit: return QStringLiteral("token_limit");
        case MetricKind::TokenRemaining: return QStringLiteral("token_remaining");
        case MetricKind::Latency: return QStringLiteral("latency");
        case MetricKind::ErrorRate: return QStringLiteral("error_rate");
        }
        return QStringLiteral("unknown");
    }();
    const QString sourceName = [source]() {
        switch (source) {
        case MetricSource::BillingApi: return QStringLiteral("billing_api");
        case MetricSource::UsageApi: return QStringLiteral("usage_api");
        case MetricSource::MetricsApi: return QStringLiteral("metrics_api");
        case MetricSource::ResponseHeaders: return QStringLiteral("response_headers");
        case MetricSource::PublishedDocumentation: return QStringLiteral("published_documentation");
        case MetricSource::LocalObservation: return QStringLiteral("local_observation");
        case MetricSource::EstimatedPricing: return QStringLiteral("estimated_pricing");
        case MetricSource::ConnectivityProbe: return QStringLiteral("connectivity_probe");
        case MetricSource::SelfTracked: return QStringLiteral("self_tracked");
        case MetricSource::BrowserSync: return QStringLiteral("browser_sync");
        }
        return QStringLiteral("unknown");
    }();
    for (qsizetype i = m_metrics.size() - 1; i >= 0; --i) {
        const QVariantMap current = m_metrics.at(i).toMap();
        if (current.value(QStringLiteral("kind")).toString() == kindName
            && current.value(QStringLiteral("scope")).toString() == scope
            && current.value(QStringLiteral("window")).toString() == window
            && current.value(QStringLiteral("modelScope")).toString() == modelScope
            && current.value(QStringLiteral("projectScope")).toString() == projectScope
            && current.value(QStringLiteral("periodStart")).toDateTime() == periodStart
            && current.value(QStringLiteral("periodEnd")).toDateTime() == periodEnd
            && (currency.isEmpty()
                || current.value(QStringLiteral("currency")).toString() == currency)) {
            m_metrics.removeAt(i);
        }
    }

    QVariantMap metric;
    metric.insert(QStringLiteral("kind"), kindName);
    metric.insert(QStringLiteral("value"), value);
    metric.insert(QStringLiteral("available"), value.isValid() && !value.isNull());
    metric.insert(QStringLiteral("unit"), unit);
    metric.insert(QStringLiteral("currency"), currency);
    metric.insert(QStringLiteral("scope"), scope);
    metric.insert(QStringLiteral("window"), window);
    metric.insert(QStringLiteral("source"), sourceName);
    metric.insert(QStringLiteral("quality"), quality);
    metric.insert(QStringLiteral("observedAt"), QDateTime::currentDateTimeUtc());
    metric.insert(QStringLiteral("resetAt"), resetAt);
    metric.insert(QStringLiteral("periodStart"), periodStart);
    metric.insert(QStringLiteral("periodEnd"), periodEnd);
    if (!modelScope.isEmpty()) metric.insert(QStringLiteral("modelScope"), modelScope);
    if (!projectScope.isEmpty()) metric.insert(QStringLiteral("projectScope"), projectScope);
    m_metrics.append(metric);
    Q_EMIT metricsChanged();
}

void ProviderBackend::markProviderMetricsStale(MetricSource source,
                                               const QString &diagnostic)
{
    const QString sourceName = [source]() {
        switch (source) {
        case MetricSource::BillingApi: return QStringLiteral("billing_api");
        case MetricSource::UsageApi: return QStringLiteral("usage_api");
        case MetricSource::MetricsApi: return QStringLiteral("metrics_api");
        case MetricSource::ResponseHeaders: return QStringLiteral("response_headers");
        case MetricSource::PublishedDocumentation: return QStringLiteral("published_documentation");
        case MetricSource::LocalObservation: return QStringLiteral("local_observation");
        case MetricSource::EstimatedPricing: return QStringLiteral("estimated_pricing");
        case MetricSource::ConnectivityProbe: return QStringLiteral("connectivity_probe");
        case MetricSource::SelfTracked: return QStringLiteral("self_tracked");
        case MetricSource::BrowserSync: return QStringLiteral("browser_sync");
        }
        return QStringLiteral("unknown");
    }();
    bool changed = false;
    for (QVariant &entry : m_metrics) {
        QVariantMap metric = entry.toMap();
        if (metric.value(QStringLiteral("source")).toString() != sourceName) continue;
        metric.insert(QStringLiteral("quality"), QStringLiteral("stale"));
        if (!diagnostic.isEmpty()) metric.insert(QStringLiteral("diagnostic"), diagnostic);
        entry = metric;
        changed = true;
    }
    if (changed) Q_EMIT metricsChanged();
}

void ProviderBackend::setCapabilityStatus(const QString &capability,
                                          const QString &status,
                                          const QString &diagnostic)
{
    if (capability.trimmed().isEmpty()) return;
    QVariantMap value;
    value.insert(QStringLiteral("status"), status.trimmed().isEmpty()
                     ? QStringLiteral("unknown") : status.trimmed());
    value.insert(QStringLiteral("diagnostic"), diagnostic);
    value.insert(QStringLiteral("observedAt"), QDateTime::currentDateTimeUtc());
    if (m_capabilityStatus.value(capability).toMap() == value) return;
    m_capabilityStatus.insert(capability, value);
    Q_EMIT capabilityStatusChanged();
}

void ProviderBackend::clearProviderMetric(MetricKind kind, const QString &scope, const QString &window)
{
    setProviderMetric(kind, QVariant(), QString(), QString(), scope, window,
                      MetricSource::LocalObservation, QStringLiteral("unknown"));
}

// --- Custom URL ---

QString ProviderBackend::customBaseUrl() const { return m_customBaseUrl; }
void ProviderBackend::setCustomBaseUrl(const QString &url)
{
    QString normalized = url.trimmed();
    while (normalized.endsWith(QLatin1Char('/'))) normalized.chop(1);
    if (!normalized.isEmpty()) {
        const QUrl parsed(normalized);
        const bool loopbackHttp = parsed.scheme() == QLatin1String("http")
            && (parsed.host() == QLatin1String("localhost")
                || parsed.host() == QLatin1String("127.0.0.1")
                || parsed.host() == QLatin1String("::1"));
        if (parsed.scheme() != QLatin1String("https") && !loopbackHttp) {
            qWarning() << "ProviderBackend:" << name()
                       << "rejected a non-HTTPS non-loopback custom endpoint";
            normalized.clear();
        }
    }
    if (m_customBaseUrl != normalized) {
        m_customBaseUrl = normalized;
        Q_EMIT customBaseUrlChanged();
    }
}

QString ProviderBackend::effectiveBaseUrl(const char *defaultUrl) const
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

    if (!m_customBaseUrl.isEmpty()) {
        // Remove trailing slash for consistency
        QString url = m_customBaseUrl;
        while (url.endsWith(QLatin1Char('/'))) {
            url.chop(1);
        }
        return url;
    }
    return QLatin1String(defaultUrl);
}

// --- Metadata ---

QDateTime ProviderBackend::lastRefreshed() const { return m_lastRefreshed; }
int ProviderBackend::refreshCount() const { return m_refreshCount; }

void ProviderBackend::updateLastRefreshed(const QDateTime &when)
{
    m_lastRefreshed = when.isValid() ? when : QDateTime::currentDateTime();
    m_lastSuccess = m_lastRefreshed.toUTC();
    m_refreshCount++;
    Q_EMIT stateChanged();
}

// --- API Key ---

void ProviderBackend::setApiKey(const QString &key)
{
    if (m_apiKey == key) {
        return;
    }
    cancelRefresh();
    m_apiKey = key;
    if (key.isEmpty()) {
        setConnected(false);
        m_providerState = ProviderState::Unconfigured;
        Q_EMIT stateChanged();
    } else if (m_providerState == ProviderState::Unconfigured) {
        m_providerState = ProviderState::Idle;
        Q_EMIT stateChanged();
    }
}

bool ProviderBackend::hasApiKey() const
{
    if (qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO")) {
        return true;
    }
    return !m_apiKey.isEmpty();
}

QString ProviderBackend::apiKey() const
{
    return m_apiKey;
}

QNetworkAccessManager *ProviderBackend::networkManager() const
{
    return m_networkManager;
}

// --- Centralized Request Builder ---

QNetworkRequest ProviderBackend::createRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);
    request.setRawHeader("Content-Type", "application/json");

    // Default Bearer auth (Anthropic overrides with x-api-key)
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    }

    return request;
}

// --- Rate Limit Header Parsing ---

void ProviderBackend::parseRateLimitHeaders(QNetworkReply *reply, const char *prefix)
{
    const auto hasHeader = [&](const QByteArray &suffix) {
        return !reply->rawHeader(QByteArray(prefix) + suffix).isEmpty();
    };
    auto readHeader = [&](const QByteArray &suffix) -> int {
        QByteArray val = reply->rawHeader(QByteArray(prefix) + suffix);
        return val.isEmpty() ? 0 : val.toInt();
    };

    int rlRequests = readHeader("limit-requests");
    int rlTokens = readHeader("limit-tokens");
    int rlReqRemaining = readHeader("remaining-requests");
    int rlTokRemaining = readHeader("remaining-tokens");
    QString rlReset = QString::fromUtf8(reply->rawHeader(QByteArray(prefix) + "reset-requests"));

    if (rlRequests > 0 && hasHeader("remaining-requests")) {
        setRateLimitRequests(rlRequests);
        setRateLimitRequestsRemaining(rlReqRemaining);
    }
    if (rlTokens > 0 && hasHeader("remaining-tokens")) {
        setRateLimitTokens(rlTokens);
        setRateLimitTokensRemaining(rlTokRemaining);
    }
    if (!rlReset.isEmpty()) {
        setRateLimitResetTime(rlReset);
    }
}

// --- Generation Counter & Request Cancellation ---

int ProviderBackend::currentGeneration() const
{
    return m_generation;
}

bool ProviderBackend::requestRefresh(RefreshReason reason)
{
    if (m_loading) {
        if (reason != RefreshReason::Manual) {
            ++m_coalescedRefreshCount;
            Q_EMIT diagnosticsChanged();
            return false;
        }
        cancelRefresh();
    }

    m_pendingRefreshReason = reason;
    refreshImpl();
    return true;
}

void ProviderBackend::refresh()
{
    requestRefresh(RefreshReason::Manual);
}

void ProviderBackend::cancelRefresh()
{
    const bool hadActiveWork = m_loading || !m_activeReplies.isEmpty();
    ++m_generation;
    for (QNetworkReply *reply : std::as_const(m_activeReplies)) {
        if (reply != nullptr && reply->isRunning()) {
            reply->abort();
        }
        if (reply != nullptr) {
            reply->deleteLater();
        }
    }
    m_activeReplies.clear();
    if (hadActiveWork) {
        ++m_cancellationCount;
        Q_EMIT diagnosticsChanged();
    }
    setLoading(false);
}

void ProviderBackend::setNextScheduledRefresh(const QDateTime &when)
{
    const QDateTime normalized = when.isValid() ? when.toUTC() : QDateTime();
    if (m_nextScheduledRefresh != normalized) {
        m_nextScheduledRefresh = normalized;
        Q_EMIT diagnosticsChanged();
    }
}

void ProviderBackend::trackReply(QNetworkReply *reply)
{
    m_activeReplies.append(reply);
    // Auto-remove from tracking when the reply finishes
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_activeReplies.removeOne(reply);
    });
}

void ProviderBackend::beginRefresh()
{
    m_generation++;
    m_lastRefreshReason = m_pendingRefreshReason;
    m_lastAttempt = QDateTime::currentDateTimeUtc();
    Q_EMIT stateChanged();

    // Abort and clean up any in-flight replies from previous refresh
    for (QNetworkReply *reply : std::as_const(m_activeReplies)) {
        if (reply != nullptr && reply->isRunning()) {
            reply->abort();
        }
        reply->deleteLater();
    }
    m_activeReplies.clear();
}

bool ProviderBackend::isCurrentGeneration(int generation) const
{
    return generation == m_generation;
}

// --- Retry Logic ---

bool ProviderBackend::isRetryableStatus(int httpStatus)
{
    return httpStatus == 408 || httpStatus == 425 || httpStatus == 429
        || httpStatus == 500 || httpStatus == 502 || httpStatus == 503 || httpStatus == 504;
}

ProviderBackend::ProviderErrorKind ProviderBackend::errorKindForNetworkReply(QNetworkReply *reply)
{
    if (!reply) {
        return ProviderErrorKind::Network;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401) return ProviderErrorKind::Authentication;
    if (status == 403) return ProviderErrorKind::Permission;
    if (status == 429) return ProviderErrorKind::RateLimit;
    if (status >= 500) return ProviderErrorKind::Server;
    if (reply->error() == QNetworkReply::OperationCanceledError) return ProviderErrorKind::Cancelled;
    if (reply->error() == QNetworkReply::TimeoutError) return ProviderErrorKind::Timeout;
    if (reply->error() != QNetworkReply::NoError) return ProviderErrorKind::Network;
    return ProviderErrorKind::None;
}

QDateTime ProviderBackend::retryAfterForReply(QNetworkReply *reply, const QDateTime &now)
{
    if (!reply) {
        return {};
    }
    const QByteArray value = reply->rawHeader("Retry-After").trimmed();
    if (value.isEmpty()) {
        return {};
    }
    bool secondsOk = false;
    const qint64 seconds = value.toLongLong(&secondsOk);
    if (secondsOk && seconds >= 0) {
        return now.toUTC().addSecs(qMin<qint64>(seconds, 3600));
    }
    QDateTime parsed = QDateTime::fromString(QString::fromLatin1(value), Qt::RFC2822Date);
    if (!parsed.isValid()) {
        parsed = QLocale::c().toDateTime(QString::fromLatin1(value),
                                         QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
        if (parsed.isValid()) {
            parsed.setTimeZone(QTimeZone::UTC);
        }
    }
    return parsed.isValid() ? parsed.toUTC() : QDateTime();
}

void ProviderBackend::retryRequest(QNetworkReply *reply,
                                    const QUrl &url,
                                    const QByteArray &postBody,
                                    std::function<void(QNetworkReply *)> callback,
                                    int attempt,
                                    int maxRetries)
{
    if (attempt > maxRetries) {
        // No more retries — let the caller handle the error
        callback(reply);
        return;
    }

    reply->deleteLater();

    // Calculate backoff: 2^attempt seconds + jitter (0-500ms)
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Check for Retry-After header (seconds or HTTP date)
    int delaySecs = (1 << attempt); // 2, 4, 8...
    const QDateTime retryAt = retryAfterForReply(reply);
    if (retryAt.isValid()) {
        delaySecs = qBound(0, static_cast<int>(QDateTime::currentDateTimeUtc().secsTo(retryAt)), 3600);
    }

    int delayMs = delaySecs * 1000 + (QRandomGenerator::global()->bounded(500)); // jitter
    int gen = m_generation;

    qWarning() << "ProviderBackend:" << name()
               << "- retrying request (attempt" << attempt << "/" << maxRetries
               << ") after" << delayMs << "ms (HTTP" << httpStatus << ")";

    QTimer::singleShot(delayMs, this, [this, url, postBody, callback, attempt, maxRetries, gen]() {
        if (!isCurrentGeneration(gen)) return; // stale

        QNetworkRequest request = createRequest(url);
        QNetworkReply *retryReply;
        if (postBody.isEmpty()) {
            retryReply = networkManager()->get(request);
        } else {
            retryReply = networkManager()->post(request, postBody);
        }
        trackReply(retryReply);

        connect(retryReply, &QNetworkReply::finished, this, [this, retryReply, url, postBody, callback, attempt, maxRetries, gen]() {
            if (!isCurrentGeneration(gen)) { retryReply->deleteLater(); return; }

            int status = retryReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (retryReply->error() != QNetworkReply::NoError && isRetryableStatus(status)) {
                retryRequest(retryReply, url, postBody, callback, attempt + 1, maxRetries);
            } else {
                callback(retryReply);
            }
        });
    });
}

// --- Token-based Cost Estimation ---

void ProviderBackend::registerModelPricing(const QString &modelName, double inputPricePerMToken, double outputPricePerMToken)
{
    m_modelPricing.insert(modelName, ModelPricing{inputPricePerMToken, outputPricePerMToken});
}

void ProviderBackend::registerCatalogPricing(const QString &providerKey)
{
    const QVariantList rows = ProviderPricingCatalog::instance()->tokenModelsForProvider(providerKey);
    for (const QVariant &rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        const QString id = row.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        registerModelPricing(id,
                             row.value(QStringLiteral("input")).toDouble(),
                             row.value(QStringLiteral("output")).toDouble());
    }
}

void ProviderBackend::updateEstimatedCost(const QString &currentModel)
{
    // Only estimate if no real cost has been set by a billing API
    if (!m_isEstimatedCost && m_cost > 0) return;

    auto it = m_modelPricing.constFind(currentModel);
    if (it == m_modelPricing.constEnd()) {
        // Try prefix matching (e.g., "mistral-large-latest" could match "mistral-large")
        for (auto pit = m_modelPricing.constBegin(); pit != m_modelPricing.constEnd(); ++pit) {
            if (currentModel.startsWith(pit.key())) {
                it = pit;
                break;
            }
        }
    }
    if (it == m_modelPricing.constEnd()) return;

    double inputCost = (static_cast<double>(m_inputTokens) / 1000000.0) * it->inputPricePerMToken;
    double outputCost = (static_cast<double>(m_outputTokens) / 1000000.0) * it->outputPricePerMToken;
    double estimatedTotal = inputCost + outputCost;

    m_cost = estimatedTotal;
    m_isEstimatedCost = true;
    setCostSource(QStringLiteral("estimated_from_usage"));
    m_dailyCost = estimatedTotal; // Best estimate for daily cost from accumulated tokens
    checkBudgetLimits();
}

void ProviderBackend::setEstimatedCost(double cost)
{
    m_cost = qMax(0.0, cost);
    m_isEstimatedCost = true;
    setCostSource(QStringLiteral("estimated_from_usage"));
    m_dailyCost = m_cost;
    m_monthlyCost = m_cost;
    checkBudgetLimits();
}
