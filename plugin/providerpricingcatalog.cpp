#include "providerpricingcatalog.h"

#include "costengine.h"

#include <QJsonArray>
#include <QDate>
#include <limits>

ProviderPricingCatalog::ProviderPricingCatalog(QObject *parent)
    : CatalogLoader(QStringLiteral("providers-v4.json"), 7, parent)
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
    const QJsonObject model = modelObject(providerKey, modelId);
    if (model.isEmpty()) {
        return {};
    }

    QVariantMap result = model.value(QStringLiteral("pricing")).toObject().toVariantMap();
    const QVariantMap provider = providerObject(providerKey).toVariantMap();
    const QVariantMap lifecycle = model.value(QStringLiteral("lifecycle")).toObject().toVariantMap();
    const QString canonicalModelId = model.value(QStringLiteral("id")).toString();
    result.insert(QStringLiteral("providerKey"), provider.value(QStringLiteral("key"), providerKey));
    result.insert(QStringLiteral("modelId"), canonicalModelId);
    result.insert(QStringLiteral("priceId"), result.value(QStringLiteral("priceId"),
                                                          providerKey.trimmed().toLower() + QLatin1Char(':') + canonicalModelId));
    result.insert(QStringLiteral("lifecycleStatus"), lifecycle.value(QStringLiteral("status")));
    result.insert(QStringLiteral("catalogVersion"), catalogVersion());
    result.insert(QStringLiteral("sourceFingerprint"), sourceFingerprint());
    result.insert(QStringLiteral("verificationState"), verificationState());
    result.insert(QStringLiteral("estimatesAllowed"), estimatesAllowed());
    result.insert(QStringLiteral("sourceRefs"), model.value(QStringLiteral("sourceRefs")).toArray().toVariantList());
    result.insert(QStringLiteral("lifecycle"), lifecycle);
    result.insert(QStringLiteral("priceChange"), model.value(QStringLiteral("priceChange")).toObject().toVariantMap());
    result.insert(QStringLiteral("aliases"), model.value(QStringLiteral("aliases")).toArray().toVariantList());
    return result;
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

QVariantMap ProviderPricingCatalog::estimateCost(const QString &providerKey,
                                                 const QString &modelId,
                                                 const QVariantMap &usage) const
{
    return CostEngine::estimate(pricing(providerKey, modelId), usage,
                                catalogVersion(), sourceFingerprint());
}

QVariantList ProviderPricingCatalog::selectableModelsForProvider(const QString &providerKey) const
{
    QVariantList result;
    const QDate today = QDate::currentDate();
    const QJsonArray models = providerObject(providerKey).value(QStringLiteral("models")).toArray();
    for (const QJsonValue &entry : models) {
        const QJsonObject model = entry.toObject();
        const QJsonObject lifecycle = model.value(QStringLiteral("lifecycle")).toObject();
        const QString status = lifecycle.value(QStringLiteral("status")).toString();
        const QDate deprecationDate = QDate::fromString(
            lifecycle.value(QStringLiteral("deprecationDate")).toString(), Qt::ISODate);
        if (status == QLatin1String("retired")
            || (status == QLatin1String("deprecated") && deprecationDate.isValid() && today >= deprecationDate)) {
            continue;
        }
        result.append(model.toVariantMap());
    }
    return result;
}

QString ProviderPricingCatalog::effectiveModelId(const QString &providerKey, const QString &modelId) const
{
    return effectiveModelIdAt(providerKey, modelId, QDate::currentDate());
}

QString ProviderPricingCatalog::effectiveModelIdAt(const QString &providerKey,
                                                    const QString &modelId,
                                                    const QDate &today) const
{
    const QJsonObject model = modelObject(providerKey, modelId);
    const QJsonObject lifecycle = model.value(QStringLiteral("lifecycle")).toObject();
    const QString status = lifecycle.value(QStringLiteral("status")).toString();
    const QDate deprecationDate = QDate::fromString(
        lifecycle.value(QStringLiteral("deprecationDate")).toString(), Qt::ISODate);
    const QString replacement = lifecycle.value(QStringLiteral("replacementId")).toString();
    if ((status == QLatin1String("retired")
         || (status == QLatin1String("deprecated") && deprecationDate.isValid() && today >= deprecationDate))
        && !replacement.isEmpty()) {
        return replacement;
    }
    return model.value(QStringLiteral("id")).toString(modelId);
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
        const QJsonArray aliases = model.value(QStringLiteral("aliases")).toArray();
        for (const QJsonValue &alias : aliases) {
            if (alias.toString() == normalized) {
                return model;
            }
        }
    }

    return QJsonObject();
}
