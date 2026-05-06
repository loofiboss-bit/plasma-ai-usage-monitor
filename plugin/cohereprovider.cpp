#include "cohereprovider.h"

CohereProvider::CohereProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("command-a-03-2025"));
    registerCatalogPricing(QStringLiteral("cohere"));
}
