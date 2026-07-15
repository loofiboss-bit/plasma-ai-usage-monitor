#include "providerpricingcatalog.h"

#include <QJsonArray>
#include <QDate>
#include <limits>

ProviderPricingCatalog::ProviderPricingCatalog(QObject *parent)
    : CatalogLoader(QStringLiteral("providers-v4.json"), 5, parent)
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

QVariantMap ProviderPricingCatalog::estimateCost(const QString &providerKey,
                                                 const QString &modelId,
                                                 const QVariantMap &usage) const
{
    const QVariantMap price = pricing(providerKey, modelId);
    QVariantMap result{{QStringLiteral("complete"), false},
                       {QStringLiteral("currency"), price.value(QStringLiteral("currency"))},
                       {QStringLiteral("precision"), price.value(QStringLiteral("precision"))},
                       {QStringLiteral("amount"), QVariant()},
                       {QStringLiteral("missingDimensions"), QStringList{}}};
    QStringList missing;
    if (price.isEmpty() || price.value(QStringLiteral("status")).toString() == QLatin1String("unknown")) {
        missing << QStringLiteral("pricing");
        result[QStringLiteral("missingDimensions")] = missing;
        return result;
    }

    const QString unit = price.value(QStringLiteral("unit")).toString();
    if (unit != QLatin1String("1M_tokens")) {
        const QVariantMap unitUsage = usage.value(QStringLiteral("unitUsage")).toMap();
        if (!price.contains(QStringLiteral("amount")) || !unitUsage.contains(unit)) {
            missing << unit;
            result[QStringLiteral("missingDimensions")] = missing;
            return result;
        }
        result[QStringLiteral("amount")] = price.value(QStringLiteral("amount")).toDouble()
            * unitUsage.value(unit).toDouble();
        result[QStringLiteral("complete")] = true;
        return result;
    }

    QVariantMap rates = price;
    const qint64 contextTokens = usage.value(QStringLiteral("contextTokens"),
                                             usage.value(QStringLiteral("inputTokens"))).toLongLong();
    const QVariantList contextTiers = price.value(QStringLiteral("contextTiers")).toList();
    for (const QVariant &entry : contextTiers) {
        const QVariantMap tier = entry.toMap();
        const qint64 minimum = tier.value(QStringLiteral("minInputTokens"), 0).toLongLong();
        const qint64 maximum = tier.value(QStringLiteral("maxInputTokens"), std::numeric_limits<qint64>::max()).toLongLong();
        if (contextTokens >= minimum && contextTokens <= maximum) {
            for (auto it = tier.cbegin(); it != tier.cend(); ++it) rates.insert(it.key(), it.value());
            break;
        }
    }

    const QString modality = usage.value(QStringLiteral("modality"), QStringLiteral("text")).toString();
    const QVariantMap modalityRates = price.value(QStringLiteral("modalityRates")).toMap();
    if (!modalityRates.isEmpty()) {
        const QVariantMap selected = modalityRates.value(modality).toMap();
        if (selected.isEmpty()) missing << QStringLiteral("modality:") + modality;
        else for (auto it = selected.cbegin(); it != selected.cend(); ++it) rates.insert(it.key(), it.value());
    }
    const QString tierName = usage.value(QStringLiteral("serviceTier"), QStringLiteral("standard")).toString();
    if (tierName == QLatin1String("priority")) {
        const QVariantMap priority = price.value(QStringLiteral("priorityRates")).toMap();
        if (priority.isEmpty()) missing << QStringLiteral("serviceTier:priority");
        else {
            for (auto it = priority.cbegin(); it != priority.cend(); ++it) rates.insert(it.key(), it.value());
            if (modality == QLatin1String("audio") && priority.contains(QStringLiteral("audioInput")))
                rates.insert(QStringLiteral("input"), priority.value(QStringLiteral("audioInput")));
            if (modality == QLatin1String("audio") && priority.contains(QStringLiteral("audioCachedInput")))
                rates.insert(QStringLiteral("cachedInput"), priority.value(QStringLiteral("audioCachedInput")));
        }
    }

    const qint64 inputTokens = qMax<qint64>(0, usage.value(QStringLiteral("inputTokens")).toLongLong());
    const qint64 cachedTokens = qBound<qint64>(0, usage.value(QStringLiteral("cachedInputTokens")).toLongLong(), inputTokens);
    const qint64 outputTokens = qMax<qint64>(0, usage.value(QStringLiteral("outputTokens")).toLongLong());
    if (!rates.contains(QStringLiteral("input")) || !rates.contains(QStringLiteral("output")))
        missing << QStringLiteral("tokenRates");
    if (cachedTokens > 0 && (!rates.contains(QStringLiteral("cachedInput"))
                             || rates.value(QStringLiteral("cachedInput")).isNull()))
        missing << QStringLiteral("cachedInputRate");

    double amount = 0.0;
    if (rates.contains(QStringLiteral("input")))
        amount += static_cast<double>(inputTokens - cachedTokens) / 1'000'000.0
            * rates.value(QStringLiteral("input")).toDouble();
    if (cachedTokens > 0 && rates.contains(QStringLiteral("cachedInput"))
        && !rates.value(QStringLiteral("cachedInput")).isNull())
        amount += static_cast<double>(cachedTokens) / 1'000'000.0
            * rates.value(QStringLiteral("cachedInput")).toDouble();
    if (rates.contains(QStringLiteral("output")))
        amount += static_cast<double>(outputTokens) / 1'000'000.0
            * rates.value(QStringLiteral("output")).toDouble();

    if (tierName == QLatin1String("batch")) {
        if (!price.contains(QStringLiteral("batchDiscountPercent"))) missing << QStringLiteral("serviceTier:batch");
        else amount *= 1.0 - price.value(QStringLiteral("batchDiscountPercent")).toDouble() / 100.0;
    }

    const QVariantMap additiveUsage = usage.value(QStringLiteral("additiveUsage")).toMap();
    const QVariantList additiveFees = price.value(QStringLiteral("additiveFees")).toList();
    for (const QVariant &entry : additiveFees) {
        const QVariantMap fee = entry.toMap();
        const QString kind = fee.value(QStringLiteral("kind")).toString();
        if (!additiveUsage.contains(kind)) continue;
        if (fee.value(QStringLiteral("freeAllowance")).toLongLong() > 0
            && !usage.value(QStringLiteral("allowanceConsumed")).toMap().contains(kind)) {
            missing << QStringLiteral("allowanceConsumed:") + kind;
            continue;
        }
        const qint64 consumed = usage.value(QStringLiteral("allowanceConsumed")).toMap().value(kind).toLongLong();
        const qint64 free = fee.value(QStringLiteral("freeAllowance")).toLongLong();
        const double requested = additiveUsage.value(kind).toDouble();
        const double billable = qMax(0.0, requested - qMax<qint64>(0, free - consumed));
        amount += billable * fee.value(QStringLiteral("amount")).toDouble();
    }

    missing.removeDuplicates();
    result[QStringLiteral("amount")] = amount;
    result[QStringLiteral("complete")] = missing.isEmpty();
    result[QStringLiteral("missingDimensions")] = missing;
    return result;
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
    return modelId;
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
