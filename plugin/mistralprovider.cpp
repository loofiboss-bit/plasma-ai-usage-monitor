#include "mistralprovider.h"

MistralProvider::MistralProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("mistral-large-latest"));
    registerCatalogPricing(QStringLiteral("mistral"));
}
