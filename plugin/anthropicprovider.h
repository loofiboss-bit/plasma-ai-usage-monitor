#ifndef ANTHROPICPROVIDER_H
#define ANTHROPICPROVIDER_H

#include "providerbackend.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * Anthropic provider backend.
 *
 * Anthropic does NOT have a usage/billing API.
 * This provider makes a lightweight count_tokens call to read
 * the rate limit headers from the response:
 *   - anthropic-ratelimit-requests-limit
 *   - anthropic-ratelimit-requests-remaining
 *   - anthropic-ratelimit-input-tokens-limit
 *   - anthropic-ratelimit-input-tokens-remaining
 *   - anthropic-ratelimit-output-tokens-limit
 *   - anthropic-ratelimit-output-tokens-remaining
 *   - anthropic-ratelimit-*-reset
 */
class AnthropicProvider : public ProviderBackend
{
    Q_OBJECT

    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit AnthropicProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Anthropic"); }
    QString iconName() const override { return QStringLiteral("globe"); }

    QString model() const;
    void setModel(const QString &model);

    void refreshImpl() override;

Q_SIGNALS:
    void modelChanged();

private Q_SLOTS:
    void onCountTokensReply(QNetworkReply *reply);

private:
    void fetchRateLimits();

    QString m_model = QStringLiteral("claude-sonnet-4-20250514");

    static constexpr const char *BASE_URL = "https://api.anthropic.com/v1";
    static constexpr const char *API_VERSION = "2023-06-01";
};

#endif // ANTHROPICPROVIDER_H
