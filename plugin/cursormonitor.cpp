#include "cursormonitor.h"

#include <QDir>

CursorMonitor::CursorMonitor(QObject *parent)
    : LocalActivityMonitorBase(parent)
{
    const QString root = QDir::homePath() + QStringLiteral("/.cursor");
    setInstallExecutableNames({QStringLiteral("cursor")});
    setInstallPaths({root});
    setWatchedPaths({root});
}

QStringList CursorMonitor::availablePlans() const
{
    return catalogPlanLabels();
}

int CursorMonitor::defaultLimitForPlan(const QString &plan) const
{
    return catalogDefaultLimitForPlan(plan);
}

double CursorMonitor::defaultCostForPlan(const QString &plan) const
{
    return catalogDefaultCostForPlan(plan);
}

double CursorMonitor::subscriptionCost() const
{
    return defaultCostForPlan(planTier());
}
