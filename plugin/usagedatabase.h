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
     * Tracks usage count against limits for tools like Claude Code, Codex,
     * Copilot.
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
    Q_INVOKABLE void
    recordRateLimitEvent(const QString &provider, const QString &eventType, int percentUsed);
    Q_INVOKABLE bool recordProviderMetrics(const QString &provider, const QVariantList &metrics);

    /**
     * Query aggregated cost or token usage per day for the last 365 days.
     * Mode 0 = Daily Cost, Mode 1 = Tokens (Input + Output).
     * Returns a map with:
     * - maxIntensity: highest value across all days
     * - days: list of { "date": "YYYY-MM-DD", "value": ... }
     */
    Q_INVOKABLE QVariantMap getYearlyActivity(int mode) const;

    /**
     * Query the neutral output_tokens / input_tokens ratio per day for the
     * last N days across compatible snapshots with positive total input.
     * Returns a list of maps with: { "date": "YYYY-MM-DD", "value": ... }
     */
    Q_INVOKABLE QVariantList getEfficiencySeries(int days) const;

    /**
     * Query analyst-friendly summary metrics across all providers for the last N
     * days. Returns keys including:
     * - averageDailyCost, currentDailyCost, weekOverWeekPercent,
     * volatilityPercent
     * - anomalyCount, anomalies, topDrivers, topModels
     */
    Q_INVOKABLE QVariantMap getAnalystOverview(int days = 30) const;

    /**
     * Build one evidence-bound Analyst snapshot for an exact UTC range.
     *
     * This synchronous helper is intended for worker instances and contract
     * tests. QML must use requestAnalyst() so database work never blocks the UI
     * thread.
     */
    Q_INVOKABLE QVariantMap getAnalystSnapshot(const QDateTime &from,
                                               const QDateTime &to,
                                               const QString &currency = QString()) const;
    Q_INVOKABLE void requestAnalyst(const QString &requestId,
                                    const QDateTime &from,
                                    const QDateTime &to,
                                    const QString &currency = QString());

    /**
     * Query usage snapshots for a provider within a time range.
     * Returns a list of QVariantMap with keys: timestamp, inputTokens,
     * outputTokens, requestCount, cost, dailyCost, monthlyCost, rlRequests,
     * rlRequestsRemaining, rlTokens, rlTokensRemaining, costSource, usageSource,
     * currency, dataQuality.
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

    Q_INVOKABLE void requestHistory(const QString &requestId,
                                    const QString &provider,
                                    const QDateTime &from,
                                    const QDateTime &to);
    Q_INVOKABLE void requestComparison(const QString &requestId,
                                       const QStringList &names,
                                       const QDateTime &from,
                                       const QDateTime &to,
                                       const QString &source,
                                       const QString &metric,
                                       int bucketMinutes = 60);

    /**
     * Discover retained provider and subscription-tool history without
     * consulting the currently enabled runtime sources.
     *
     * Each source contains its stable database identity, source kind,
     * observation bounds, sample count, and only the metric kinds that have
     * compatible stored values.
     */
    Q_INVOKABLE QVariantList getHistoryCatalog() const;
    Q_INVOKABLE void requestHistoryCatalog(const QString &requestId);

    /**
     * Query one or more retained sources through the schema-v4 history
     * contract. Source maps require sourceKind ("provider" or "tool") and
     * dbName. Optional displayName, historyOnly, and stale fields are copied
     * into result metadata.
     *
     * The result contains ok/error metadata and bounded, gap-preserving
     * series. Comparisons fail closed when units, semantics, or currencies
     * are incompatible.
     */
    Q_INVOKABLE QVariantMap getHistorySeries(const QVariantList &sources,
                                             const QDateTime &from,
                                             const QDateTime &to,
                                             const QString &metric,
                                             int bucketMinutes = 60) const;
    Q_INVOKABLE void requestHistorySeries(const QString &requestId,
                                          const QVariantList &sources,
                                          const QDateTime &from,
                                          const QDateTime &to,
                                          const QString &metric,
                                          int bucketMinutes = 60);

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
     * Returns items with keys: name, points, latestValue, deltaPercent,
     * sampleCount. Each points entry has: timestamp, value.
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
     * Returns items with keys: name, points, latestValue, deltaPercent,
     * sampleCount. Each points entry has: timestamp, value.
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
    Q_INVOKABLE void
    requestExportAll(const QString &requestId, const QString &dirPath, const QStringList &formats);

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
    void historyReady(const QString &requestId, const QVariantMap &payload);
    void comparisonReady(const QString &requestId, const QVariantList &series);
    void historyCatalogReady(const QString &requestId, const QVariantList &sources);
    void historySeriesReady(const QString &requestId, const QVariantMap &payload);
    void analystReady(const QString &requestId, const QVariantMap &snapshot);
    void exportFinished(const QString &requestId, const QStringList &paths);

private:
    void initDatabase();
    void createTables();
    bool migrateToObservationSchemaV3();
    bool migrateToObservationSchemaV4();
    bool recordObservations(const QString &provider,
                            const QString &model,
                            qint64 inputTokens,
                            qint64 outputTokens,
                            int requestCount,
                            double cost,
                            const QString &currency,
                            const QString &costSource,
                            const QString &usageSource,
                            const QString &dataQuality);
    void ensureColumnExists(const QString &table, const QString &column, const QString &definition);

    QSqlDatabase m_db;
    QString m_connectionName;
    bool m_enabled = true;
    int m_retentionDays = 90;
    bool m_initialized = false;
    QString m_latestHistoryCatalogRequestId;
    QString m_latestHistorySeriesRequestId;
    QString m_latestAnalystRequestId;

    static std::atomic<int> s_instanceCounter;

    // Write throttling: minimum 60 seconds between writes per provider
    static constexpr int WRITE_THROTTLE_SECS = 60;
    QHash<QString, qint64> m_lastWriteTime;        // provider -> epoch seconds
    QHash<QString, QByteArray> m_lastWrittenState; // complete normalized provider state
};

#endif // USAGEDATABASE_H
