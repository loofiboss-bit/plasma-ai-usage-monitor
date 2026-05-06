#include "groqprovider.h"

GroqProvider::GroqProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("llama-3.3-70b-versatile"));
    registerCatalogPricing(QStringLiteral("groq"));
}
