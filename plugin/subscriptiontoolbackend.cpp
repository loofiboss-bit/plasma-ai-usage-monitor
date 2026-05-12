#include "subscriptiontoolbackend.h"
#include "subscriptionplancatalog.h"
#include <QDate>
#include <QDebug>
#include <QTimeZone>
#include <QNetworkAccessManager>
#include <QVariantMap>

namespace {
QString sourceBadge(const QVariantMap &row)
{
    const QString precision = row.value(QStringLiteral("precision")).toString();
    const QString source = row.value(QStringLiteral("source")).toString();

    if (row.value(QStringLiteral("needsManualReview"), false).toBool()
        || row.value(QStringLiteral("sourceConflict"), false).toBool()
        || precision == QLatin1String("needs_manual_review")) {
        return QStringLiteral("Needs review");
    }
    if (source == QLatin1String("browser_sync") || precision == QLatin1String("browser_sync_actual")) {
        return QStringLiteral("Synced");
    }
    if (source == QLatin1String("user_config")) {
        return QStringLiteral("Custom");
    }
    if (source == QLatin1String("local_activity") || precision == QLatin1String("self_tracked_local")) {
        return QStringLiteral("Self-tracked");
    }
    if (precision == QLatin1String("official_exact")) {
        return QStringLiteral("Official");
    }
    if (precision == QLatin1String("official_range")) {
        return QStringLiteral("Official range");
    }
    if (precision == QLatin1String("official_approx")) {
        return QStringLiteral("Official approx");
    }
    if (precision == QLatin1String("official_qualitative")) {
        return QStringLiteral("Official note");
    }
    if (precision == QLatin1String("estimated")) {
        return QStringLiteral("Estimated");
    }
    return QStringLiteral("Unknown");
}

QString periodKind(SubscriptionToolBackend::UsagePeriod period)
{
    switch (period) {
    case SubscriptionToolBackend::FiveHour:
        return QStringLiteral("rolling_5h");
    case SubscriptionToolBackend::Daily:
        return QStringLiteral("daily_budget");
    case SubscriptionToolBackend::Weekly:
        return QStringLiteral("rolling_weekly");
    case SubscriptionToolBackend::Monthly:
        return QStringLiteral("monthly_utc");
    }
    return QStringLiteral("custom");
}

bool isNumericQuotaUnit(const QString &unit)
{
    return unit == QLatin1String("messages")
        || unit == QLatin1String("requests")
        || unit == QLatin1String("tasks")
        || unit == QLatin1String("reviews");
}

int localActivityFallbackLimit(const QString &toolKey, const QString &planId)
{
    if (toolKey == QLatin1String("claude-code")) {
        if (planId == QLatin1String("max_20x")) return 2000;
        if (planId == QLatin1String("max_5x")) return 500;
        return 100;
    }
    if (toolKey == QLatin1String("codex-cli")) {
        if (planId == QLatin1String("pro")) return 1000;
        if (planId == QLatin1String("pro_100")) return 500;
        if (planId == QLatin1String("plus")) return 100;
        if (planId == QLatin1String("free") || planId == QLatin1String("go")) return 25;
        return 500;
    }
    if (toolKey == QLatin1String("cursor")) {
        if (planId == QLatin1String("ultra")) return 10000;
        if (planId == QLatin1String("pro_plus")) return 1500;
        if (planId == QLatin1String("hobby")) return 50;
        return 500;
    }
    if (toolKey == QLatin1String("windsurf")) {
        if (planId == QLatin1String("max")) return 5000;
        if (planId == QLatin1String("free")) return 25;
        return 500;
    }
    if (toolKey == QLatin1String("jetbrains-ai")) {
        if (planId == QLatin1String("ai_ultimate") || planId == QLatin1String("ai_enterprise")) return 70;
        if (planId == QLatin1String("ai_pro")) return 20;
        return 3;
    }
    return 0;
}

QVariantMap usageRow(const QString &kind,
                     const QString &label,
                     const QString &unit,
                     int used,
                     int limit,
                     const QDateTime &resetAt,
                     const QString &timeUntilReset,
                     const QString &source,
                     const QString &precision)
{
    QVariantMap row;
    row.insert(QStringLiteral("kind"), kind);
    row.insert(QStringLiteral("label"), label);
    row.insert(QStringLiteral("unit"), unit);
    row.insert(QStringLiteral("used"), qMax(0, used));
    if (limit > 0) {
        row.insert(QStringLiteral("limit"), limit);
        row.insert(QStringLiteral("remaining"), qMax(0, limit - used));
        row.insert(QStringLiteral("percentUsed"), qBound(0.0, (static_cast<double>(used) / limit) * 100.0, 999.0));
    }
    if (resetAt.isValid()) {
        row.insert(QStringLiteral("resetAt"), resetAt.toUTC().toString(Qt::ISODate));
    }
    if (!timeUntilReset.isEmpty()) {
        row.insert(QStringLiteral("timeUntilReset"), timeUntilReset);
    }
    row.insert(QStringLiteral("source"), source);
    row.insert(QStringLiteral("precision"), precision);
    row.insert(QStringLiteral("visibleByDefault"), true);
    row.insert(QStringLiteral("badge"), sourceBadge(row));
    return row;
}
} // namespace

SubscriptionToolBackend::SubscriptionToolBackend(QObject *parent)
    : QObject(parent)
    , m_resetCheckTimer(new QTimer(this))
{
    // Check for period resets every 60 seconds
    m_resetCheckTimer->setInterval(60 * 1000);
    connect(m_resetCheckTimer, &QTimer::timeout, this, &SubscriptionToolBackend::checkAndResetPeriod);
    connect(this, &SubscriptionToolBackend::usageUpdated, this, &SubscriptionToolBackend::quotaWindowsChanged);
    connect(this, &SubscriptionToolBackend::usageLimitChanged, this, &SubscriptionToolBackend::quotaWindowsChanged);
    connect(this, &SubscriptionToolBackend::planTierChanged, this, &SubscriptionToolBackend::quotaWindowsChanged);
    connect(this, &SubscriptionToolBackend::syncStatusChanged, this, &SubscriptionToolBackend::quotaWindowsChanged);
}

SubscriptionToolBackend::~SubscriptionToolBackend() = default;

// --- State ---

bool SubscriptionToolBackend::isEnabled() const { return m_enabled; }
void SubscriptionToolBackend::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (enabled) {
            checkToolInstalled();
            if (m_periodStart.isNull()) {
                m_periodStart = QDateTime::currentDateTimeUtc();
                m_secondaryPeriodStart = QDateTime::currentDateTimeUtc();
            }
            m_resetCheckTimer->start();
        } else {
            m_resetCheckTimer->stop();
        }
        Q_EMIT enabledChanged();
    }
}

bool SubscriptionToolBackend::isInstalled() const { return m_installed; }
void SubscriptionToolBackend::setInstalled(bool installed)
{
    if (m_installed != installed) {
        m_installed = installed;
        Q_EMIT installedChanged();
    }
}

QString SubscriptionToolBackend::planTier() const { return m_planTier; }
void SubscriptionToolBackend::setPlanTier(const QString &tier)
{
    if (m_planTier != tier) {
        m_planTier = tier;
        Q_EMIT planTierChanged();
        Q_EMIT usageUpdated();
    }
}

// --- Usage ---

int SubscriptionToolBackend::usageCount() const { return m_usageCount; }
void SubscriptionToolBackend::setUsageCount(int count)
{
    const int normalized = qMax(0, count);
    if (m_usageCount != normalized) {
        m_usageCount = normalized;
        Q_EMIT usageUpdated();
    }
}

int SubscriptionToolBackend::usageLimit() const { return m_usageLimit; }
void SubscriptionToolBackend::setUsageLimit(int limit)
{
    if (m_usageLimit != limit) {
        m_usageLimit = limit;
        Q_EMIT usageLimitChanged();
        Q_EMIT usageUpdated();
    }
}

double SubscriptionToolBackend::percentUsed() const
{
    if (m_usageLimit <= 0) return 0.0;
    return (static_cast<double>(m_usageCount) / m_usageLimit) * 100.0;
}

bool SubscriptionToolBackend::isLimitReached() const
{
    return m_usageLimit > 0 && m_usageCount >= m_usageLimit;
}

int SubscriptionToolBackend::warningThreshold() const { return m_warningThreshold; }
void SubscriptionToolBackend::setWarningThreshold(int threshold)
{
    const int normalized = qBound(1, threshold, 100);
    if (m_warningThreshold != normalized) {
        m_warningThreshold = normalized;
        Q_EMIT warningThresholdChanged();
        checkLimitWarnings();
    }
}

int SubscriptionToolBackend::criticalThreshold() const { return m_criticalThreshold; }
void SubscriptionToolBackend::setCriticalThreshold(int threshold)
{
    const int normalized = qBound(1, threshold, 100);
    if (m_criticalThreshold != normalized) {
        m_criticalThreshold = normalized;
        Q_EMIT warningThresholdChanged();
        checkLimitWarnings();
    }
}

// --- Secondary Window ---

int SubscriptionToolBackend::secondaryUsageCount() const { return m_secondaryUsageCount; }
void SubscriptionToolBackend::setSecondaryUsageCount(int count)
{
    const int normalized = qMax(0, count);
    if (m_secondaryUsageCount != normalized) {
        m_secondaryUsageCount = normalized;
        Q_EMIT usageUpdated();
    }
}

int SubscriptionToolBackend::secondaryUsageLimit() const { return m_secondaryUsageLimit; }
void SubscriptionToolBackend::setSecondaryUsageLimit(int limit)
{
    if (m_secondaryUsageLimit != limit) {
        m_secondaryUsageLimit = limit;
        Q_EMIT usageLimitChanged();
        Q_EMIT usageUpdated();
    }
}

double SubscriptionToolBackend::secondaryPercentUsed() const
{
    if (m_secondaryUsageLimit <= 0) return 0.0;
    return (static_cast<double>(m_secondaryUsageCount) / m_secondaryUsageLimit) * 100.0;
}

bool SubscriptionToolBackend::isSecondaryLimitReached() const
{
    return m_secondaryUsageLimit > 0 && m_secondaryUsageCount >= m_secondaryUsageLimit;
}

QString SubscriptionToolBackend::secondaryPeriodLabel() const { return QString(); }
bool SubscriptionToolBackend::hasSecondaryLimit() const { return false; }
SubscriptionToolBackend::UsagePeriod SubscriptionToolBackend::secondaryPeriodType() const { return Weekly; }
int SubscriptionToolBackend::defaultSecondaryLimitForPlan(const QString &) const { return 0; }

// --- Time ---

QDateTime SubscriptionToolBackend::periodStart() const { return m_periodStart; }
void SubscriptionToolBackend::setPeriodStart(const QDateTime &start)
{
    if (m_periodStart != start) {
        m_periodStart = start;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setSecondaryPeriodStart(const QDateTime &start)
{
    if (m_secondaryPeriodStart != start) {
        m_secondaryPeriodStart = start;
        Q_EMIT usageUpdated();
    }
}

QDateTime SubscriptionToolBackend::periodEnd() const
{
    return calculatePeriodEnd(primaryPeriodType(), m_periodStart);
}

int SubscriptionToolBackend::monthlyResetDay() const
{
    return m_monthlyResetDay;
}

void SubscriptionToolBackend::setMonthlyResetDay(int day)
{
    const int normalized = qBound(1, day, 28);
    if (m_monthlyResetDay != normalized) {
        m_monthlyResetDay = normalized;
        Q_EMIT usageLimitChanged();
        Q_EMIT usageUpdated();
    }
}

int SubscriptionToolBackend::secondsUntilReset() const
{
    QDateTime end = periodEnd();
    if (!end.isValid()) return 0;
    qint64 secs = QDateTime::currentDateTimeUtc().secsTo(end);
    return secs > 0 ? static_cast<int>(secs) : 0;
}

QString SubscriptionToolBackend::timeUntilReset() const
{
    int secs = secondsUntilReset();
    if (secs <= 0) return QStringLiteral("now");

    int hours = secs / 3600;
    int mins = (secs % 3600) / 60;

    if (hours > 24) {
        int days = hours / 24;
        return QStringLiteral("%1d %2h").arg(days).arg(hours % 24);
    }
    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(mins);
    }
    return QStringLiteral("%1m").arg(mins);
}

QDateTime SubscriptionToolBackend::lastActivity() const { return m_lastActivity; }
void SubscriptionToolBackend::setLastActivity(const QDateTime &time)
{
    if (m_lastActivity != time) {
        m_lastActivity = time;
        Q_EMIT usageUpdated();
    }
}

// --- Actions ---

void SubscriptionToolBackend::incrementUsage()
{
    checkAndResetPeriod();

    m_usageCount++;
    m_secondaryUsageCount++;
    m_lastActivity = QDateTime::currentDateTimeUtc();

    checkLimitWarnings();
    Q_EMIT usageUpdated();
    Q_EMIT activityDetected(toolName());
}

void SubscriptionToolBackend::resetUsage()
{
    m_usageCount = 0;
    m_periodStart = QDateTime::currentDateTimeUtc();
    Q_EMIT usageUpdated();
}

// --- Period Management ---

QDateTime SubscriptionToolBackend::calculatePeriodEnd(UsagePeriod period, const QDateTime &start) const
{
    if (!start.isValid()) return QDateTime();

    switch (period) {
    case FiveHour:
        return start.addSecs(5 * 3600);
    case Daily:
        return start.addDays(1);
    case Weekly:
        return start.addDays(7);
    case Monthly: {
        QDate d = start.date();
        QDate resetThisMonth(d.year(), d.month(), m_monthlyResetDay);
        if (d < resetThisMonth) {
            return QDateTime(resetThisMonth, QTime(0, 0), QTimeZone::utc());
        }

        QDate nextMonth = d.addMonths(1);
        QDate resetNextMonth(nextMonth.year(), nextMonth.month(), m_monthlyResetDay);
        return QDateTime(resetNextMonth, QTime(0, 0), QTimeZone::utc());
    }
    }
    return QDateTime();
}

void SubscriptionToolBackend::checkAndResetPeriod()
{
    QDateTime now = QDateTime::currentDateTimeUtc();

    // Check primary period
    QDateTime end = calculatePeriodEnd(primaryPeriodType(), m_periodStart);
    if (end.isValid() && now >= end) {
        m_usageCount = 0;
        m_periodStart = now;
        Q_EMIT usageUpdated();
    }

    // Check secondary period
    if (hasSecondaryLimit()) {
        QDateTime secEnd = calculatePeriodEnd(secondaryPeriodType(), m_secondaryPeriodStart);
        if (secEnd.isValid() && now >= secEnd) {
            m_secondaryUsageCount = 0;
            m_secondaryPeriodStart = now;
            Q_EMIT usageUpdated();
        }
    }
}

// --- Warning Checks ---

void SubscriptionToolBackend::checkLimitWarnings()
{
    if (m_usageLimit <= 0) return;

    double pct = percentUsed();

    if (m_usageCount >= m_usageLimit) {
        Q_EMIT usageLimitReached(toolName());
    } else if (pct >= m_warningThreshold) {
        Q_EMIT limitWarning(toolName(), static_cast<int>(pct));
    }

    // Check secondary limit too
    if (hasSecondaryLimit() && m_secondaryUsageLimit > 0) {
        if (m_secondaryUsageCount >= m_secondaryUsageLimit) {
            Q_EMIT usageLimitReached(toolName());
        }
    }
}

// --- Secondary Time ---

QDateTime SubscriptionToolBackend::secondaryPeriodEnd() const
{
    if (!hasSecondaryLimit()) return QDateTime();
    return calculatePeriodEnd(secondaryPeriodType(), m_secondaryPeriodStart);
}

int SubscriptionToolBackend::secondarySecondsUntilReset() const
{
    QDateTime end = secondaryPeriodEnd();
    if (!end.isValid()) return 0;
    qint64 secs = QDateTime::currentDateTimeUtc().secsTo(end);
    return secs > 0 ? static_cast<int>(secs) : 0;
}

QString SubscriptionToolBackend::secondaryTimeUntilReset() const
{
    int secs = secondarySecondsUntilReset();
    if (secs <= 0) return QStringLiteral("now");

    int hours = secs / 3600;
    int mins = (secs % 3600) / 60;

    if (hours > 24) {
        int days = hours / 24;
        return QStringLiteral("%1d %2h").arg(days).arg(hours % 24);
    }
    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(mins);
    }
    return QStringLiteral("%1m").arg(mins);
}

// --- Session Info ---

double SubscriptionToolBackend::sessionPercentUsed() const { return m_sessionPercentUsed; }
bool SubscriptionToolBackend::hasSessionInfo() const { return m_hasSessionInfo; }
void SubscriptionToolBackend::setSessionPercentUsed(double pct)
{
    const double normalized = qBound(0.0, pct, 100.0);
    if (!qFuzzyCompare(m_sessionPercentUsed + 1.0, normalized + 1.0)) {
        m_sessionPercentUsed = normalized;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setHasSessionInfo(bool has)
{
    if (m_hasSessionInfo != has) {
        m_hasSessionInfo = has;
        Q_EMIT usageUpdated();
    }
}

// --- Extra / Metered Usage ---

bool SubscriptionToolBackend::hasExtraUsage() const { return m_hasExtraUsage; }
double SubscriptionToolBackend::extraUsageSpent() const { return m_extraUsageSpent; }
double SubscriptionToolBackend::extraUsageLimit() const { return m_extraUsageLimit; }
double SubscriptionToolBackend::extraUsagePercent() const
{
    if (m_extraUsageLimit <= 0.0) return 0.0;
    return (m_extraUsageSpent / m_extraUsageLimit) * 100.0;
}
QDateTime SubscriptionToolBackend::extraUsageResetDate() const { return m_extraUsageResetDate; }
QString SubscriptionToolBackend::currencySymbol() const { return m_currencySymbol; }

void SubscriptionToolBackend::setExtraUsageSpent(double spent)
{
    const double normalized = qMax(0.0, spent);
    if (!qFuzzyCompare(m_extraUsageSpent + 1.0, normalized + 1.0)) {
        m_extraUsageSpent = normalized;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setExtraUsageLimit(double limit)
{
    const double normalized = qMax(0.0, limit);
    if (!qFuzzyCompare(m_extraUsageLimit + 1.0, normalized + 1.0)) {
        m_extraUsageLimit = normalized;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setExtraUsageResetDate(const QDateTime &date)
{
    if (m_extraUsageResetDate != date) {
        m_extraUsageResetDate = date;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setCurrencySymbol(const QString &symbol)
{
    if (m_currencySymbol != symbol) {
        m_currencySymbol = symbol;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setHasExtraUsage(bool has)
{
    if (m_hasExtraUsage != has) {
        m_hasExtraUsage = has;
        Q_EMIT usageUpdated();
    }
}

// --- Subscription Cost ---

double SubscriptionToolBackend::subscriptionCost() const { return m_subscriptionCost; }
bool SubscriptionToolBackend::hasSubscriptionCost() const { return false; }
void SubscriptionToolBackend::setSubscriptionCostValue(double cost)
{
    const double normalized = qMax(0.0, cost);
    if (!qFuzzyCompare(m_subscriptionCost + 1.0, normalized + 1.0)) {
        m_subscriptionCost = normalized;
        Q_EMIT usageUpdated();
    }
}
double SubscriptionToolBackend::defaultCostForPlan(const QString &plan) const { return catalogDefaultCostForPlan(plan); }

// --- Tertiary Usage ---

bool SubscriptionToolBackend::hasTertiaryLimit() const { return false; }
QString SubscriptionToolBackend::tertiaryPeriodLabel() const { return QString(); }
double SubscriptionToolBackend::tertiaryPercentRemaining() const { return m_tertiaryPercentRemaining; }
QDateTime SubscriptionToolBackend::tertiaryResetDate() const { return m_tertiaryResetDate; }
void SubscriptionToolBackend::setTertiaryPercentRemaining(double pct)
{
    const double normalized = qBound(0.0, pct, 100.0);
    if (!qFuzzyCompare(m_tertiaryPercentRemaining + 1.0, normalized + 1.0)) {
        m_tertiaryPercentRemaining = normalized;
        Q_EMIT usageUpdated();
    }
}
void SubscriptionToolBackend::setTertiaryResetDate(const QDateTime &date)
{
    if (m_tertiaryResetDate != date) {
        m_tertiaryResetDate = date;
        Q_EMIT usageUpdated();
    }
}

// --- Credits ---

bool SubscriptionToolBackend::hasCredits() const { return false; }
int SubscriptionToolBackend::remainingCredits() const { return m_remainingCredits; }
void SubscriptionToolBackend::setRemainingCredits(int credits)
{
    const int normalized = qMax(0, credits);
    if (m_remainingCredits != normalized) {
        m_remainingCredits = normalized;
        Q_EMIT usageUpdated();
    }
}

// --- Catalog-backed quota windows ---

QString SubscriptionToolBackend::catalogToolKey() const
{
    return QString();
}

QString SubscriptionToolBackend::catalogBillingMode() const
{
    return QString();
}

QStringList SubscriptionToolBackend::catalogPlanLabels() const
{
    const QString key = catalogToolKey();
    if (key.isEmpty()) {
        return QStringList();
    }
    return SubscriptionPlanCatalog::instance()->planLabelsForTool(key);
}

QString SubscriptionToolBackend::planIdForLabel(const QString &planLabelOrId) const
{
    const QString key = catalogToolKey();
    if (key.isEmpty()) {
        return planLabelOrId;
    }

    const QString id = SubscriptionPlanCatalog::instance()->planIdForLabel(key, planLabelOrId);
    return id.isEmpty() ? planLabelOrId : id;
}

int SubscriptionToolBackend::catalogDefaultLimitForPlan(const QString &plan) const
{
    const QString key = catalogToolKey();
    if (key.isEmpty()) {
        return 0;
    }

    const QVariantList windows = SubscriptionPlanCatalog::instance()->quotaWindows(key, plan);
    for (const QVariant &value : windows) {
        const QVariantMap row = value.toMap();
        if (!isNumericQuotaUnit(row.value(QStringLiteral("unit")).toString())) {
            continue;
        }
        if (row.contains(QStringLiteral("limit"))) {
            return row.value(QStringLiteral("limit")).toInt();
        }
    }

    const QString planId = SubscriptionPlanCatalog::instance()->planIdForLabel(key, plan);
    return localActivityFallbackLimit(key, planId.isEmpty() ? plan : planId);
}

int SubscriptionToolBackend::catalogDefaultSecondaryLimitForPlan(const QString &plan) const
{
    const QString key = catalogToolKey();
    if (key.isEmpty()) {
        return 0;
    }

    const QVariantList windows = SubscriptionPlanCatalog::instance()->quotaWindows(key, plan);
    for (const QVariant &value : windows) {
        const QVariantMap row = value.toMap();
        if (!row.value(QStringLiteral("kind")).toString().contains(QStringLiteral("weekly"))
            || !isNumericQuotaUnit(row.value(QStringLiteral("unit")).toString())) {
            continue;
        }
        if (row.contains(QStringLiteral("limit"))) {
            return row.value(QStringLiteral("limit")).toInt();
        }
    }
    return 0;
}

double SubscriptionToolBackend::catalogDefaultCostForPlan(const QString &plan) const
{
    const QString key = catalogToolKey();
    if (key.isEmpty()) {
        return 0.0;
    }

    const QVariantMap price = SubscriptionPlanCatalog::instance()->price(key, plan);
    if (price.contains(QStringLiteral("amount"))) {
        return price.value(QStringLiteral("amount")).toDouble();
    }
    if (price.value(QStringLiteral("precision")).toString() == QLatin1String("official_range")
        && price.contains(QStringLiteral("rangeMin"))) {
        return price.value(QStringLiteral("rangeMin")).toDouble();
    }
    return 0.0;
}

QVariantList SubscriptionToolBackend::quotaWindows() const
{
    QVariantList rows;
    const QString key = catalogToolKey();
    const auto appendRow = [&rows](QVariantMap row) {
        if (!row.contains(QStringLiteral("badge"))) {
            row.insert(QStringLiteral("badge"), sourceBadge(row));
        }
        rows << row;
    };

    if (m_hasSessionInfo) {
        QVariantMap row;
        row.insert(QStringLiteral("kind"), QStringLiteral("browser_session"));
        row.insert(QStringLiteral("label"), QStringLiteral("Current session"));
        row.insert(QStringLiteral("unit"), QStringLiteral("percent"));
        row.insert(QStringLiteral("percentUsed"), m_sessionPercentUsed);
        row.insert(QStringLiteral("source"), QStringLiteral("browser_sync"));
        row.insert(QStringLiteral("precision"), QStringLiteral("browser_sync_actual"));
        row.insert(QStringLiteral("visibleByDefault"), true);
        appendRow(row);
    }

    if (m_hasExtraUsage) {
        QVariantMap row;
        row.insert(QStringLiteral("kind"), QStringLiteral("extra_spend"));
        row.insert(QStringLiteral("label"), QStringLiteral("Extra usage"));
        row.insert(QStringLiteral("unit"), QStringLiteral("usd"));
        row.insert(QStringLiteral("used"), m_extraUsageSpent);
        row.insert(QStringLiteral("limit"), m_extraUsageLimit);
        if (m_extraUsageLimit > 0.0) {
            row.insert(QStringLiteral("remaining"), qMax(0.0, m_extraUsageLimit - m_extraUsageSpent));
            row.insert(QStringLiteral("percentUsed"), qBound(0.0, extraUsagePercent(), 999.0));
        }
        if (m_extraUsageResetDate.isValid()) {
            row.insert(QStringLiteral("resetAt"), m_extraUsageResetDate.toUTC().toString(Qt::ISODate));
        }
        row.insert(QStringLiteral("source"), QStringLiteral("browser_sync"));
        row.insert(QStringLiteral("precision"), QStringLiteral("browser_sync_actual"));
        row.insert(QStringLiteral("visibleByDefault"), true);
        appendRow(row);
    }

    if (hasTertiaryLimit()) {
        QVariantMap row;
        row.insert(QStringLiteral("kind"), QStringLiteral("code_review"));
        row.insert(QStringLiteral("label"), tertiaryPeriodLabel().isEmpty() ? QStringLiteral("Tertiary quota") : tertiaryPeriodLabel());
        row.insert(QStringLiteral("unit"), QStringLiteral("percent_remaining"));
        row.insert(QStringLiteral("percentRemaining"), m_tertiaryPercentRemaining);
        row.insert(QStringLiteral("percentUsed"), qBound(0.0, 100.0 - m_tertiaryPercentRemaining, 100.0));
        if (m_tertiaryResetDate.isValid()) {
            row.insert(QStringLiteral("resetAt"), m_tertiaryResetDate.toUTC().toString(Qt::ISODate));
        }
        row.insert(QStringLiteral("source"), QStringLiteral("browser_sync"));
        row.insert(QStringLiteral("precision"), QStringLiteral("browser_sync_actual"));
        row.insert(QStringLiteral("visibleByDefault"), true);
        appendRow(row);
    }

    if (hasCredits()) {
        QVariantMap row;
        row.insert(QStringLiteral("kind"), QStringLiteral("ai_credits"));
        row.insert(QStringLiteral("label"), QStringLiteral("Remaining credits"));
        row.insert(QStringLiteral("unit"), QStringLiteral("credits"));
        row.insert(QStringLiteral("remaining"), m_remainingCredits);
        row.insert(QStringLiteral("source"), QStringLiteral("browser_sync"));
        row.insert(QStringLiteral("precision"), QStringLiteral("browser_sync_actual"));
        row.insert(QStringLiteral("visibleByDefault"), true);
        appendRow(row);
    }

    if (!key.isEmpty()) {
        const QVariantMap tool = SubscriptionPlanCatalog::instance()->tool(key);
        const bool needsReview = tool.value(QStringLiteral("needsManualReview"), false).toBool();
        const bool sourceConflict = tool.value(QStringLiteral("sourceConflict"), false).toBool();

        const QVariantList billingRows = SubscriptionPlanCatalog::instance()->billingModeQuotaWindows(key, catalogBillingMode());
        for (const QVariant &value : billingRows) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("needsManualReview"), row.value(QStringLiteral("needsManualReview"), false).toBool() || needsReview);
            row.insert(QStringLiteral("sourceConflict"), row.value(QStringLiteral("sourceConflict"), false).toBool() || sourceConflict);
            appendRow(row);
        }

        const QVariantList catalogRows = SubscriptionPlanCatalog::instance()->quotaWindows(key, m_planTier);
        for (const QVariant &value : catalogRows) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("needsManualReview"), row.value(QStringLiteral("needsManualReview"), false).toBool() || needsReview);
            row.insert(QStringLiteral("sourceConflict"), row.value(QStringLiteral("sourceConflict"), false).toBool() || sourceConflict);
            appendRow(row);
        }
    }

    if (m_usageLimit > 0 || m_usageCount > 0) {
        appendRow(usageRow(periodKind(primaryPeriodType()),
                           periodLabel().isEmpty() ? QStringLiteral("Primary quota") : periodLabel(),
                           primaryPeriodType() == Monthly ? QStringLiteral("requests") : QStringLiteral("messages"),
                           m_usageCount,
                           m_usageLimit,
                           periodEnd(),
                           timeUntilReset(),
                           m_usageLimit > 0 ? QStringLiteral("user_config") : QStringLiteral("local_activity"),
                           m_usageLimit > 0 ? QStringLiteral("self_tracked_local") : QStringLiteral("estimated")));
    }

    if (hasSecondaryLimit() && (m_secondaryUsageLimit > 0 || m_secondaryUsageCount > 0)) {
        appendRow(usageRow(periodKind(secondaryPeriodType()),
                           secondaryPeriodLabel().isEmpty() ? QStringLiteral("Secondary quota") : secondaryPeriodLabel(),
                           QStringLiteral("messages"),
                           m_secondaryUsageCount,
                           m_secondaryUsageLimit,
                           secondaryPeriodEnd(),
                           secondaryTimeUntilReset(),
                           m_secondaryUsageLimit > 0 ? QStringLiteral("user_config") : QStringLiteral("local_activity"),
                           m_secondaryUsageLimit > 0 ? QStringLiteral("self_tracked_local") : QStringLiteral("estimated")));
    }

    return rows;
}

// --- Browser Sync ---

bool SubscriptionToolBackend::isSyncEnabled() const { return m_syncEnabled; }
void SubscriptionToolBackend::setSyncEnabled(bool enabled)
{
    if (m_syncEnabled != enabled) {
        m_syncEnabled = enabled;
        Q_EMIT syncEnabledChanged();
    }
}

QString SubscriptionToolBackend::syncStatus() const { return m_syncStatus; }
QDateTime SubscriptionToolBackend::lastSyncTime() const { return m_lastSyncTime; }
bool SubscriptionToolBackend::isSyncing() const { return m_syncing; }

void SubscriptionToolBackend::setSyncing(bool syncing)
{
    if (m_syncing != syncing) {
        m_syncing = syncing;
        Q_EMIT syncStatusChanged();
    }
}

void SubscriptionToolBackend::setSyncStatus(const QString &status)
{
    if (m_syncStatus != status) {
        m_syncStatus = status;
        Q_EMIT syncStatusChanged();
    }
}

void SubscriptionToolBackend::setLastSyncTime(const QDateTime &time)
{
    m_lastSyncTime = time;
    Q_EMIT syncStatusChanged();
}

void SubscriptionToolBackend::syncFromBrowser(const QString &cookieHeader, int browserType)
{
    Q_UNUSED(cookieHeader);
    Q_UNUSED(browserType);
    // Default implementation — subclasses override for actual sync
    setSyncStatus(QStringLiteral("Not supported"));
    const QString message = QStringLiteral("Sync not implemented for this tool");
    Q_EMIT syncDiagnostic(toolName(), QStringLiteral("not_supported"), message);
    Q_EMIT syncCompleted(false, message);
}

QNetworkAccessManager *SubscriptionToolBackend::networkManager()
{
    if (m_networkManager == nullptr) {
        m_networkManager = new QNetworkAccessManager(this);
    }
    return m_networkManager;
}
