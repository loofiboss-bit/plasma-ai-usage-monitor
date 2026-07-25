#ifndef TOGETHERPROVIDER_H
#define TOGETHERPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * Together AI provider backend.
 *
 * Uses OpenAI-compatible API at api.together.xyz/v1.
 * - Rate limit info from response headers (x-ratelimit-*)
 * - Usage data from chat completion response body
 *
 * Together AI provides fast inference for open-source models:
 * Meta Llama, Mixtral, Qwen, DeepSeek, and more.
 *
 * Models: meta-llama/Llama-3.3-70B-Instruct-Turbo,
 *         Qwen/Qwen2.5-72B-Instruct-Turbo,
 *         deepseek-ai/DeepSeek-V3, etc.
 */
class TogetherProvider : public OpenAICompatibleProvider
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit TogetherProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Together AI"); }
    QString iconName() const override { return QStringLiteral("globe"); }

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private:
    static constexpr const char *BASE_URL = "https://api.together.xyz/v1";
};

#endif // TOGETHERPROVIDER_H
