#include "jetbrainsaimonitor.h"

#include <QDir>

JetBrainsAiMonitor::JetBrainsAiMonitor(QObject *parent)
    : LocalActivityMonitorBase(parent)
{
    const QString root = QDir::homePath() + QStringLiteral("/.config/JetBrains");
    setInstallPaths({root});
    setWatchedPaths({root});
}

QStringList JetBrainsAiMonitor::availablePlans() const
{
    return catalogPlanLabels();
}

int JetBrainsAiMonitor::defaultLimitForPlan(const QString &plan) const
{
    return catalogDefaultLimitForPlan(plan);
}

double JetBrainsAiMonitor::defaultCostForPlan(const QString &plan) const
{
    return catalogDefaultCostForPlan(plan);
}

double JetBrainsAiMonitor::subscriptionCost() const
{
    return defaultCostForPlan(planTier());
}
