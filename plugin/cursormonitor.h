#ifndef CURSORMONITOR_H
#define CURSORMONITOR_H

#include "localactivitymonitorbase.h"

class CursorMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT

public:
    explicit CursorMonitor(QObject *parent = nullptr);

    QString toolName() const override { return QStringLiteral("Cursor"); }
    QString iconName() const override { return QStringLiteral("cursor-arrow"); }
    QString toolColor() const override { return QStringLiteral("#3b82f6"); }
    QString periodLabel() const override { return QStringLiteral("Monthly"); }

    Q_INVOKABLE QStringList availablePlans() const override;
    Q_INVOKABLE int defaultLimitForPlan(const QString &plan) const override;
    Q_INVOKABLE double defaultCostForPlan(const QString &plan) const override;

    double subscriptionCost() const override;
    bool hasSubscriptionCost() const override { return true; }

protected:
    UsagePeriod primaryPeriodType() const override { return Monthly; }
    QString catalogToolKey() const override { return QStringLiteral("cursor"); }
};

#endif // CURSORMONITOR_H
