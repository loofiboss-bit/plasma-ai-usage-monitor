#ifndef WINDSURFMONITOR_H
#define WINDSURFMONITOR_H

#include "localactivitymonitorbase.h"

class WindsurfMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit WindsurfMonitor(QObject *parent = nullptr);

    QString toolName() const override { return QStringLiteral("Windsurf"); }
    QString iconName() const override { return QStringLiteral("applications-development"); }
    QString toolColor() const override { return QStringLiteral("#06b6d4"); }
    QString periodLabel() const override { return QStringLiteral("Monthly"); }

    Q_INVOKABLE QStringList availablePlans() const override;
    Q_INVOKABLE int defaultLimitForPlan(const QString &plan) const override;
    Q_INVOKABLE double defaultCostForPlan(const QString &plan) const override;

    double subscriptionCost() const override;
    bool hasSubscriptionCost() const override { return true; }

protected:
    UsagePeriod primaryPeriodType() const override { return Monthly; }
    QString catalogToolKey() const override { return QStringLiteral("windsurf"); }
};

#endif // WINDSURFMONITOR_H
