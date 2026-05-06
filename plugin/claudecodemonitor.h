#ifndef CLAUDECODEMONITOR_H
#define CLAUDECODEMONITOR_H

#include "localactivitymonitorbase.h"

/**
 * Monitor for Claude Code CLI usage.
 *
 * Claude Code (Anthropic) uses rolling time windows:
 * - Primary: 5-hour session window
 * - Secondary: Weekly rolling window (separate Opus/non-Opus limits)
 *
 * v2.3: Added browser-sync support via Claude.ai internal APIs.
 * When enabled, extracts session cookies from Firefox to call:
 * - GET /api/account → get organization UUID
 * - GET /api/organizations/{uuid}/usage → session %, weekly limits
 * - GET /api/organizations/{uuid}/settings/billing → extra usage spend
 *
 * Pricing and quota presets live in subscriptions-v1.json.
 */
class ClaudeCodeMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT

public:
    explicit ClaudeCodeMonitor(QObject *parent = nullptr);

    // Identity
    QString toolName() const override { return QStringLiteral("Claude Code"); }
    QString iconName() const override { return QStringLiteral("utilities-terminal"); }
    QString toolColor() const override { return QStringLiteral("#D4A574"); }

    // Period labels
    QString periodLabel() const override { return QStringLiteral("5-hour session"); }
    QString secondaryPeriodLabel() const override { return QStringLiteral("Weekly"); }
    bool hasSecondaryLimit() const override { return true; }

    // Subscription cost
    bool hasSubscriptionCost() const override { return true; }
    double subscriptionCost() const override;

    // Plan management
    Q_INVOKABLE QStringList availablePlans() const override;
    Q_INVOKABLE int defaultLimitForPlan(const QString &plan) const override;
    Q_INVOKABLE int defaultSecondaryLimitForPlan(const QString &plan) const override;
    Q_INVOKABLE double defaultCostForPlan(const QString &plan) const override;

    // Browser sync
    Q_INVOKABLE void syncFromBrowser(const QString &cookieHeader, int browserType) override;

protected:
    UsagePeriod primaryPeriodType() const override { return FiveHour; }
    UsagePeriod secondaryPeriodType() const override { return Weekly; }
    QString catalogToolKey() const override { return QStringLiteral("claude-code"); }

private:
    QString claudeConfigDir() const;
    void fetchAccountInfo(const QString &cookieHeader);
    void fetchUsageData(const QString &orgUuid, const QString &cookieHeader);

    QString m_orgUuid;
};

#endif // CLAUDECODEMONITOR_H
