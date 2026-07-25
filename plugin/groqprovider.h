#ifndef GROQPROVIDER_H
#define GROQPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * Groq provider backend.
 *
 * Uses OpenAI-compatible API at api.groq.com/openai/v1.
 * - Rate limit headers: x-ratelimit-*
 * - Token usage from chat completion response body
 * - Very fast inference on optimized hardware
 *
 * Models: llama-3.3-70b-versatile, llama-3.1-8b-instant, mixtral-8x7b-32768, gemma2-9b-it
 */
class GroqProvider : public OpenAICompatibleProvider
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit GroqProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Groq"); }
    QString iconName() const override { return QStringLiteral("globe"); }

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private:
    static constexpr const char *BASE_URL = "https://api.groq.com/openai/v1";
};

#endif // GROQPROVIDER_H
