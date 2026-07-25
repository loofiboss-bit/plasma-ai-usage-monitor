#ifndef MISTRALPROVIDER_H
#define MISTRALPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * Mistral AI provider backend.
 *
 * Uses OpenAI-compatible API at api.mistral.ai/v1.
 * - Rate limit headers: x-ratelimit-*
 * - Token usage from chat completion response body
 *
 * Models: mistral-large-latest, mistral-medium-latest, mistral-small-latest, codestral-latest
 */
class MistralProvider : public OpenAICompatibleProvider
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MistralProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Mistral AI"); }
    QString iconName() const override { return QStringLiteral("globe"); }

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private:
    static constexpr const char *BASE_URL = "https://api.mistral.ai/v1";
};

#endif // MISTRALPROVIDER_H
