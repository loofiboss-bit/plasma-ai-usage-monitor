#ifndef XAIPROVIDER_H
#define XAIPROVIDER_H

#include "openaicompatibleprovider.h"

/**
 * xAI/Grok provider backend.
 *
 * Uses OpenAI-compatible API at api.x.ai/v1.
 * - Rate limit headers: x-ratelimit-*
 * - Token usage from chat completion response body
 *
 * Models: grok-3, grok-3-mini, grok-2
 */
class XAIProvider : public OpenAICompatibleProvider
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit XAIProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("xAI"); }
    QString iconName() const override { return QStringLiteral("globe"); }

protected:
    const char *defaultBaseUrl() const override { return BASE_URL; }

private:
    static constexpr const char *BASE_URL = "https://api.x.ai/v1";
};

#endif // XAIPROVIDER_H
