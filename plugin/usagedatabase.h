#ifndef USAGEDATABASE_H
#define USAGEDATABASE_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QHash>
#include <atomic>

/**
 * SQLite database for persisting AI usage history.
 *
 * Stores periodic snapshots of provider usage data and rate limit events.
 * Supports configurable retention and querying by time range for charts.
 */
class UsageDatabase : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int retentionDays READ retentionDays WRITE setRetentionDays NOTIFY retentionDaysChanged)

public:
    explicit UsageDatabase(QObject *parent = nullptr);
    ~UsageDatabase() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);
    int retentionDays() const;
    void setRetentionDays(int days);

    /**
     * Record a usage snapshot for a provider.
     * Called automatically after each successful refresh.
     */
    Q_INVOKABLE void recordSnapshot(const QString &provider,
                                     qint64 inputTokens,
                                     qint64 outputTokens,
                                     int requestCount,
                                     double cost,
                                     double dailyCost,
                                     double monthlyCost,
                                     int rateLimitRequests,
                                     int rateLimitRequestsRemaining,
                                     int rateLimitTokens,
                                     int rateLimitTokensRemaining,
                                     const QString &model = QString(),
                                     bool isEstimatedCost = false,
                                     const QString &costSource = QStringLiteral("unknown"),
                                     const QString &usageSource = QStringLiteral("unknown"),
                                     const QString &currency = QStringLiteral("USD"),
                                     const QString &dataQuality = QStringLiteral("unknown"));

    /**
     * Record a usage snapshot for a subscription tool.
     * Tracks usage count against limits for tools like Claude Code, Codex, Copilot.
     */
    Q_INVOKABLE void recordToolSnapshot(const QString &toolName,
                                         int usageCount,
                                         int usageLimit,
                                         const QString &periodType,
                                         const QString &planTier,
                                         bool limitReached);

    /**
     * Record a rate limit event (hitting or approaching limits).
     */
    Q_INVOKABLE void recordRateLimitEvent(const QString &provider,
                                          const QString &eventType,
                                          int percentUsed);

    /**
     * Query aggregated cost or token usage per day for the last 365 days.
     * Mode 0 = Daily Cost, Mode 1 = Tokens (Input + Output).
     * Returns a map with:
     * - maxIntensity: highest value across all days
     * - days: list of { "date": "YYYY-MM-DD", "value": ... }
     */
    Q_INVOKABLE QVariantMap getYearlyActivity(int mode) const;

    /**
     * Query average token efficiency (output_tokens / input_tokens) per day
     * for the last N days across all providers.
     * Returns a list of maps with: { "date": "YYYY-MM-DD", "value": ... }
     */
    Q_INVOKABLE QVariantList getEfficiencySeries(int days) const;

    /**
     * Query analyst-friendly summary metrics across all providers for the last N days.
     * Returns keys including:
     * - averageDailyCost, currentDailyCost, weekOverWeekPercent, volatilityPercent
     * - anomalyCount, anomalies, topDrivers, topModels
     */
    Q_INVOKABLE QVariantMap getAnalystOverview(int days = 30) const;

    /**
     * Query usage snapshots for a provider within a time range.
     * Returns a list of QVariantMap with keys: timestamp, inputTokens, outputTokens,
     * requestCount, cost, dailyCost, monthlyCost, rlRequests, rlRequestsRemaining,
     * rlTokens, rlTokensRemaining, costSource, usageSource, currency, dataQuality.
     */
    Q_INVOKABLE QVariantList getSnapshots(const QString &provider,
                                           const QDateTime &from,
                                           const QDateTime &to) const;

    /**
     * Query cost data aggregated by day for a provider.
     * Returns a list of QVariantMap with keys: date, totalCost, maxDailyCost.
     */
    Q_INVOKABLE QVariantList getDailyCosts(const QString &provider,
                                            const QDateTime &from,
                                            const QDateTime &to) const;

    /**
     * Get summary statistics for a provider over a time range.
     * Returns a QVariantMap with keys: totalCost, avgDailyCost, maxDailyCost,
     * totalRequests, peakTokenUsage, snapshotCount.
     */
    Q_INVOKABLE QVariantMap getSummary(const QString &provider,
                                       const QDateTime &from,
                                       const QDateTime &to) const;

    /**
     * Get all providers that have recorded data.
     */
    Q_INVOKABLE QStringList getProviders() const;

    /**
     * Query subscription tool usage snapshots within a time range.
     * Returns a list of QVariantMap with keys: timestamp, usageCount,
     * usageLimit, periodType, planTier, limitReached, percentUsed.
     */
    Q_INVOKABLE QVariantList getToolSnapshots(const QString &toolName,
                                               const QDateTime &from,
                                               const QDateTime &to) const;

    /**
     * Query aggregated time series for one or more providers.
     * Returns items with keys: name, points, latestValue, deltaPercent, sampleCount.
     * Each points entry has: timestamp, value.
     *
     * Supported metrics: cost, tokens, requests, rateLimitUsed
     */
    Q_INVOKABLE QVariantList getProviderSeries(const QStringList &providers,
                                               const QDateTime &from,
                                               const QDateTime &to,
                                               const QString &metric,
                                               int bucketMinutes = 60) const;

    /**
     * Query aggregated time series for one or more subscription tools.
     * Returns items with keys: name, points, latestValue, deltaPercent, sampleCount.
     * Each points entry has: timestamp, value.
     *
     * Supported metrics: percentUsed, usageCount, remaining
     */
    Q_INVOKABLE QVariantList getToolSeries(const QStringList &tools,
                                           const QDateTime &from,
                                           const QDateTime &to,
                                           const QString &metric,
                                           int bucketMinutes = 60) const;

    /**
     * Get all subscription tool names that have recorded data.
     */
    Q_INVOKABLE QStringList getToolNames() const;

    /**
     * Export data as CSV for a provider within a time range.
     */
    Q_INVOKABLE QString exportCsv(const QString &provider,
                                   const QDateTime &from,
                                   const QDateTime &to) const;

    /**
     * Export data as JSON for a provider within a time range.
     */
    Q_INVOKABLE QString exportJson(const QString &provider,
                                    const QDateTime &from,
                                    const QDateTime &to) const;

    /**
     * Export all stored provider and subscription-tool history to timestamped
     * files in the target directory. Supported formats: "json", "csv".
     * Returns absolute paths written successfully.
     */
    Q_INVOKABLE QStringList exportAllToDirectory(const QString &dirPath,
                                                 const QStringList &formats) const;

    /**
     * Remove data older than retentionDays.
     */
    Q_INVOKABLE void pruneOldData();

    /**
     * Eagerly initialize the database.
     * Call early (e.g., Component.onCompleted) to avoid blocking on first write.
     */
    Q_INVOKABLE void init();

    /**
     * Get the total database size in bytes.
     */
    Q_INVOKABLE qint64 databaseSize() const;

Q_SIGNALS:
    void enabledChanged();
    void retentionDaysChanged();

private:
    void initDatabase();
    void createTables();
    void ensureColumnExists(const QString &table,
                            const QString &column,
                            const QString &definition);

    QSqlDatabase m_db;
    QString m_connectionName;
    bool m_enabled = true;
    int m_retentionDays = 90;
    bool m_initialized = false;

    static std::atomic<int> s_instanceCounter;

    // Write throttling: minimum 60 seconds between writes per provider
    static constexpr int WRITE_THROTTLE_SECS = 60;
    QHash<QString, qint64> m_lastWriteTime; // provider -> epoch seconds
    QHash<QString, double> m_lastWrittenCost; // provider -> last cost to detect changes
};

#endif // USAGEDATABASE_H
