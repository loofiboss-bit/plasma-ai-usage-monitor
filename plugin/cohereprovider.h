#ifndef COHEREPROVIDER_H
#define COHEREPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * Cohere provider backend.
 *
 * Uses Cohere's OpenAI-compatible API at api.cohere.com/compatibility/v1.
 * - Rate limit info from response headers (x-ratelimit-*)
 * - Usage data from chat completion response body
 *
 * Cohere specializes in enterprise RAG and multilingual models.
 *
 * Models: command-a-03-2025, command-r-plus, command-r, command-light
 */
class CohereProvider : public OpenAICompatibleProvider
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit CohereProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Cohere"); }
    QString iconName() const override { return QStringLiteral("globe"); }

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private:
    static constexpr const char *BASE_URL = "https://api.cohere.com/compatibility/v1";
};

#endif // COHEREPROVIDER_H
