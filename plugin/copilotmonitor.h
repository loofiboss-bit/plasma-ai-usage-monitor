#ifndef COPILOTMONITOR_H
#define COPILOTMONITOR_H

#include "localactivitymonitorbase.h"
#include <QNetworkReply>
#include <QDateTime>

/**
 * Monitor for GitHub Copilot usage.
 *
 * GitHub Copilot uses monthly premium request limits that reset on the 1st
 * of each month at 00:00 UTC. Standard model usage doesn't consume premium
 * requests; only premium/advanced models count.
 *
 * Optionally, if the user provides a GitHub PAT with appropriate scopes,
 * the monitor can query the GitHub REST API for organization-level metrics.
 *
 * Pricing, premium request, and AI credit mode metadata live in subscriptions-v1.json.
 */
class CopilotMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString githubToken READ githubToken WRITE setGithubToken NOTIFY githubTokenChanged)
    Q_PROPERTY(QString orgName READ orgName WRITE setOrgName NOTIFY orgNameChanged)
    Q_PROPERTY(QString billingMode READ billingMode WRITE setBillingMode NOTIFY billingModeChanged)
    Q_PROPERTY(QString usageSourceLabel READ usageSourceLabel NOTIFY billingModeChanged)
    Q_PROPERTY(bool hasOrgMetrics READ hasOrgMetrics NOTIFY orgMetricsUpdated)
    Q_PROPERTY(int orgActiveUsers READ orgActiveUsers NOTIFY orgMetricsUpdated)
    Q_PROPERTY(int orgTotalSeats READ orgTotalSeats NOTIFY orgMetricsUpdated)

public:
    explicit CopilotMonitor(QObject *parent = nullptr);

    // Identity
    QString toolName() const override { return QStringLiteral("GitHub Copilot"); }
    QString iconName() const override { return QStringLiteral("code-context"); }
    QString toolColor() const override { return QStringLiteral("#6e40c9"); }

    // Period labels
    QString periodLabel() const override { return QStringLiteral("Monthly"); }

    // Plan management
    Q_INVOKABLE QStringList availablePlans() const override;
    Q_INVOKABLE int defaultLimitForPlan(const QString &plan) const override;

    // Cost
    double subscriptionCost() const override;
    double defaultCostForPlan(const QString &plan) const override;
    bool hasSubscriptionCost() const override { return true; }

    // Tool detection
    Q_INVOKABLE void checkToolInstalled() override;
    Q_INVOKABLE void detectActivity() override;

    // GitHub API integration (optional)
    QString githubToken() const;
    void setGithubToken(const QString &token);
    QString orgName() const;
    void setOrgName(const QString &name);
    QString billingMode() const;
    void setBillingMode(const QString &mode);
    Q_INVOKABLE QString effectiveBillingMode() const;
    Q_INVOKABLE QString billingModeForDate(const QString &isoDate) const;
    QString usageSourceLabel() const;
    bool hasOrgMetrics() const;
    int orgActiveUsers() const;
    int orgTotalSeats() const;

    Q_INVOKABLE void fetchOrgMetrics();

Q_SIGNALS:
    void githubTokenChanged();
    void orgNameChanged();
    void billingModeChanged();
    void orgMetricsUpdated();

protected:
    UsagePeriod primaryPeriodType() const override { return Monthly; }
    QString catalogToolKey() const override { return QStringLiteral("github-copilot"); }
    QString catalogBillingMode() const override { return effectiveBillingMode(); }

private Q_SLOTS:
    void onBillingReply(QNetworkReply *reply);

private:
    QString m_githubToken;
    QString m_orgName;
    QString m_billingMode = QStringLiteral("auto");
    bool m_loggedDetectionFallback = false;

    bool m_hasOrgMetrics = false;
    int m_orgActiveUsers = 0;
    int m_orgTotalSeats = 0;
    int m_fetchGeneration = 0; // stale-reply guard for org metrics
};

#endif // COPILOTMONITOR_H
