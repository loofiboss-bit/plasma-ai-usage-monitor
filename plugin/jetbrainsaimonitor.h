#ifndef JETBRAINSAIMONITOR_H
#define JETBRAINSAIMONITOR_H

#include "localactivitymonitorbase.h"

class JetBrainsAiMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT

public:
    explicit JetBrainsAiMonitor(QObject *parent = nullptr);

    QString toolName() const override { return QStringLiteral("JetBrains AI"); }
    QString iconName() const override { return QStringLiteral("jetbrains-toolbox"); }
    QString toolColor() const override { return QStringLiteral("#f97316"); }
    QString periodLabel() const override { return QStringLiteral("Monthly"); }

    Q_INVOKABLE QStringList availablePlans() const override;
    Q_INVOKABLE int defaultLimitForPlan(const QString &plan) const override;
    Q_INVOKABLE double defaultCostForPlan(const QString &plan) const override;

    double subscriptionCost() const override;
    bool hasSubscriptionCost() const override { return true; }

protected:
    UsagePeriod primaryPeriodType() const override { return Monthly; }
    QString catalogToolKey() const override { return QStringLiteral("jetbrains-ai"); }
};

#endif // JETBRAINSAIMONITOR_H
