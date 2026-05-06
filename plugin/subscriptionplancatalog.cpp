#include "subscriptionplancatalog.h"

#include <QJsonArray>

SubscriptionPlanCatalog::SubscriptionPlanCatalog(QObject *parent)
    : CatalogLoader(QStringLiteral("subscriptions-v1.json"), 1, parent)
{
    load();
}

SubscriptionPlanCatalog *SubscriptionPlanCatalog::instance()
{
    static SubscriptionPlanCatalog catalog;
    return &catalog;
}

QVariantList SubscriptionPlanCatalog::tools() const
{
    QVariantList result;
    const QJsonArray tools = rootObject().value(QStringLiteral("tools")).toArray();
    for (const QJsonValue &entry : tools) {
        result << entry.toObject().toVariantMap();
    }
    return result;
}

QVariantMap SubscriptionPlanCatalog::tool(const QString &toolKey) const
{
    return toolObject(toolKey).toVariantMap();
}

QStringList SubscriptionPlanCatalog::planIdsForTool(const QString &toolKey) const
{
    QStringList result;
    const QJsonArray plans = toolObject(toolKey).value(QStringLiteral("plans")).toArray();
    for (const QJsonValue &entry : plans) {
        const QString id = entry.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            result << id;
        }
    }
    return result;
}

QStringList SubscriptionPlanCatalog::planLabelsForTool(const QString &toolKey) const
{
    QStringList result;
    const QJsonArray plans = toolObject(toolKey).value(QStringLiteral("plans")).toArray();
    for (const QJsonValue &entry : plans) {
        const QString label = entry.toObject().value(QStringLiteral("label")).toString();
        if (!label.isEmpty()) {
            result << label;
        }
    }
    return result;
}

QString SubscriptionPlanCatalog::planIdForLabel(const QString &toolKey, const QString &planLabelOrId) const
{
    const QString normalized = planLabelOrId.trimmed().toLower();
    const QJsonArray plans = toolObject(toolKey).value(QStringLiteral("plans")).toArray();
    if (normalized.isEmpty() && !plans.isEmpty()) {
        return plans.first().toObject().value(QStringLiteral("id")).toString();
    }
    for (const QJsonValue &entry : plans) {
        const QJsonObject plan = entry.toObject();
        if (plan.value(QStringLiteral("id")).toString().toLower() == normalized
            || plan.value(QStringLiteral("label")).toString().toLower() == normalized) {
            return plan.value(QStringLiteral("id")).toString();
        }
    }
    return QString();
}

QString SubscriptionPlanCatalog::planLabelForId(const QString &toolKey, const QString &planIdOrLabel) const
{
    const QString normalized = planIdOrLabel.trimmed().toLower();
    const QJsonArray plans = toolObject(toolKey).value(QStringLiteral("plans")).toArray();
    if (normalized.isEmpty() && !plans.isEmpty()) {
        return plans.first().toObject().value(QStringLiteral("label")).toString();
    }
    for (const QJsonValue &entry : plans) {
        const QJsonObject plan = entry.toObject();
        if (plan.value(QStringLiteral("id")).toString().toLower() == normalized
            || plan.value(QStringLiteral("label")).toString().toLower() == normalized) {
            return plan.value(QStringLiteral("label")).toString();
        }
    }
    return QString();
}

QVariantMap SubscriptionPlanCatalog::plan(const QString &toolKey, const QString &planIdOrLabel) const
{
    return planObject(toolKey, planIdOrLabel).toVariantMap();
}

QVariantList SubscriptionPlanCatalog::quotaWindows(const QString &toolKey, const QString &planIdOrLabel) const
{
    QVariantList result;
    const QJsonArray windows = planObject(toolKey, planIdOrLabel).value(QStringLiteral("quotaWindows")).toArray();
    for (const QJsonValue &entry : windows) {
        result << entry.toObject().toVariantMap();
    }
    return result;
}

QVariantMap SubscriptionPlanCatalog::price(const QString &toolKey, const QString &planIdOrLabel) const
{
    return planObject(toolKey, planIdOrLabel).value(QStringLiteral("price")).toObject().toVariantMap();
}

QVariantMap SubscriptionPlanCatalog::billingMode(const QString &toolKey, const QString &modeId) const
{
    const QString normalized = modeId.trimmed().toLower();
    const QJsonArray modes = toolObject(toolKey).value(QStringLiteral("billingModes")).toArray();
    for (const QJsonValue &entry : modes) {
        const QJsonObject mode = entry.toObject();
        if (mode.value(QStringLiteral("id")).toString().toLower() == normalized) {
            return mode.toVariantMap();
        }
    }
    return QVariantMap();
}

QVariantList SubscriptionPlanCatalog::billingModeQuotaWindows(const QString &toolKey, const QString &modeId) const
{
    QVariantList result;
    const QString normalized = modeId.trimmed().toLower();
    const QJsonArray modes = toolObject(toolKey).value(QStringLiteral("billingModes")).toArray();
    QJsonArray windows;
    for (const QJsonValue &entry : modes) {
        const QJsonObject mode = entry.toObject();
        if (mode.value(QStringLiteral("id")).toString().toLower() == normalized) {
            windows = mode.value(QStringLiteral("quotaWindows")).toArray();
            break;
        }
    }
    for (const QJsonValue &entry : windows) {
        result << entry.toObject().toVariantMap();
    }
    return result;
}

QJsonObject SubscriptionPlanCatalog::toolObject(const QString &toolKey) const
{
    const QString normalized = toolKey.trimmed().toLower();
    const QJsonArray tools = rootObject().value(QStringLiteral("tools")).toArray();
    for (const QJsonValue &entry : tools) {
        const QJsonObject tool = entry.toObject();
        if (tool.value(QStringLiteral("key")).toString().toLower() == normalized) {
            return tool;
        }
    }
    return QJsonObject();
}

QJsonObject SubscriptionPlanCatalog::planObject(const QString &toolKey, const QString &planIdOrLabel) const
{
    const QString normalized = planIdOrLabel.trimmed().toLower();
    const QJsonArray plans = toolObject(toolKey).value(QStringLiteral("plans")).toArray();
    if (plans.isEmpty()) {
        return QJsonObject();
    }

    if (normalized.isEmpty()) {
        return plans.first().toObject();
    }

    for (const QJsonValue &entry : plans) {
        const QJsonObject plan = entry.toObject();
        if (plan.value(QStringLiteral("id")).toString().toLower() == normalized
            || plan.value(QStringLiteral("label")).toString().toLower() == normalized) {
            return plan;
        }
    }

    return plans.first().toObject();
}
