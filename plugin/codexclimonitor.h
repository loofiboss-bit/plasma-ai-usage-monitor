#ifndef CODEXCLIMONITOR_H
#define CODEXCLIMONITOR_H

#include "localactivitymonitorbase.h"

/**
 * Monitor for OpenAI Codex CLI usage.
 *
 * Codex CLI uses a 5-hour rolling window for usage limits tied to the
 * ChatGPT subscription plan. Limits vary by plan and task type.
 *
 * No public API exists for checking remaining quota. This monitor:
 * 1. Detects if Codex CLI is installed (checks PATH + ~/.codex/)
 * 2. Watches ~/.codex/ for session activity
 * 3. Self-tracks usage counts in the local database
 * 4. Lets users set their plan tier to auto-fill limits
 * 5. (Optional) Syncs live quota from the local Codex login, with browser
 *    account cookies as a compatibility fallback
 *
 * Pricing and quota presets live in subscriptions-v1.json.
 *
 * Live sync fetches from ChatGPT internal APIs:
 * - 5-hour usage limit (primary)
 * - Weekly usage limit (secondary)
 * - Code review (tertiary)
 * - Remaining credits
 */
class CodexCliMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT

public:
    explicit CodexCliMonitor(QObject *parent = nullptr);

    // Identity
    QString toolName() const override { return QStringLiteral("Codex CLI"); }
    QString iconName() const override { return QStringLiteral("utilities-terminal"); }
    QString toolColor() const override { return QStringLiteral("#10A37F"); }

    // Period labels
    QString periodLabel() const override { return QStringLiteral("5-hour window"); }

    // Plan management
    Q_INVOKABLE QStringList availablePlans() const override;
    Q_INVOKABLE int defaultLimitForPlan(const QString &plan) const override;
    Q_INVOKABLE int defaultSecondaryLimitForPlan(const QString &plan) const override;

    // Live sync
    Q_INVOKABLE void syncFromBrowser(const QString &cookieHeader, int browserType) override;

    // Pure parser kept public for deterministic regression tests.
    static QVariantList quotaWindowsFromUsagePayload(const QByteArray &payload);

    // Cost
    double subscriptionCost() const override;
    double defaultCostForPlan(const QString &plan) const override;
    bool hasSubscriptionCost() const override { return true; }

    // Tertiary & credits overrides
    bool hasTertiaryLimit() const override { return m_hasTertiary; }
    QString tertiaryPeriodLabel() const override { return QStringLiteral("Code review"); }
    bool hasCredits() const override { return m_hasCreditsData; }
    bool hasSecondaryLimit() const override { return true; }
    QString secondaryPeriodLabel() const override { return QStringLiteral("Weekly"); }

protected:
    UsagePeriod primaryPeriodType() const override { return FiveHour; }
    UsagePeriod secondaryPeriodType() const override { return Weekly; }
    QString catalogToolKey() const override { return QStringLiteral("codex-cli"); }

private:
    QString codexConfigDir() const;
    bool fetchCodexUsage(const QString &cookieHeader);
    void fetchAccountCheck(const QString &cookieHeader);

    bool m_hasTertiary = false;
    bool m_hasCreditsData = false;
};

#endif // CODEXCLIMONITOR_H
