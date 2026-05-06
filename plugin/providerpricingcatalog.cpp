#include "providerpricingcatalog.h"

#include <QJsonArray>

ProviderPricingCatalog::ProviderPricingCatalog(QObject *parent)
    : CatalogLoader(QStringLiteral("providers-v3.json"), 3, parent)
{
    load();
}

ProviderPricingCatalog *ProviderPricingCatalog::instance()
{
    static ProviderPricingCatalog catalog;
    return &catalog;
}

QVariantList ProviderPricingCatalog::providers() const
{
    QVariantList result;
    const QJsonArray providers = rootObject().value(QStringLiteral("providers")).toArray();
    for (const QJsonValue &entry : providers) {
        result << entry.toObject().toVariantMap();
    }
    return result;
}

QVariantMap ProviderPricingCatalog::provider(const QString &providerKey) const
{
    return providerObject(providerKey).toVariantMap();
}

QVariantMap ProviderPricingCatalog::model(const QString &providerKey, const QString &modelId) const
{
    return modelObject(providerKey, modelId).toVariantMap();
}

QVariantMap ProviderPricingCatalog::pricing(const QString &providerKey, const QString &modelId) const
{
    return modelObject(providerKey, modelId).value(QStringLiteral("pricing")).toObject().toVariantMap();
}

QVariantList ProviderPricingCatalog::tokenModelsForProvider(const QString &providerKey) const
{
    QVariantList result;
    const QJsonObject provider = providerObject(providerKey);
    const QJsonArray models = provider.value(QStringLiteral("models")).toArray();
    for (const QJsonValue &entry : models) {
        const QJsonObject model = entry.toObject();
        const QJsonObject pricing = model.value(QStringLiteral("pricing")).toObject();
        if (pricing.value(QStringLiteral("unit")).toString() != QLatin1String("1M_tokens")) {
            continue;
        }
        if (!pricing.contains(QStringLiteral("input")) || !pricing.contains(QStringLiteral("output"))) {
            continue;
        }

        QVariantMap row;
        row.insert(QStringLiteral("id"), model.value(QStringLiteral("id")).toString());
        row.insert(QStringLiteral("input"), pricing.value(QStringLiteral("input")).toDouble());
        row.insert(QStringLiteral("output"), pricing.value(QStringLiteral("output")).toDouble());
        row.insert(QStringLiteral("precision"), pricing.value(QStringLiteral("precision")).toString());
        result << row;
    }
    return result;
}

double ProviderPricingCatalog::amountForModelUnit(const QString &providerKey, const QString &modelId, const QString &unit) const
{
    const QJsonObject model = modelObject(providerKey, modelId);
    const QJsonObject pricing = model.value(QStringLiteral("pricing")).toObject();
    if (pricing.value(QStringLiteral("unit")).toString() != unit) {
        return 0.0;
    }
    return pricing.value(QStringLiteral("amount")).toDouble(0.0);
}

QJsonObject ProviderPricingCatalog::providerObject(const QString &providerKey) const
{
    const QString normalized = providerKey.trimmed().toLower();
    const QJsonArray providers = rootObject().value(QStringLiteral("providers")).toArray();
    for (const QJsonValue &entry : providers) {
        const QJsonObject provider = entry.toObject();
        if (provider.value(QStringLiteral("key")).toString().toLower() == normalized) {
            return provider;
        }
    }
    return QJsonObject();
}

QJsonObject ProviderPricingCatalog::modelObject(const QString &providerKey, const QString &modelId) const
{
    const QString normalized = modelId.trimmed();
    if (normalized.isEmpty()) {
        return QJsonObject();
    }

    const QJsonArray models = providerObject(providerKey).value(QStringLiteral("models")).toArray();
    for (const QJsonValue &entry : models) {
        const QJsonObject model = entry.toObject();
        if (model.value(QStringLiteral("id")).toString() == normalized) {
            return model;
        }
    }

    for (const QJsonValue &entry : models) {
        const QJsonObject model = entry.toObject();
        const QString catalogId = model.value(QStringLiteral("id")).toString();
        if (!catalogId.isEmpty() && normalized.startsWith(catalogId)) {
            return model;
        }
    }

    return QJsonObject();
}
