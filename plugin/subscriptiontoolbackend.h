#ifndef SUBSCRIPTIONTOOLBACKEND_H
#define SUBSCRIPTIONTOOLBACKEND_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <QJsonObject>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * Abstract base class for subscription-based AI coding tool monitors.
 *
 * Unlike ProviderBackend (which tracks API billing/tokens), this class
 * tracks usage counts against fixed subscription limits with rolling
 * time windows (5-hour, daily, weekly, monthly).
 *
 * Subclasses implement tool-specific detection and monitoring logic
 * for tools like Claude Code, OpenAI Codex CLI, and GitHub Copilot.
 *
 * v2.3: Added browser-sync support. When enabled, the widget extracts
 * session cookies from the user's browser to call internal service APIs
 * for accurate usage data (experimental/optional feature).
 */
class SubscriptionToolBackend : public QObject
{
    Q_OBJECT

    // Identity
    Q_PROPERTY(QString toolName READ toolName CONSTANT)
    Q_PROPERTY(QString iconName READ iconName CONSTANT)
    Q_PROPERTY(QString toolColor READ toolColor CONSTANT)

    // State
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool installed READ isInstalled NOTIFY installedChanged)
    Q_PROPERTY(QString planTier READ planTier WRITE setPlanTier NOTIFY planTierChanged)

    // Usage tracking — primary window
    Q_PROPERTY(int usageCount READ usageCount NOTIFY usageUpdated)
    Q_PROPERTY(int usageLimit READ usageLimit WRITE setUsageLimit NOTIFY usageLimitChanged)
    Q_PROPERTY(double percentUsed READ percentUsed NOTIFY usageUpdated)
    Q_PROPERTY(bool limitReached READ isLimitReached NOTIFY usageUpdated)
    Q_PROPERTY(QString periodLabel READ periodLabel CONSTANT)

    // Secondary usage window (e.g., weekly limit alongside 5-hour session)
    Q_PROPERTY(int secondaryUsageCount READ secondaryUsageCount NOTIFY usageUpdated)
    Q_PROPERTY(int secondaryUsageLimit READ secondaryUsageLimit WRITE setSecondaryUsageLimit NOTIFY usageLimitChanged)
    Q_PROPERTY(double secondaryPercentUsed READ secondaryPercentUsed NOTIFY usageUpdated)
    Q_PROPERTY(bool secondaryLimitReached READ isSecondaryLimitReached NOTIFY usageUpdated)
    Q_PROPERTY(QString secondaryPeriodLabel READ secondaryPeriodLabel CONSTANT)
    Q_PROPERTY(bool hasSecondaryLimit READ hasSecondaryLimit CONSTANT)
    Q_PROPERTY(QVariantList quotaWindows READ quotaWindows NOTIFY quotaWindowsChanged)

    // Time tracking
    Q_PROPERTY(QDateTime periodStart READ periodStart NOTIFY usageUpdated)
    Q_PROPERTY(QDateTime periodEnd READ periodEnd NOTIFY usageUpdated)
    Q_PROPERTY(int monthlyResetDay READ monthlyResetDay WRITE setMonthlyResetDay NOTIFY usageLimitChanged)
    Q_PROPERTY(int secondsUntilReset READ secondsUntilReset NOTIFY usageUpdated)
    Q_PROPERTY(QString timeUntilReset READ timeUntilReset NOTIFY usageUpdated)
    Q_PROPERTY(QDateTime lastActivity READ lastActivity NOTIFY usageUpdated)
    Q_PROPERTY(QDateTime secondaryPeriodEnd READ secondaryPeriodEnd NOTIFY usageUpdated)
    Q_PROPERTY(int secondarySecondsUntilReset READ secondarySecondsUntilReset NOTIFY usageUpdated)
    Q_PROPERTY(QString secondaryTimeUntilReset READ secondaryTimeUntilReset NOTIFY usageUpdated)

    // Session tracking (current active session percentage — from API sync)
    Q_PROPERTY(double sessionPercentUsed READ sessionPercentUsed NOTIFY usageUpdated)
    Q_PROPERTY(bool hasSessionInfo READ hasSessionInfo NOTIFY usageUpdated)

    // Extra / metered usage (e.g., Claude extra spending, Copilot metered)
    Q_PROPERTY(bool hasExtraUsage READ hasExtraUsage NOTIFY usageUpdated)
    Q_PROPERTY(double extraUsageSpent READ extraUsageSpent NOTIFY usageUpdated)
    Q_PROPERTY(double extraUsageLimit READ extraUsageLimit NOTIFY usageUpdated)
    Q_PROPERTY(double extraUsagePercent READ extraUsagePercent NOTIFY usageUpdated)
    Q_PROPERTY(QDateTime extraUsageResetDate READ extraUsageResetDate NOTIFY usageUpdated)
    Q_PROPERTY(QString currencySymbol READ currencySymbol NOTIFY usageUpdated)

    // Subscription cost
    Q_PROPERTY(double subscriptionCost READ subscriptionCost NOTIFY usageUpdated)
    Q_PROPERTY(bool hasSubscriptionCost READ hasSubscriptionCost NOTIFY usageUpdated)

    // Browser sync
    Q_PROPERTY(bool syncEnabled READ isSyncEnabled WRITE setSyncEnabled NOTIFY syncEnabledChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY syncStatusChanged)
    Q_PROPERTY(QDateTime lastSyncTime READ lastSyncTime NOTIFY syncStatusChanged)
    Q_PROPERTY(bool syncing READ isSyncing NOTIFY syncStatusChanged)

    // Tertiary usage (e.g., Codex code‐review cap)
    Q_PROPERTY(bool hasTertiaryLimit READ hasTertiaryLimit NOTIFY usageUpdated)
    Q_PROPERTY(QString tertiaryPeriodLabel READ tertiaryPeriodLabel CONSTANT)
    Q_PROPERTY(double tertiaryPercentRemaining READ tertiaryPercentRemaining NOTIFY usageUpdated)
    Q_PROPERTY(QDateTime tertiaryResetDate READ tertiaryResetDate NOTIFY usageUpdated)

    // Credits (e.g., Codex remaining credits)
    Q_PROPERTY(int remainingCredits READ remainingCredits NOTIFY usageUpdated)
    Q_PROPERTY(bool hasCredits READ hasCredits NOTIFY usageUpdated)

public:
    enum UsagePeriod {
        FiveHour,   // 5-hour rolling window (Claude Code, Codex)
        Daily,      // 24-hour from midnight
        Weekly,     // 7-day rolling window
        Monthly     // Calendar month (resets 1st)
    };
    Q_ENUM(UsagePeriod)

    explicit SubscriptionToolBackend(QObject *parent = nullptr);
    ~SubscriptionToolBackend() override;

    // Identity (pure virtual — subclasses must implement)
    virtual QString toolName() const = 0;
    virtual QString iconName() const = 0;
    virtual QString toolColor() const = 0;

    // State
    bool isEnabled() const;
    void setEnabled(bool enabled);
    bool isInstalled() const;
    QString planTier() const;
    void setPlanTier(const QString &tier);

    // Usage — primary
    int usageCount() const;
    int usageLimit() const;
    void setUsageLimit(int limit);
    double percentUsed() const;
    bool isLimitReached() const;
    virtual QString periodLabel() const = 0;

    // Secondary window
    int secondaryUsageCount() const;
    int secondaryUsageLimit() const;
    void setSecondaryUsageLimit(int limit);
    double secondaryPercentUsed() const;
    bool isSecondaryLimitReached() const;
    virtual QString secondaryPeriodLabel() const;
    virtual bool hasSecondaryLimit() const;

    // Time — primary
    QDateTime periodStart() const;
    QDateTime periodEnd() const;
    int monthlyResetDay() const;
    void setMonthlyResetDay(int day);
    int secondsUntilReset() const;
    QString timeUntilReset() const;
    QDateTime lastActivity() const;

    // Time — secondary
    QDateTime secondaryPeriodEnd() const;
    int secondarySecondsUntilReset() const;
    QString secondaryTimeUntilReset() const;

    // Session (from API)
    double sessionPercentUsed() const;
    bool hasSessionInfo() const;

    // Extra / metered usage
    bool hasExtraUsage() const;
    double extraUsageSpent() const;
    double extraUsageLimit() const;
    double extraUsagePercent() const;
    QDateTime extraUsageResetDate() const;
    QString currencySymbol() const;

    // Subscription cost
    virtual double subscriptionCost() const;
    virtual bool hasSubscriptionCost() const;

    // Browser sync
    bool isSyncEnabled() const;
    void setSyncEnabled(bool enabled);
    QString syncStatus() const;
    QDateTime lastSyncTime() const;
    bool isSyncing() const;

    // Tertiary (code review, etc.)
    virtual bool hasTertiaryLimit() const;
    virtual QString tertiaryPeriodLabel() const;
    double tertiaryPercentRemaining() const;
    QDateTime tertiaryResetDate() const;

    // Credits
    virtual bool hasCredits() const;
    int remainingCredits() const;

    QVariantList quotaWindows() const;

    // Actions
    Q_INVOKABLE void incrementUsage();
    Q_INVOKABLE void resetUsage();
    Q_INVOKABLE virtual void checkToolInstalled() = 0;
    Q_INVOKABLE virtual void detectActivity() = 0;
    Q_INVOKABLE virtual void syncFromBrowser(const QString &cookieHeader, int browserType);

    // Plan presets (subclasses populate these)
    Q_INVOKABLE virtual QStringList availablePlans() const = 0;
    Q_INVOKABLE virtual int defaultLimitForPlan(const QString &plan) const = 0;
    Q_INVOKABLE virtual int defaultSecondaryLimitForPlan(const QString &plan) const;
    Q_INVOKABLE virtual double defaultCostForPlan(const QString &plan) const;
    Q_INVOKABLE QString planIdForLabel(const QString &planLabelOrId) const;

Q_SIGNALS:
    void enabledChanged();
    void installedChanged();
    void planTierChanged();
    void usageUpdated();
    void usageLimitChanged();
    void syncEnabledChanged();
    void syncStatusChanged();
    void syncCompleted(bool success, const QString &message);
    void syncDiagnostic(const QString &toolName, const QString &code, const QString &message);
    void quotaWindowsChanged();
    void limitWarning(const QString &tool, int percentUsed);
    void usageLimitReached(const QString &tool);
    void activityDetected(const QString &tool);

protected:
    void setInstalled(bool installed);
    void setUsageCount(int count);
    void setSecondaryUsageCount(int count);
    void setPeriodStart(const QDateTime &start);
    void setSecondaryPeriodStart(const QDateTime &start);
    void setLastActivity(const QDateTime &time);

    // Sync helpers for subclasses
    void setSyncing(bool syncing);
    void setSyncStatus(const QString &status);
    void setLastSyncTime(const QDateTime &time);
    void setSessionPercentUsed(double pct);
    void setHasSessionInfo(bool has);
    void setExtraUsageSpent(double spent);
    void setExtraUsageLimit(double limit);
    void setExtraUsageResetDate(const QDateTime &date);
    void setCurrencySymbol(const QString &symbol);
    void setHasExtraUsage(bool has);
    void setTertiaryPercentRemaining(double pct);
    void setTertiaryResetDate(const QDateTime &date);
    void setRemainingCredits(int credits);
    void setSubscriptionCostValue(double cost);

    // Period management
    virtual UsagePeriod primaryPeriodType() const = 0;
    virtual UsagePeriod secondaryPeriodType() const;
    virtual QString catalogToolKey() const;
    virtual QString catalogBillingMode() const;
    QStringList catalogPlanLabels() const;
    int catalogDefaultLimitForPlan(const QString &plan) const;
    int catalogDefaultSecondaryLimitForPlan(const QString &plan) const;
    double catalogDefaultCostForPlan(const QString &plan) const;
    void checkAndResetPeriod();
    QDateTime calculatePeriodEnd(UsagePeriod period, const QDateTime &start) const;

    QNetworkAccessManager *networkManager();

private:
    void checkLimitWarnings();

    bool m_enabled = false;
    bool m_installed = false;
    QString m_planTier;

    int m_usageCount = 0;
    int m_usageLimit = 0;
    int m_secondaryUsageCount = 0;
    int m_secondaryUsageLimit = 0;

    QDateTime m_periodStart;
    QDateTime m_secondaryPeriodStart;
    QDateTime m_lastActivity;
    int m_monthlyResetDay = 1;

    // Session & extra usage (from sync)
    double m_sessionPercentUsed = 0.0;
    bool m_hasSessionInfo = false;
    bool m_hasExtraUsage = false;
    double m_extraUsageSpent = 0.0;
    double m_extraUsageLimit = 0.0;
    QDateTime m_extraUsageResetDate;
    QString m_currencySymbol = QStringLiteral("$");
    double m_subscriptionCost = 0.0;

    // Tertiary window
    double m_tertiaryPercentRemaining = 0.0;
    QDateTime m_tertiaryResetDate;

    // Credits
    int m_remainingCredits = 0;

    // Sync state
    bool m_syncEnabled = false;
    bool m_syncing = false;
    QString m_syncStatus;
    QDateTime m_lastSyncTime;

    QTimer *m_resetCheckTimer;
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // SUBSCRIPTIONTOOLBACKEND_H
