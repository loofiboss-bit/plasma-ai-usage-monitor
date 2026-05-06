#include "xaiprovider.h"

XAIProvider::XAIProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("grok-3"));
    registerCatalogPricing(QStringLiteral("xai"));
}
