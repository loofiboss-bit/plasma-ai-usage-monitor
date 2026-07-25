#ifndef OLLAMACLOUDPROVIDER_H
#define OLLAMACLOUDPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * Ollama Cloud provider backend.
 *
 * Uses Ollama Cloud's OpenAI-compatible API at ollama.com/v1.
 * - Rate limit info from response headers when present
 * - Usage data from chat completion response body
 *
 * Ollama Cloud accepts API keys created in the user's Ollama settings.
 * Cost estimation is intentionally disabled until Ollama publishes stable
 * metered pricing for the OpenAI-compatible cloud API.
 */
class OllamaCloudProvider : public OpenAICompatibleProvider
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit OllamaCloudProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Ollama Cloud"); }
    QString iconName() const override { return QStringLiteral("network-server"); }

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private:
    static constexpr const char *BASE_URL = "https://ollama.com/v1";
};

#endif // OLLAMACLOUDPROVIDER_H
