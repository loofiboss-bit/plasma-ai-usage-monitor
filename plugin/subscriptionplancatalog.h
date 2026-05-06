#ifndef SUBSCRIPTIONPLANCATALOG_H
#define SUBSCRIPTIONPLANCATALOG_H

#include "catalogloader.h"

#include <QVariantList>
#include <QVariantMap>

class SubscriptionPlanCatalog : public CatalogLoader
{
    Q_OBJECT

public:
    explicit SubscriptionPlanCatalog(QObject *parent = nullptr);

    static SubscriptionPlanCatalog *instance();

    Q_INVOKABLE QVariantList tools() const;
    Q_INVOKABLE QVariantMap tool(const QString &toolKey) const;
    Q_INVOKABLE QStringList planIdsForTool(const QString &toolKey) const;
    Q_INVOKABLE QStringList planLabelsForTool(const QString &toolKey) const;
    Q_INVOKABLE QString planIdForLabel(const QString &toolKey, const QString &planLabelOrId) const;
    Q_INVOKABLE QString planLabelForId(const QString &toolKey, const QString &planIdOrLabel) const;
    Q_INVOKABLE QVariantMap plan(const QString &toolKey, const QString &planIdOrLabel) const;
    Q_INVOKABLE QVariantList quotaWindows(const QString &toolKey, const QString &planIdOrLabel) const;
    Q_INVOKABLE QVariantMap price(const QString &toolKey, const QString &planIdOrLabel) const;
    Q_INVOKABLE QVariantMap billingMode(const QString &toolKey, const QString &modeId) const;
    Q_INVOKABLE QVariantList billingModeQuotaWindows(const QString &toolKey, const QString &modeId) const;

private:
    QJsonObject toolObject(const QString &toolKey) const;
    QJsonObject planObject(const QString &toolKey, const QString &planIdOrLabel) const;
};

#endif // SUBSCRIPTIONPLANCATALOG_H
