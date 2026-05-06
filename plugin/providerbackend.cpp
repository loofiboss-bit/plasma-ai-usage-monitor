#include "providerbackend.h"
#include "providerpricingcatalog.h"
#include <QDate>
#include <QUrl>
#include <QRandomGenerator>

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
        Q_EMIT loadingChanged();
    }
}

void ProviderBackend::setError(const QString &error)
{
    m_error = error;
    m_errorCount++;
    m_consecutiveErrors++;
    Q_EMIT errorChanged();
}

void ProviderBackend::clearError()
{
    if (!m_error.isEmpty()) {
        m_error.clear();
        m_consecutiveErrors = 0;
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

void ProviderBackend::setInputTokens(qint64 tokens) { m_inputTokens = tokens; }
void ProviderBackend::setOutputTokens(qint64 tokens) { m_outputTokens = tokens; }
void ProviderBackend::setRequestCount(int count) { m_requestCount = count; }
void ProviderBackend::setCost(double cost) {
    m_cost = cost;
    m_isEstimatedCost = false;
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
    double warningFraction = m_budgetWarningPercent / 100.0;

    // Daily budget checks
    if (m_dailyBudget > 0) {
        if (m_dailyCost >= m_dailyBudget && !m_dailyExceededEmitted) {
            m_dailyExceededEmitted = true;
            Q_EMIT budgetExceeded(name(), QStringLiteral("daily"), m_dailyCost, m_dailyBudget);
        } else if (m_dailyCost >= m_dailyBudget * warningFraction && !m_dailyWarningEmitted) {
            m_dailyWarningEmitted = true;
            Q_EMIT budgetWarning(name(), QStringLiteral("daily"), m_dailyCost, m_dailyBudget);
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
            Q_EMIT budgetExceeded(name(), QStringLiteral("monthly"), m_monthlyCost, m_monthlyBudget);
        } else if (m_monthlyCost >= m_monthlyBudget * warningFraction && !m_monthlyWarningEmitted) {
            m_monthlyWarningEmitted = true;
            Q_EMIT budgetWarning(name(), QStringLiteral("monthly"), m_monthlyCost, m_monthlyBudget);
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

void ProviderBackend::setRateLimitRequests(int limit) { m_rateLimitRequests = limit; }
void ProviderBackend::setRateLimitTokens(int limit) { m_rateLimitTokens = limit; }
void ProviderBackend::setRateLimitRequestsRemaining(int remaining) { m_rateLimitRequestsRemaining = remaining; }
void ProviderBackend::setRateLimitTokensRemaining(int remaining) { m_rateLimitTokensRemaining = remaining; }
void ProviderBackend::setRateLimitResetTime(const QString &time) { m_rateLimitResetTime = time; }

// --- Custom URL ---

QString ProviderBackend::customBaseUrl() const { return m_customBaseUrl; }
void ProviderBackend::setCustomBaseUrl(const QString &url)
{
    if (m_customBaseUrl != url) {
        m_customBaseUrl = url;
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
        // Warn if HTTP (not HTTPS) and not localhost
        QUrl parsed(url);
        if (parsed.scheme() == QLatin1String("http")
            && parsed.host() != QLatin1String("localhost")
            && parsed.host() != QLatin1String("127.0.0.1")
            && parsed.host() != QLatin1String("::1")) {
            qWarning() << "ProviderBackend:" << name()
                       << "- custom URL uses insecure HTTP. API keys will be sent unencrypted:"
                       << url;
        }
        return url;
    }
    return QLatin1String(defaultUrl);
}

// --- Metadata ---

QDateTime ProviderBackend::lastRefreshed() const { return m_lastRefreshed; }
int ProviderBackend::refreshCount() const { return m_refreshCount; }

void ProviderBackend::updateLastRefreshed()
{
    m_lastRefreshed = QDateTime::currentDateTime();
    m_refreshCount++;
}

// --- API Key ---

void ProviderBackend::setApiKey(const QString &key)
{
    m_apiKey = key;
    if (key.isEmpty()) {
        setConnected(false);
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
    auto readHeader = [&](const QByteArray &suffix) -> int {
        QByteArray val = reply->rawHeader(QByteArray(prefix) + suffix);
        return val.isEmpty() ? 0 : val.toInt();
    };

    int rlRequests = readHeader("limit-requests");
    int rlTokens = readHeader("limit-tokens");
    int rlReqRemaining = readHeader("remaining-requests");
    int rlTokRemaining = readHeader("remaining-tokens");
    QString rlReset = QString::fromUtf8(reply->rawHeader(QByteArray(prefix) + "reset-requests"));

    if (rlRequests > 0) {
        setRateLimitRequests(rlRequests);
        setRateLimitRequestsRemaining(rlReqRemaining);
    }
    if (rlTokens > 0) {
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
    return httpStatus == 429 || httpStatus == 500 || httpStatus == 502 || httpStatus == 503;
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
    QByteArray retryAfter = reply->rawHeader("Retry-After");
    if (!retryAfter.isEmpty()) {
        bool ok;
        int retryVal = retryAfter.toInt(&ok);
        if (ok && retryVal > 0 && retryVal < 120) {
            delaySecs = retryVal;
        }
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
    m_dailyCost = estimatedTotal; // Best estimate for daily cost from accumulated tokens
    checkBudgetLimits();
}

void ProviderBackend::setEstimatedCost(double cost)
{
    m_cost = qMax(0.0, cost);
    m_isEstimatedCost = true;
    m_dailyCost = m_cost;
    m_monthlyCost = m_cost;
    checkBudgetLimits();
}
