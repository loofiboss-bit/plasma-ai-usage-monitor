#ifndef OPENAICOMPATIBLEPROVIDER_H
#define OPENAICOMPATIBLEPROVIDER_H

#include "providerbackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * Base class for OpenAI-compatible provider backends.
 *
 * Handles the common pattern of:
 * - POST /chat/completions with max_tokens=1 to read rate limit headers
 * - Parsing x-ratelimit-* response headers
 * - Parsing usage object from response body as widget probe traffic
 * - Probe-level token tracking kept separate from user usage and spend
 *
 * Subclasses must provide: name(), iconName(), defaultModel(), baseUrl()
 * Subclasses can override refreshImpl() to add extra API calls (e.g., balance endpoint)
 */
class OpenAICompatibleProvider : public ProviderBackend
{
    Q_OBJECT

    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit OpenAICompatibleProvider(QObject *parent = nullptr);

    QString model() const;
    void setModel(const QString &model);

    void refreshImpl() override;

Q_SIGNALS:
    void modelChanged();

protected:
    /// The default base URL for this provider (e.g., "https://api.groq.com/openai/v1")
    virtual const char *defaultBaseUrl() const = 0;

    /// Called when the chat completion request finishes successfully.
    /// Base implementation parses rate limits and usage. Override to add extra logic.
    virtual void onCompletionFinished(QNetworkReply *reply);

    /// Called when all pending requests are done (for multi-request providers)
    virtual void onAllRequestsDone();

    /// Increment/decrement pending request counter (for subclasses like DeepSeek)
    void addPendingRequest();
    bool decrementPendingRequest();

private:
    void fetchRateLimits();
    void parseRateLimitHeaders(QNetworkReply *reply);
    void parseUsageBody(QNetworkReply *reply);

    QString m_model;
    int m_pendingRequests = 0;
    QByteArray m_lastRequestBody; // stored for retry support

    // Widget-generated connectivity probes, kept separate from user usage.
    qint64 m_sessionProbeInputTokens = 0;
    qint64 m_sessionProbeOutputTokens = 0;
    int m_sessionProbeRequestCount = 0;
};

#endif // OPENAICOMPATIBLEPROVIDER_H
