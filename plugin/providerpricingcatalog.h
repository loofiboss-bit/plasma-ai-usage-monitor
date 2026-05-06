#ifndef PROVIDERPRICINGCATALOG_H
#define PROVIDERPRICINGCATALOG_H

#include "catalogloader.h"

#include <QVariantList>
#include <QVariantMap>

class ProviderPricingCatalog : public CatalogLoader
{
    Q_OBJECT

public:
    explicit ProviderPricingCatalog(QObject *parent = nullptr);

    static ProviderPricingCatalog *instance();

    Q_INVOKABLE QVariantList providers() const;
    Q_INVOKABLE QVariantMap provider(const QString &providerKey) const;
    Q_INVOKABLE QVariantMap model(const QString &providerKey, const QString &modelId) const;
    Q_INVOKABLE QVariantMap pricing(const QString &providerKey, const QString &modelId) const;
    Q_INVOKABLE QVariantList tokenModelsForProvider(const QString &providerKey) const;
    Q_INVOKABLE double amountForModelUnit(const QString &providerKey, const QString &modelId, const QString &unit) const;

private:
    QJsonObject providerObject(const QString &providerKey) const;
    QJsonObject modelObject(const QString &providerKey, const QString &modelId) const;
};

#endif // PROVIDERPRICINGCATALOG_H
