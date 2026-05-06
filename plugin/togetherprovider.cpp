#include "togetherprovider.h"

TogetherProvider::TogetherProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("meta-llama/Llama-3.3-70B-Instruct-Turbo"));
    registerCatalogPricing(QStringLiteral("together"));
}
