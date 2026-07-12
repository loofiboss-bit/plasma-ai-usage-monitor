#ifndef DEEPSEEKPROVIDER_H
#define DEEPSEEKPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * DeepSeek provider backend.
 *
 * Uses OpenAI-compatible API at api.deepseek.com.
 * - Rate limit info from response headers (x-ratelimit-*)
 * - Usage data from chat completion response body
 * - Balance from GET /user/balance endpoint
 *
 * Models: deepseek-v4-flash, deepseek-v4-pro, and compatibility aliases.
 */
class DeepSeekProvider : public OpenAICompatibleProvider
{
    Q_OBJECT

    Q_PROPERTY(double balance READ balance NOTIFY balanceChanged)

public:
    explicit DeepSeekProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("DeepSeek"); }
    QString iconName() const override { return QStringLiteral("globe"); }

    double balance() const;

    void refreshImpl() override;

Q_SIGNALS:
    void balanceChanged();

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private Q_SLOTS:
    void onBalanceReply(QNetworkReply *reply);

private:
    void fetchBalance();

    double m_balance = 0.0;

    static constexpr const char *BASE_URL = "https://api.deepseek.com";
};

#endif // DEEPSEEKPROVIDER_H
