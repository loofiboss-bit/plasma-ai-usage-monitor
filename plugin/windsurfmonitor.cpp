#include "windsurfmonitor.h"

#include <QDir>

WindsurfMonitor::WindsurfMonitor(QObject *parent)
    : LocalActivityMonitorBase(parent)
{
    const QString root = QDir::homePath() + QStringLiteral("/.codeium");
    setInstallExecutableNames({QStringLiteral("windsurf")});
    setInstallPaths({root});
    setWatchedPaths({root});
}

QStringList WindsurfMonitor::availablePlans() const
{
    return catalogPlanLabels();
}

int WindsurfMonitor::defaultLimitForPlan(const QString &plan) const
{
    return catalogDefaultLimitForPlan(plan);
}

double WindsurfMonitor::defaultCostForPlan(const QString &plan) const
{
    return catalogDefaultCostForPlan(plan);
}

double WindsurfMonitor::subscriptionCost() const
{
    return defaultCostForPlan(planTier());
}
