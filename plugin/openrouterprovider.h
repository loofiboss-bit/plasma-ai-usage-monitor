#ifndef OPENROUTERPROVIDER_H
#define OPENROUTERPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * OpenRouter provider backend.
 *
 * Uses OpenAI-compatible API at openrouter.ai/api/v1.
 * - Rate limit info from response headers (x-ratelimit-*)
 * - Usage data from chat completion response body
 * - Credits balance from GET /api/v1/auth/key endpoint
 *
 * OpenRouter is a unified gateway to 600+ models from multiple providers.
 * Users select their model on the OpenRouter dashboard; we track the
 * default or configured model for cost estimation.
 *
 * Popular models: openai/gpt-4o, anthropic/claude-sonnet-4, google/gemini-2.0-flash,
 *                 meta-llama/llama-3.3-70b, deepseek/deepseek-chat
 */
class OpenRouterProvider : public OpenAICompatibleProvider
{
    Q_OBJECT

    Q_PROPERTY(double credits READ credits NOTIFY creditsChanged)

public:
    explicit OpenRouterProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("OpenRouter"); }
    QString iconName() const override { return QStringLiteral("globe"); }

    double credits() const;

    void refreshImpl() override;

Q_SIGNALS:
    void creditsChanged();

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private Q_SLOTS:
    void onCreditsReply(QNetworkReply *reply);

private:
    void fetchCredits();

    double m_credits = 0.0;

    static constexpr const char *BASE_URL = "https://openrouter.ai/api/v1";
};

#endif // OPENROUTERPROVIDER_H
