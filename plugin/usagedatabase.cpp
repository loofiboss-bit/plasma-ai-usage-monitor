#include "usagedatabase.h"
#include "analystquery.h"
#include "forecastcontract.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>

std::atomic<int> UsageDatabase::s_instanceCounter{0};

namespace {
constexpr int MAX_SERIES_POINTS = 240;

struct BucketAggregate {
    double sum = 0.0;
    int count = 0;
    QDateTime bucketStart;
};

struct HistorySample {
    QDateTime timestamp;
    QDateTime firstObservation;
    QDateTime lastObservation;
    double value = 0.0;
    int sampleCount = 1;
    QString unit;
    QString currency;
    QString semantic;
    QString source;
    QString quality;
    QString scope;
    QString window;
    QDateTime resetAt;
};

struct HistoryBucket {
    double sum = 0.0;
    double last = 0.0;
    int count = 0;
    QDateTime bucketStart;
};

struct HistorySegment {
    QString unit;
    QString currency;
    QString semantic;
    QString source;
    QString scope;
    QString window;
    QDateTime resetAt;
    QSet<QString> qualityClasses;
    QMap<qint64, HistoryBucket> buckets;
    int sampleCount = 0;
    QDateTime firstObservation;
    QDateTime lastObservation;
};

struct DailyOverviewRow {
    QString day;
    double totalCost = 0.0;
    double totalTokens = 0.0;
    int providerCount = 0;
};

double percentChange(double previous, double current)
{
    if (qFuzzyIsNull(previous)) {
        return 0.0;
    }
    return ((current - previous) / std::abs(previous)) * 100.0;
}

QDateTime parseSnapshotTimestamp(const QString &raw)
{
    QDateTime dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(raw, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (!dt.isValid()) {
        return {};
    }

    if (dt.timeSpec() == Qt::UTC || dt.timeSpec() == Qt::OffsetFromUTC || dt.timeSpec() == Qt::TimeZone) {
        return dt.toUTC();
    }

    return QDateTime(dt.date(), dt.time(), QTimeZone::utc());
}

QString toDbDateTimeString(const QDateTime &dt)
{
    return dt.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

int effectiveBucketSeconds(const QDateTime &fromUtc, const QDateTime &toUtc,
                           int bucketMinutes)
{
    int baseBucketSecs = qBound(1, bucketMinutes, 24 * 60) * 60;
    qint64 rangeSecs = qMax<qint64>(1, fromUtc.secsTo(toUtc));
    int minBucketSecs = static_cast<int>(std::ceil(
        static_cast<double>(rangeSecs) / static_cast<double>(MAX_SERIES_POINTS)));
    return qMax(baseBucketSecs, minBucketSecs);
}

double deltaPercent(double first, double last)
{
    if (qFuzzyIsNull(first)) {
        return 0.0;
    }
    return ((last - first) / std::abs(first)) * 100.0;
}

QVariantList bucketToPoints(const QMap<qint64, BucketAggregate> &buckets)
{
    QVariantList points;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        const BucketAggregate &bucket = it.value();
        if (bucket.count <= 0 || !bucket.bucketStart.isValid()) {
            continue;
        }

        QVariantMap point;
        point[QStringLiteral("timestamp")] = bucket.bucketStart.toString(Qt::ISODate);
        point[QStringLiteral("value")] = bucket.sum / static_cast<double>(bucket.count);
        points.append(point);
    }
    return points;
}

QString normalizedQualityClass(const QString &source, const QString &quality,
                               const QString &semantic)
{
    const QString normalizedSource = source.trimmed().toLower();
    const QString normalizedQuality = quality.trimmed().toLower();
    if (semantic == QLatin1String("local_estimate") || normalizedQuality.contains(QStringLiteral("estimated")) || normalizedSource.contains(QStringLiteral("estimated")) || normalizedSource == QLatin1String("self_tracked") || normalizedSource == QLatin1String("browser_sync")) {
        return QStringLiteral("estimated");
    }
    if (normalizedSource.contains(QStringLiteral("connectivity")) || normalizedSource.contains(QStringLiteral("model_discovery"))) {
        return QStringLiteral("connectivity_only");
    }
    if (normalizedSource == QLatin1String("unknown") && (normalizedQuality.isEmpty() || normalizedQuality == QLatin1String("unknown"))) {
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("actual");
}

QString historyMetricForObservation(const QString &kind)
{
    if (kind == QLatin1String("cost")) {
        return QStringLiteral("cost");
    }
    if (kind == QLatin1String("input_tokens") || kind == QLatin1String("output_tokens")) {
        return QStringLiteral("tokens");
    }
    if (kind == QLatin1String("requests")) {
        return QStringLiteral("requests");
    }
    return {};
}

QString historySegmentKey(const HistorySample &sample)
{
    return QStringList{sample.unit, sample.currency, sample.semantic,
                       sample.source, sample.scope, sample.window}
        .join(QChar(0x1f));
}

QVariantMap buildHistorySeriesPayload(const QVariantMap &sourceDescriptor,
                                      const QString &metric,
                                      const QList<HistorySample> &samples,
                                      const QDateTime &fromUtc,
                                      const QDateTime &toUtc,
                                      int bucketMinutes)
{
    const int bucketSecs = effectiveBucketSeconds(fromUtc, toUtc, bucketMinutes);
    QMap<QString, HistorySegment> segments;

    for (const HistorySample &sample : samples) {
        if (!sample.timestamp.isValid() || sample.timestamp < fromUtc || sample.timestamp > toUtc) {
            continue;
        }
        const QString key = historySegmentKey(sample);
        HistorySegment &segment = segments[key];
        segment.unit = sample.unit;
        segment.currency = sample.currency;
        segment.semantic = sample.semantic;
        segment.source = sample.source;
        segment.scope = sample.scope;
        segment.window = sample.window;
        if (sample.resetAt.isValid()) {
            segment.resetAt = sample.resetAt;
        }
        segment.qualityClasses.insert(
            normalizedQualityClass(sample.source, sample.quality, sample.semantic));
        segment.sampleCount += qMax(1, sample.sampleCount);
        const QDateTime firstObservation = sample.firstObservation.isValid()
            ? sample.firstObservation
            : sample.timestamp;
        const QDateTime lastObservation = sample.lastObservation.isValid()
            ? sample.lastObservation
            : sample.timestamp;
        if (!segment.firstObservation.isValid()
            || firstObservation < segment.firstObservation) {
            segment.firstObservation = firstObservation;
        }
        if (!segment.lastObservation.isValid()
            || lastObservation > segment.lastObservation) {
            segment.lastObservation = lastObservation;
        }

        const qint64 bucketIndex = fromUtc.secsTo(sample.timestamp) / bucketSecs;
        HistoryBucket &bucket = segment.buckets[bucketIndex];
        if (!bucket.bucketStart.isValid()) {
            bucket.bucketStart = fromUtc.addSecs(bucketIndex * bucketSecs);
        }
        bucket.sum += sample.value;
        bucket.last = sample.value;
        bucket.count++;
    }

    QVariantList resultSeries;
    for (auto it = segments.constBegin(); it != segments.constEnd(); ++it) {
        const HistorySegment &segment = it.value();
        QVariantList points;
        bool containsGaps = false;
        qint64 previousIndex = -1;
        for (auto bucketIt = segment.buckets.constBegin();
             bucketIt != segment.buckets.constEnd(); ++bucketIt) {
            const HistoryBucket &bucket = bucketIt.value();
            if (bucket.count <= 0 || !bucket.bucketStart.isValid()) {
                continue;
            }
            if (previousIndex >= 0 && bucketIt.key() - previousIndex > 1) {
                QVariantMap gap;
                gap.insert(QStringLiteral("timestamp"),
                           fromUtc.addSecs((previousIndex + 1) * bucketSecs)
                               .toString(Qt::ISODate));
                gap.insert(QStringLiteral("value"), QVariant());
                gap.insert(QStringLiteral("available"), false);
                points.append(gap);
                containsGaps = true;
            }

            QVariantMap point;
            point.insert(QStringLiteral("timestamp"),
                         bucket.bucketStart.toString(Qt::ISODate));
            const bool intervalTotal = segment.semantic == QLatin1String("interval_total");
            point.insert(QStringLiteral("value"),
                         intervalTotal ? bucket.sum : bucket.last);
            point.insert(QStringLiteral("available"), true);
            points.append(point);
            previousIndex = bucketIt.key();
        }

        QVariantMap row;
        row.insert(QStringLiteral("sourceId"),
                   sourceDescriptor.value(QStringLiteral("historyId")));
        row.insert(QStringLiteral("dbName"),
                   sourceDescriptor.value(QStringLiteral("dbName")));
        row.insert(QStringLiteral("displayName"),
                   sourceDescriptor.value(
                       QStringLiteral("displayName"),
                       sourceDescriptor.value(QStringLiteral("dbName"))));
        row.insert(QStringLiteral("sourceKind"),
                   sourceDescriptor.value(QStringLiteral("sourceKind")));
        row.insert(QStringLiteral("metricKind"), metric);
        row.insert(QStringLiteral("unit"), segment.unit);
        row.insert(QStringLiteral("currency"), segment.currency);
        row.insert(QStringLiteral("semantic"), segment.semantic);
        row.insert(QStringLiteral("source"), segment.source);
        row.insert(QStringLiteral("scope"), segment.scope);
        row.insert(QStringLiteral("window"), segment.window);
        row.insert(QStringLiteral("resetAt"), segment.resetAt);
        QStringList qualityClasses = segment.qualityClasses.values();
        qualityClasses.sort();
        row.insert(QStringLiteral("sourceQualityClasses"), qualityClasses);
        row.insert(QStringLiteral("sampleCount"), segment.sampleCount);
        row.insert(QStringLiteral("availablePointCount"), segment.buckets.size());
        row.insert(QStringLiteral("firstObservation"), segment.firstObservation);
        row.insert(QStringLiteral("lastObservation"), segment.lastObservation);
        row.insert(QStringLiteral("bucketSizeSeconds"), bucketSecs);
        row.insert(QStringLiteral("containsGaps"), containsGaps);
        row.insert(QStringLiteral("stale"),
                   sourceDescriptor.value(QStringLiteral("stale"), false));
        row.insert(QStringLiteral("historyOnly"),
                   sourceDescriptor.value(QStringLiteral("historyOnly"), false));
        row.insert(QStringLiteral("points"), points);
        resultSeries.append(row);
    }

    QVariantMap payload;
    payload.insert(QStringLiteral("ok"), true);
    payload.insert(QStringLiteral("errorKey"), QString());
    payload.insert(QStringLiteral("metricKind"), metric);
    payload.insert(QStringLiteral("series"), resultSeries);
    return payload;
}

QString csvField(const QString &value)
{
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"')) && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'))) {
        return value;
    }
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
} // namespace

UsageDatabase::UsageDatabase(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("aiusagemonitor_history_%1")
                           .arg(s_instanceCounter.fetch_add(1)))
{
}

UsageDatabase::~UsageDatabase()
{
    const QString connectionName = m_connectionName;
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool UsageDatabase::isEnabled() const { return m_enabled; }
void UsageDatabase::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        Q_EMIT enabledChanged();
    }
}

int UsageDatabase::retentionDays() const { return m_retentionDays; }
void UsageDatabase::setRetentionDays(int days)
{
    // Clamp to valid range: 1–365
    days = qBound(1, days, 365);
    if (m_retentionDays != days) {
        m_retentionDays = days;
        Q_EMIT retentionDaysChanged();
    }
}

int UsageDatabase::pendingWorkerCount() const
{
    return m_pendingWorkerCount;
}

int UsageDatabase::databaseConnectionCount() const
{
    int count = 0;
    const QStringList names = QSqlDatabase::connectionNames();
    for (const QString &name : names) {
        if (name.startsWith(QLatin1String("aiusagemonitor_history_"))) {
            ++count;
        }
    }
    return count;
}

void UsageDatabase::beginWorker()
{
    ++m_pendingWorkerCount;
    Q_EMIT pendingWorkerCountChanged();
}

void UsageDatabase::finishWorker()
{
    m_pendingWorkerCount = qMax(0, m_pendingWorkerCount - 1);
    Q_EMIT pendingWorkerCountChanged();
}

void UsageDatabase::initDatabase()
{
    if (m_initialized)
        return;

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-ai-usage-monitor");
    QDir().mkpath(dataDir);

    QString dbPath = dataDir + QStringLiteral("/usage_history.db");

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "UsageDatabase: Failed to open database:"
                   << m_db.lastError().text();
        return;
    }

    // Enable WAL mode for better concurrent read performance
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    createTables();
    m_initialized = true;
}

void UsageDatabase::createTables()
{
    QSqlQuery query(m_db);

    // Usage snapshots -- one row per provider per refresh
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS usage_snapshots ("
                              "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "  timestamp DATETIME DEFAULT (datetime('now')),"
                              "  provider TEXT NOT NULL,"
                              "  model TEXT DEFAULT '',"
                              "  input_tokens INTEGER DEFAULT 0,"
                              "  output_tokens INTEGER DEFAULT 0,"
                              "  request_count INTEGER DEFAULT 0,"
                              "  cost REAL DEFAULT 0.0,"
                              "  is_estimated_cost INTEGER DEFAULT 0,"
                              "  daily_cost REAL DEFAULT 0.0,"
                              "  monthly_cost REAL DEFAULT 0.0,"
                              "  rl_requests INTEGER DEFAULT 0,"
                              "  rl_requests_remaining INTEGER DEFAULT 0,"
                              "  rl_tokens INTEGER DEFAULT 0,"
                              "  rl_tokens_remaining INTEGER DEFAULT 0,"
                              "  cost_source TEXT NOT NULL DEFAULT 'unknown',"
                              "  usage_source TEXT NOT NULL DEFAULT 'unknown',"
                              "  currency TEXT DEFAULT 'USD',"
                              "  data_quality TEXT DEFAULT 'unknown'"
                              ")"));

    // Indexes for efficient time-range queries
    query.exec(
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_snapshots_provider_time "
                       "ON usage_snapshots(provider, timestamp)"));

    // Rate limit events -- recorded when thresholds are hit
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS rate_limit_events ("
                              "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "  timestamp DATETIME DEFAULT (datetime('now')),"
                              "  provider TEXT NOT NULL,"
                              "  event_type TEXT NOT NULL,"
                              "  percent_used INTEGER DEFAULT 0"
                              ")"));

    query.exec(
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_ratelimit_provider_time "
                       "ON rate_limit_events(provider, timestamp)"));

    // Subscription tool usage snapshots
    query.exec(
        QStringLiteral("CREATE TABLE IF NOT EXISTS subscription_tool_usage ("
                       "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "  timestamp DATETIME DEFAULT (datetime('now')),"
                       "  tool_name TEXT NOT NULL,"
                       "  usage_count INTEGER DEFAULT 0,"
                       "  usage_limit INTEGER DEFAULT 0,"
                       "  period_type TEXT NOT NULL,"
                       "  plan_tier TEXT DEFAULT '',"
                       "  limit_reached BOOLEAN DEFAULT 0"
                       ")"));

    query.exec(
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tool_usage_name_time "
                       "ON subscription_tool_usage(tool_name, timestamp)"));

    // Migrate older databases created before analyst metadata existed.
    ensureColumnExists(QStringLiteral("usage_snapshots"), QStringLiteral("model"),
                       QStringLiteral("TEXT DEFAULT ''"));
    ensureColumnExists(QStringLiteral("usage_snapshots"),
                       QStringLiteral("is_estimated_cost"),
                       QStringLiteral("INTEGER DEFAULT 0"));
    ensureColumnExists(QStringLiteral("usage_snapshots"),
                       QStringLiteral("cost_source"),
                       QStringLiteral("TEXT NOT NULL DEFAULT 'unknown'"));
    ensureColumnExists(QStringLiteral("usage_snapshots"),
                       QStringLiteral("usage_source"),
                       QStringLiteral("TEXT NOT NULL DEFAULT 'unknown'"));
    ensureColumnExists(QStringLiteral("usage_snapshots"),
                       QStringLiteral("currency"),
                       QStringLiteral("TEXT DEFAULT 'USD'"));
    ensureColumnExists(QStringLiteral("usage_snapshots"),
                       QStringLiteral("data_quality"),
                       QStringLiteral("TEXT DEFAULT 'unknown'"));

    if (!migrateToObservationSchemaV3()) {
        qWarning() << "UsageDatabase: observation schema v3 migration failed; "
                      "legacy history remains intact";
        return;
    }
    if (!migrateToObservationSchemaV4()) {
        qWarning() << "UsageDatabase: observation schema v4 migration failed; v3 "
                      "history remains intact";
        return;
    }

    // Keep the original labels in migrated rows. This explicit table is the
    // stable compatibility contract used by exports and future migrations to
    // interpret v11/v12 source names without rewriting historical evidence.
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS metric_source_mapping ("
                              " legacy_source TEXT PRIMARY KEY, normalized_source "
                              "TEXT NOT NULL, meaning TEXT NOT NULL)"));
    const QStringList mappings = {
        QStringLiteral("actual_api|usage_api|Provider-reported usage"),
        QStringLiteral("billing_api|billing_api|Provider-reported billing"),
        QStringLiteral("usage_api|usage_api|Provider-reported usage"),
        QStringLiteral("estimated_from_usage|estimated_pricing|Local pricing estimate"),
        QStringLiteral("connectivity_probe|connectivity_probe|Manual connectivity probe"),
        QStringLiteral("connectivity_read_only|connectivity_probe|Read-only "
                       "connectivity check"),
        QStringLiteral("model_discovery_api|connectivity_probe|Read-only model discovery"),
        QStringLiteral("self_tracked|self_tracked|Local self-tracked value"),
        QStringLiteral("browser_sync|browser_sync|Local browser-derived value"),
        QStringLiteral("unknown|unknown|Unavailable or unknown source"),
    };
    QSqlQuery mappingQuery(m_db);
    mappingQuery.prepare(QStringLiteral("INSERT OR IGNORE INTO "
                                        "metric_source_mapping(legacy_source,normalized_source,meaning) "
                                        "VALUES(?,?,?)"));
    for (const QString &mapping : mappings) {
        const QStringList fields = mapping.split(QLatin1Char('|'));
        mappingQuery.bindValue(0, fields.value(0));
        mappingQuery.bindValue(1, fields.value(1));
        mappingQuery.bindValue(2, fields.value(2));
        if (!mappingQuery.exec()) {
            qWarning() << "UsageDatabase: source mapping insert failed" << mappingQuery.lastError().text();
        }
    }
    if (!query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_observations_provider_epoch_kind "
                                   "ON observations(provider, unixepoch(observed_at_utc), "
                                   "metric_kind)"))) {
        qWarning() << "UsageDatabase: history performance index creation failed" << query.lastError().text();
    }
    if (!migrateToSchemaV5()) {
        qWarning() << "UsageDatabase: schema v5 migration failed; v4 history "
                      "remains intact";
    }
}

bool UsageDatabase::migrateToObservationSchemaV3()
{
    QSqlQuery versionQuery(m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next()) {
        return false;
    }
    const int currentVersion = versionQuery.value(0).toInt();
    if (currentVersion >= 3) {
        return true;
    }

    const QString databasePath = m_db.databaseName();
    const QString backupPath = databasePath + QStringLiteral(".v11-backup");
    if (QFileInfo::exists(databasePath) && !QFileInfo::exists(backupPath)) {
        if (!QFile::copy(databasePath, backupPath)) {
            qWarning() << "UsageDatabase: unable to create pre-v12 backup"
                       << backupPath;
            return false;
        }
    }

    if (!m_db.transaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    const QString createSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS observations ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " provider TEXT NOT NULL,"
        " observed_at_utc DATETIME NOT NULL DEFAULT (datetime('now')),"
        " interval_start_utc DATETIME,"
        " interval_end_utc DATETIME,"
        " metric_kind TEXT NOT NULL,"
        " unit TEXT NOT NULL,"
        " value REAL NOT NULL,"
        " currency TEXT,"
        " semantic TEXT NOT NULL CHECK(semantic IN "
        "('gauge','cumulative_counter','interval_total','local_estimate')),"
        " source TEXT NOT NULL,"
        " data_quality TEXT NOT NULL DEFAULT 'unknown',"
        " model_scope TEXT DEFAULT '',"
        " project_scope TEXT DEFAULT '',"
        " correlation_id TEXT NOT NULL"
        ")");
    if (!query.exec(createSql) || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS "
                                                             "idx_observations_provider_time_source_currency "
                                                             "ON observations(provider, observed_at_utc, source, currency)"))) {
        m_db.rollback();
        return false;
    }

    QSqlQuery countQuery(m_db);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM observations")) || !countQuery.next()) {
        m_db.rollback();
        return false;
    }
    if (countQuery.value(0).toLongLong() == 0) {
        const QString correlation = QStringLiteral("'legacy-' || id");
        const QString migrationSql = QStringLiteral(
                                         "INSERT INTO observations(provider, observed_at_utc, metric_kind, "
                                         "unit, value, currency, semantic, source, data_quality, "
                                         "model_scope, correlation_id) "
                                         "SELECT provider, timestamp, 'cost', "
                                         "COALESCE(NULLIF(currency,''),'USD'), cost, "
                                         "COALESCE(NULLIF(currency,''),'USD'), "
                                         "CASE WHEN is_estimated_cost != 0 THEN 'local_estimate' ELSE "
                                         "'gauge' END, cost_source, data_quality, model, %1 FROM "
                                         "usage_snapshots "
                                         "UNION ALL SELECT provider, timestamp, 'input_tokens', 'token', "
                                         "input_tokens, NULL, 'cumulative_counter', usage_source, "
                                         "data_quality, model, %1 FROM usage_snapshots "
                                         "UNION ALL SELECT provider, timestamp, 'output_tokens', 'token', "
                                         "output_tokens, NULL, 'cumulative_counter', usage_source, "
                                         "data_quality, model, %1 FROM usage_snapshots "
                                         "UNION ALL SELECT provider, timestamp, 'requests', 'request', "
                                         "request_count, NULL, 'cumulative_counter', usage_source, "
                                         "data_quality, model, %1 FROM usage_snapshots")
                                         .arg(correlation);
        if (!query.exec(migrationSql)) {
            qWarning() << "UsageDatabase: v3 migration insert failed"
                       << query.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version = 3"))) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool UsageDatabase::migrateToObservationSchemaV4()
{
    QSqlQuery versionQuery(m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next()) {
        return false;
    }
    if (versionQuery.value(0).toInt() >= 4) {
        return true;
    }
    versionQuery.finish();

    const QString databasePath = m_db.databaseName();
    const QString backupPath = databasePath + QStringLiteral(".v13-backup");
    if (QFileInfo::exists(databasePath) && !QFileInfo::exists(backupPath) && !QFile::copy(databasePath, backupPath)) {
        qWarning() << "UsageDatabase: unable to create pre-v13 backup"
                   << backupPath;
        return false;
    }
    if (!m_db.transaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    const QString createSql = QStringLiteral(
        "CREATE TABLE observations_v4 ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " provider TEXT NOT NULL,"
        " observed_at_utc DATETIME NOT NULL DEFAULT (datetime('now')),"
        " interval_start_utc DATETIME, interval_end_utc DATETIME,"
        " metric_kind TEXT NOT NULL, unit TEXT NOT NULL, value REAL NULL, "
        "currency TEXT,"
        " semantic TEXT NOT NULL CHECK(semantic IN "
        "('gauge','cumulative_counter','interval_total','local_estimate')),"
        " source TEXT NOT NULL, data_quality TEXT NOT NULL DEFAULT 'unknown',"
        " scope TEXT NOT NULL DEFAULT 'api_key', window TEXT NOT NULL DEFAULT "
        "'current',"
        " model_scope TEXT DEFAULT '', project_scope TEXT DEFAULT '', "
        "reset_at_utc DATETIME,"
        " correlation_id TEXT NOT NULL)");
    if (!query.exec(createSql) || !query.exec(QStringLiteral("INSERT INTO "
                                                             "observations_v4(id,provider,observed_at_utc,interval_start_utc,"
                                                             "interval_end_utc,metric_kind,unit,value,currency,semantic,source,"
                                                             "data_quality,model_scope,project_scope,correlation_id) "
                                                             "SELECT "
                                                             "id,provider,observed_at_utc,interval_start_utc,interval_end_utc,"
                                                             "metric_kind,unit,value,currency,semantic,source,data_quality,model_"
                                                             "scope,project_scope,correlation_id FROM observations"))
        || !query.exec(QStringLiteral("DROP TABLE observations")) || !query.exec(QStringLiteral("ALTER TABLE observations_v4 RENAME TO observations")) || !query.exec(QStringLiteral("CREATE INDEX idx_observations_provider_time_source_currency "
                                                                                                                                                                                     "ON observations(provider, observed_at_utc, source, currency)"))
        || !query.exec(QStringLiteral("PRAGMA user_version = 4"))) {
        qWarning() << "UsageDatabase: v4 migration failed"
                   << query.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool UsageDatabase::migrateToSchemaV5()
{
    QSqlQuery versionQuery(m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next()) {
        return false;
    }
    if (versionQuery.value(0).toInt() >= 5) {
        return true;
    }
    versionQuery.finish();

    const QString databasePath = m_db.databaseName();
    const QString backupPath = databasePath + QStringLiteral(".v17-backup");
    if (QFileInfo::exists(databasePath) && !QFileInfo::exists(backupPath) && !QFile::copy(databasePath, backupPath)) {
        qWarning() << "UsageDatabase: unable to create pre-v17 backup" << backupPath;
        return false;
    }
    if (!m_db.transaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    const QString createSql = QStringLiteral("CREATE TABLE IF NOT EXISTS guardrail_events ("
                                             " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                             " stable_id TEXT NOT NULL,"
                                             " source_id TEXT NOT NULL,"
                                             " source_kind TEXT NOT NULL,"
                                             " risk_kind TEXT NOT NULL CHECK(risk_kind IN "
                                             "('quota_exhaustion','budget_overrun')),"
                                             " window_key TEXT NOT NULL,"
                                             " scope TEXT NOT NULL,"
                                             " transition TEXT NOT NULL CHECK(transition IN "
                                             "('warning','critical','recovered')),"
                                             " observed_at_utc DATETIME NOT NULL,"
                                             " predicted_at_utc DATETIME,"
                                             " period_end_utc DATETIME NOT NULL,"
                                             " method_id TEXT NOT NULL,"
                                             " evidence_grade TEXT NOT NULL CHECK(evidence_grade IN "
                                             "('unavailable','usable','strong')),"
                                             " current_value REAL,"
                                             " projected_value REAL,"
                                             " limit_value REAL,"
                                             " unit TEXT NOT NULL,"
                                             " currency TEXT,"
                                             " value_class TEXT NOT NULL CHECK(value_class IN "
                                             "('actual','estimated')),"
                                             " reason_key TEXT NOT NULL DEFAULT ''"
                                             ")");
    if (!query.exec(createSql)
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS "
                                      "idx_guardrail_events_stable_observed "
                                      "ON guardrail_events(stable_id, observed_at_utc DESC, id DESC)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_guardrail_events_retention "
                                      "ON guardrail_events(observed_at_utc)"))
        || !query.exec(QStringLiteral("PRAGMA user_version = 5"))) {
        qWarning() << "UsageDatabase: v5 migration failed" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

void UsageDatabase::ensureColumnExists(const QString &table,
                                       const QString &column,
                                       const QString &definition)
{
    QSqlQuery pragma(m_db);
    if (!pragma.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        qWarning() << "UsageDatabase: Failed to inspect table schema for" << table
                   << ":" << pragma.lastError().text();
        return;
    }

    while (pragma.next()) {
        if (pragma.value(1).toString() == column) {
            return;
        }
    }

    QSqlQuery alter(m_db);
    const QString sql = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                            .arg(table, column, definition);
    if (!alter.exec(sql)) {
        qWarning() << "UsageDatabase: Failed to add column" << column << "to"
                   << table << ":" << alter.lastError().text();
    }
}

void UsageDatabase::recordSnapshot(
    const QString &provider, qint64 inputTokens, qint64 outputTokens,
    int requestCount, double cost, double dailyCost, double monthlyCost,
    int rateLimitRequests, int rateLimitRequestsRemaining, int rateLimitTokens,
    int rateLimitTokensRemaining, const QString &model, bool isEstimatedCost,
    const QString &costSource, const QString &usageSource,
    const QString &currency, const QString &dataQuality)
{
    if (!m_enabled)
        return;

    // Throttle writes: skip if the same provider wrote recently AND data hasn't
    // changed
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 lastWrite = m_lastWriteTime.value(provider, 0);
    const QByteArray normalizedState = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16")
                                           .arg(inputTokens)
                                           .arg(outputTokens)
                                           .arg(requestCount)
                                           .arg(cost, 0, 'g', 17)
                                           .arg(dailyCost, 0, 'g', 17)
                                           .arg(monthlyCost, 0, 'g', 17)
                                           .arg(rateLimitRequests)
                                           .arg(rateLimitRequestsRemaining)
                                           .arg(rateLimitTokens)
                                           .arg(rateLimitTokensRemaining)
                                           .arg(model, costSource, usageSource, currency, dataQuality)
                                           .arg(isEstimatedCost ? 1 : 0)
                                           .toUtf8();
    const bool dataChanged = m_lastWrittenState.value(provider) != normalizedState;
    bool throttled = (now - lastWrite) < WRITE_THROTTLE_SECS;

    if (throttled && !dataChanged)
        return;

    initDatabase();
    if (!m_initialized)
        return;

    if (!m_db.transaction()) {
        qWarning() << "UsageDatabase: Failed to begin snapshot transaction";
        return;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO usage_snapshots "
        "(provider, model, input_tokens, output_tokens, request_count, cost, "
        "is_estimated_cost, "
        "daily_cost, monthly_cost, rl_requests, rl_requests_remaining, "
        "rl_tokens, rl_tokens_remaining, "
        "cost_source, usage_source, currency, data_quality) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(provider);
    query.addBindValue(model.trimmed());
    query.addBindValue(inputTokens);
    query.addBindValue(outputTokens);
    query.addBindValue(requestCount);
    query.addBindValue(cost);
    query.addBindValue(isEstimatedCost ? 1 : 0);
    query.addBindValue(dailyCost);
    query.addBindValue(monthlyCost);
    query.addBindValue(rateLimitRequests);
    query.addBindValue(rateLimitRequestsRemaining);
    query.addBindValue(rateLimitTokens);
    query.addBindValue(rateLimitTokensRemaining);
    query.addBindValue(costSource.trimmed().isEmpty() ? QStringLiteral("unknown")
                                                      : costSource.trimmed());
    query.addBindValue(usageSource.trimmed().isEmpty() ? QStringLiteral("unknown")
                                                       : usageSource.trimmed());
    query.addBindValue(currency.trimmed().isEmpty()
                           ? QStringLiteral("USD")
                           : currency.trimmed().toUpper());
    query.addBindValue(dataQuality.trimmed().isEmpty() ? QStringLiteral("unknown")
                                                       : dataQuality.trimmed());

    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to record snapshot:"
                   << query.lastError().text();
        m_db.rollback();
    } else if (!recordObservations(provider, model, inputTokens, outputTokens,
                                   requestCount, cost, currency, costSource,
                                   usageSource, dataQuality)) {
        qWarning() << "UsageDatabase: Failed to record normalized observations";
        m_db.rollback();
    } else if (!m_db.commit()) {
        qWarning() << "UsageDatabase: Failed to commit snapshot transaction";
    } else {
        m_lastWriteTime[provider] = now;
        m_lastWrittenState[provider] = normalizedState;
    }
}

bool UsageDatabase::recordObservations(const QString &provider,
                                       const QString &model, qint64 inputTokens,
                                       qint64 outputTokens, int requestCount,
                                       double cost, const QString &currency,
                                       const QString &costSource,
                                       const QString &usageSource,
                                       const QString &dataQuality)
{
    const QString correlationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString currencyCode = currency.trimmed().isEmpty()
        ? QStringLiteral("USD")
        : currency.trimmed().toUpper();
    const QString quality = dataQuality.trimmed().isEmpty()
        ? QStringLiteral("unknown")
        : dataQuality.trimmed();

    struct Observation {
        QString kind;
        QString unit;
        double value;
        QString currency;
        QString semantic;
        QString source;
    };
    const QList<Observation> observations = {
        {QStringLiteral("cost"), currencyCode, cost, currencyCode,
         costSource == QLatin1String("estimated_from_usage")
             ? QStringLiteral("local_estimate")
             : QStringLiteral("gauge"),
         costSource.trimmed().isEmpty() ? QStringLiteral("unknown")
                                        : costSource.trimmed()},
        {QStringLiteral("input_tokens"),
         QStringLiteral("token"),
         static_cast<double>(inputTokens),
         {},
         QStringLiteral("cumulative_counter"),
         usageSource},
        {QStringLiteral("output_tokens"),
         QStringLiteral("token"),
         static_cast<double>(outputTokens),
         {},
         QStringLiteral("cumulative_counter"),
         usageSource},
        {QStringLiteral("requests"),
         QStringLiteral("request"),
         static_cast<double>(requestCount),
         {},
         QStringLiteral("cumulative_counter"),
         usageSource},
    };

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO observations(provider, metric_kind, unit, value, currency, "
        "semantic, source, data_quality, model_scope, correlation_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const Observation &observation : observations) {
        query.bindValue(0, provider);
        query.bindValue(1, observation.kind);
        query.bindValue(2, observation.unit);
        query.bindValue(3, observation.value);
        query.bindValue(4, observation.currency.isEmpty() ? QVariant() : QVariant(observation.currency));
        query.bindValue(5, observation.semantic);
        query.bindValue(6, observation.source.trimmed().isEmpty() ? QStringLiteral("unknown") : observation.source.trimmed());
        query.bindValue(7, quality);
        query.bindValue(8, model.trimmed());
        query.bindValue(9, correlationId);
        if (!query.exec()) {
            qWarning() << "UsageDatabase: observation insert failed"
                       << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool UsageDatabase::recordProviderMetrics(const QString &provider,
                                          const QVariantList &metrics)
{
    if (!m_enabled)
        return false;
    initDatabase();
    if (!m_initialized || !m_db.transaction())
        return false;

    const QString correlationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO "
        "observations(provider,observed_at_utc,interval_start_utc,interval_end_"
        "utc,metric_kind,unit,value,currency,semantic,source,data_quality,scope,"
        "window,model_scope,project_scope,reset_at_utc,correlation_id) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    for (const QVariant &entry : metrics) {
        const QVariantMap metric = entry.toMap();
        const QString requestedSemantic =
            metric.value(QStringLiteral("semantic")).toString();
        const QSet<QString> validSemantics{
            QStringLiteral("gauge"), QStringLiteral("cumulative_counter"),
            QStringLiteral("interval_total"), QStringLiteral("local_estimate")};
        QString semantic = requestedSemantic;
        if (!validSemantics.contains(semantic)) {
            const QString quality =
                metric.value(QStringLiteral("quality")).toString().toLower();
            const QString source =
                metric.value(QStringLiteral("source")).toString().toLower();
            if (quality.contains(QStringLiteral("estimated"))
                || source.contains(QStringLiteral("estimated"))) {
                semantic = QStringLiteral("local_estimate");
            } else {
                semantic = QStringLiteral("gauge");
            }
        }
        query.bindValue(0, provider);
        const QDateTime observedAt = metric
                                         .value(QStringLiteral("observedAt"),
                                                QDateTime::currentDateTimeUtc())
                                         .toDateTime();
        query.bindValue(1,
                        toDbDateTimeString(observedAt.isValid()
                                               ? observedAt
                                               : QDateTime::currentDateTimeUtc()));
        query.bindValue(2, metric.value(QStringLiteral("periodStart")));
        query.bindValue(3, metric.value(QStringLiteral("periodEnd")));
        query.bindValue(
            4, metric.value(QStringLiteral("kind"), QStringLiteral("unknown")));
        query.bindValue(
            5, metric.value(QStringLiteral("unit"), QStringLiteral("unknown")));
        query.bindValue(6, metric.value(QStringLiteral("available")).toBool() ? metric.value(QStringLiteral("value")) : QVariant());
        query.bindValue(7, metric.value(QStringLiteral("currency")));
        query.bindValue(8, semantic);
        query.bindValue(
            9, metric.value(QStringLiteral("source"), QStringLiteral("unknown")));
        query.bindValue(
            10, metric.value(QStringLiteral("quality"), QStringLiteral("unknown")));
        query.bindValue(
            11, metric.value(QStringLiteral("scope"), QStringLiteral("api_key")));
        query.bindValue(
            12, metric.value(QStringLiteral("window"), QStringLiteral("current")));
        query.bindValue(13, metric.value(QStringLiteral("modelScope")));
        query.bindValue(14, metric.value(QStringLiteral("projectScope")));
        query.bindValue(15, metric.value(QStringLiteral("resetAt")));
        query.bindValue(16, correlationId);
        if (!query.exec()) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

bool UsageDatabase::recordGuardrailTransition(const QVariantMap &forecast, const QString &transition)
{
    if (!m_enabled) {
        return false;
    }
    initDatabase();
    if (!m_initialized) {
        return false;
    }

    QString diagnostic;
    const auto result = ForecastContract::fromVariantMap(forecast, &diagnostic);
    const QSet<QString> allowedTransitions {
        QStringLiteral("warning"),
        QStringLiteral("critical"),
        QStringLiteral("recovered"),
    };
    if (!result || !allowedTransitions.contains(transition)) {
        qWarning() << "UsageDatabase: rejected guardrail transition" << diagnostic;
        return false;
    }
    if ((transition == QLatin1String("warning") && result->state != ForecastContract::State::Warning)
        || (transition == QLatin1String("critical") && result->state != ForecastContract::State::Critical)
        || (transition == QLatin1String("recovered") && result->state != ForecastContract::State::Safe
            && result->state != ForecastContract::State::Unavailable)) {
        qWarning() << "UsageDatabase: transition does not match forecast state";
        return false;
    }

    const QString stableId = result->stableId();
    if (!m_db.transaction()) {
        return false;
    }
    QSqlQuery latest(m_db);
    latest.prepare(QStringLiteral("SELECT transition FROM guardrail_events WHERE stable_id = ? "
                                  "ORDER BY id DESC LIMIT 1"));
    latest.addBindValue(stableId);
    if (!latest.exec()) {
        m_db.rollback();
        return false;
    }
    if (latest.next() && latest.value(0).toString() == transition) {
        m_db.rollback();
        return false;
    }
    latest.finish();

    const auto optionalNumber
        = [](const std::optional<double> &value) { return value ? QVariant(*value) : QVariant(); };
    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO guardrail_events("
                                  "stable_id,source_id,source_kind,risk_kind,window_key,scope,transition,"
                                  "observed_at_utc,predicted_at_utc,period_end_utc,method_id,"
                                  "evidence_grade,current_value,projected_value,limit_value,unit,currency,"
                                  "value_class,reason_key) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    insert.addBindValue(stableId);
    insert.addBindValue(result->sourceId);
    insert.addBindValue(result->sourceKind);
    insert.addBindValue(ForecastContract::kindKey(result->kind));
    insert.addBindValue(result->window);
    insert.addBindValue(result->scope);
    insert.addBindValue(transition);
    insert.addBindValue(toDbDateTimeString(result->generatedAt));
    insert.addBindValue(result->predictedAt ? QVariant(toDbDateTimeString(*result->predictedAt)) : QVariant());
    insert.addBindValue(toDbDateTimeString(result->periodEnd));
    insert.addBindValue(result->methodId);
    insert.addBindValue(ForecastContract::evidenceGradeKey(result->evidenceGrade));
    insert.addBindValue(optionalNumber(result->currentValue));
    insert.addBindValue(optionalNumber(result->projectedValue));
    insert.addBindValue(optionalNumber(result->limitValue));
    insert.addBindValue(result->unit);
    insert.addBindValue(result->currency ? QVariant(*result->currency) : QVariant());
    insert.addBindValue(ForecastContract::valueClassKey(result->valueClass));
    insert.addBindValue(result->reasonKey.isNull() ? QStringLiteral("") : result->reasonKey);
    if (!insert.exec()) {
        qWarning() << "UsageDatabase: failed to record guardrail transition" << insert.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

QVariantMap UsageDatabase::lastGuardrailTransition(const QString &stableId) const
{
    if (!m_initialized || stableId.trimmed().isEmpty()) {
        return { };
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT stable_id,source_id,source_kind,risk_kind,window_key,scope,"
                                 "transition,observed_at_utc,predicted_at_utc,period_end_utc,method_id,"
                                 "evidence_grade,current_value,projected_value,limit_value,unit,currency,"
                                 "value_class,reason_key FROM guardrail_events WHERE stable_id = ? "
                                 "ORDER BY id DESC LIMIT 1"));
    query.addBindValue(stableId);
    if (!query.exec() || !query.next()) {
        return { };
    }

    const QStringList keys {
        QStringLiteral("stableId"),
        QStringLiteral("sourceId"),
        QStringLiteral("sourceKind"),
        QStringLiteral("kind"),
        QStringLiteral("window"),
        QStringLiteral("scope"),
        QStringLiteral("transition"),
        QStringLiteral("observedAt"),
        QStringLiteral("predictedAt"),
        QStringLiteral("periodEnd"),
        QStringLiteral("methodId"),
        QStringLiteral("evidenceGrade"),
        QStringLiteral("currentValue"),
        QStringLiteral("projectedValue"),
        QStringLiteral("limitValue"),
        QStringLiteral("unit"),
        QStringLiteral("currency"),
        QStringLiteral("valueClass"),
        QStringLiteral("reasonKey"),
    };
    QVariantMap event;
    for (int i = 0; i < keys.size(); ++i) {
        event.insert(keys.at(i), query.isNull(i) ? QVariant() : query.value(i));
    }
    return event;
}

void UsageDatabase::recordRateLimitEvent(const QString &provider,
                                         const QString &eventType,
                                         int percentUsed)
{
    if (!m_enabled)
        return;

    initDatabase();
    if (!m_initialized)
        return;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO rate_limit_events (provider, "
                                 "event_type, percent_used) VALUES (?, ?, ?)"));
    query.addBindValue(provider);
    query.addBindValue(eventType);
    query.addBindValue(percentUsed);

    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to record rate limit event:"
                   << query.lastError().text();
    }
}

void UsageDatabase::recordToolSnapshot(const QString &toolName, int usageCount,
                                       int usageLimit,
                                       const QString &periodType,
                                       const QString &planTier,
                                       bool limitReached)
{
    if (!m_enabled)
        return;

    // Throttle writes: skip if the same tool wrote recently AND data hasn't
    // changed
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QString throttleKey = QStringLiteral("tool:") + toolName;
    qint64 lastWrite = m_lastWriteTime.value(throttleKey, 0);
    const QByteArray normalizedState = QStringLiteral("%1|%2|%3|%4|%5")
                                           .arg(usageCount)
                                           .arg(usageLimit)
                                           .arg(periodType, planTier)
                                           .arg(limitReached ? 1 : 0)
                                           .toUtf8();
    const bool dataChanged = m_lastWrittenState.value(throttleKey) != normalizedState;
    bool throttled = (now - lastWrite) < WRITE_THROTTLE_SECS;

    if (throttled && !dataChanged)
        return;

    initDatabase();
    if (!m_initialized)
        return;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO subscription_tool_usage "
                                 "(tool_name, usage_count, usage_limit, "
                                 "period_type, plan_tier, limit_reached) "
                                 "VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(toolName);
    query.addBindValue(usageCount);
    query.addBindValue(usageLimit);
    query.addBindValue(periodType);
    query.addBindValue(planTier);
    query.addBindValue(limitReached ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to record tool snapshot:"
                   << query.lastError().text();
    } else {
        m_lastWriteTime[throttleKey] = now;
        m_lastWrittenState[throttleKey] = normalizedState;
    }
}

QVariantList UsageDatabase::getSnapshots(const QString &provider,
                                         const QDateTime &from,
                                         const QDateTime &to) const
{
    QVariantList results;

    if (!m_initialized)
        return results;

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral("SELECT timestamp, model, input_tokens, output_tokens, "
                       "request_count, cost, "
                       "is_estimated_cost, daily_cost, monthly_cost, "
                       "rl_requests, rl_requests_remaining, "
                       "rl_tokens, rl_tokens_remaining, cost_source, "
                       "usage_source, currency, data_quality "
                       "FROM usage_snapshots "
                       "WHERE provider = ? AND timestamp >= ? AND timestamp <= ? "
                       "ORDER BY timestamp ASC LIMIT 10000"));
    query.addBindValue(provider);
    query.addBindValue(toDbDateTimeString(from));
    query.addBindValue(toDbDateTimeString(to));

    if (!query.exec()) {
        qWarning() << "UsageDatabase: getSnapshots query failed:"
                   << query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap row;
        row[QStringLiteral("timestamp")] = query.value(0).toString();
        row[QStringLiteral("model")] = query.value(1).toString();
        row[QStringLiteral("inputTokens")] = query.value(2).toLongLong();
        row[QStringLiteral("outputTokens")] = query.value(3).toLongLong();
        row[QStringLiteral("requestCount")] = query.value(4).toInt();
        row[QStringLiteral("cost")] = query.value(5).toDouble();
        row[QStringLiteral("isEstimatedCost")] = query.value(6).toBool();
        row[QStringLiteral("dailyCost")] = query.value(7).toDouble();
        row[QStringLiteral("monthlyCost")] = query.value(8).toDouble();
        row[QStringLiteral("rlRequests")] = query.value(9).toInt();
        row[QStringLiteral("rlRequestsRemaining")] = query.value(10).toInt();
        row[QStringLiteral("rlTokens")] = query.value(11).toInt();
        row[QStringLiteral("rlTokensRemaining")] = query.value(12).toInt();
        row[QStringLiteral("costSource")] = query.value(13).toString();
        row[QStringLiteral("usageSource")] = query.value(14).toString();
        row[QStringLiteral("currency")] = query.value(15).toString();
        row[QStringLiteral("dataQuality")] = query.value(16).toString();
        results.append(row);
    }

    return results;
}

QVariantList UsageDatabase::getDailyCosts(const QString &provider,
                                          const QDateTime &from,
                                          const QDateTime &to) const
{
    QVariantList results;

    if (!m_initialized)
        return results;

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral("SELECT date(observed_at_utc) AS day, currency, "
                       "SUM(value) AS interval_total "
                       "FROM observations "
                       "WHERE provider = ? AND metric_kind = 'cost' AND semantic "
                       "= 'interval_total' "
                       "AND observed_at_utc >= ? AND observed_at_utc <= ? "
                       "GROUP BY day, currency ORDER BY day ASC, currency ASC"));
    query.addBindValue(provider);
    query.addBindValue(toDbDateTimeString(from));
    query.addBindValue(toDbDateTimeString(to));

    if (!query.exec()) {
        qWarning() << "UsageDatabase: getDailyCosts query failed:"
                   << query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap row;
        row[QStringLiteral("date")] = query.value(0).toString();
        row[QStringLiteral("currency")] = query.value(1).toString();
        row[QStringLiteral("totalCost")] = query.value(2).toDouble();
        row[QStringLiteral("maxDailyCost")] = query.value(2).toDouble();
        results.append(row);
    }

    return results;
}

QVariantMap UsageDatabase::getSummary(const QString &provider,
                                      const QDateTime &from,
                                      const QDateTime &to) const
{
    QVariantMap result;

    if (!m_initialized)
        return result;

    QVariantMap currencyTotals;
    QSqlQuery moneyQuery(m_db);
    moneyQuery.prepare(
        QStringLiteral("SELECT currency, SUM(value) FROM observations o "
                       "WHERE provider = ? AND metric_kind = 'cost' "
                       "AND observed_at_utc >= ? AND observed_at_utc <= ? "
                       "AND id IN (SELECT MAX(id) FROM observations "
                       "WHERE provider = ? AND metric_kind = 'cost' "
                       "AND observed_at_utc >= ? AND observed_at_utc <= ? "
                       "GROUP BY provider, currency) "
                       "GROUP BY currency ORDER BY currency"));
    const QString fromText = toDbDateTimeString(from);
    const QString toText = toDbDateTimeString(to);
    moneyQuery.addBindValue(provider);
    moneyQuery.addBindValue(fromText);
    moneyQuery.addBindValue(toText);
    moneyQuery.addBindValue(provider);
    moneyQuery.addBindValue(fromText);
    moneyQuery.addBindValue(toText);
    if (!moneyQuery.exec()) {
        qWarning() << "UsageDatabase: monetary summary query failed:"
                   << moneyQuery.lastError().text();
        return result;
    }
    while (moneyQuery.next()) {
        currencyTotals.insert(moneyQuery.value(0).toString(), moneyQuery.value(1));
    }
    result[QStringLiteral("currencyTotals")] = currencyTotals;
    result[QStringLiteral("mixedCurrencies")] = currencyTotals.size() > 1;
    result[QStringLiteral("totalCost")] = currencyTotals.size() == 1
        ? currencyTotals.constBegin().value().toDouble()
        : 0.0;

    QSqlQuery usageQuery(m_db);
    usageQuery.prepare(QStringLiteral(
        "SELECT MAX(request_count), MAX(input_tokens + output_tokens), COUNT(*) "
        "FROM usage_snapshots WHERE provider = ? AND timestamp >= ? AND "
        "timestamp <= ?"));
    usageQuery.addBindValue(provider);
    usageQuery.addBindValue(fromText);
    usageQuery.addBindValue(toText);
    if (usageQuery.exec() && usageQuery.next()) {
        result[QStringLiteral("totalRequests")] = usageQuery.value(0).toInt();
        result[QStringLiteral("peakTokenUsage")] = usageQuery.value(1).toLongLong();
        result[QStringLiteral("snapshotCount")] = usageQuery.value(2).toInt();
    }

    const QVariantList dailyRows = getDailyCosts(provider, from, to);
    double dailySum = 0.0;
    double dailyMax = 0.0;
    for (const QVariant &rowValue : dailyRows) {
        const double value = rowValue.toMap().value(QStringLiteral("totalCost")).toDouble();
        dailySum += value;
        dailyMax = qMax(dailyMax, value);
    }
    result[QStringLiteral("avgDailyCost")] = dailyRows.isEmpty() ? 0.0 : dailySum / dailyRows.size();
    result[QStringLiteral("maxDailyCost")] = dailyMax;

    return result;
}

void UsageDatabase::requestHistory(const QString &requestId,
                                   const QString &provider,
                                   const QDateTime &from, const QDateTime &to)
{
    initDatabase();
    beginWorker();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId]() {
                Q_EMIT historyReady(requestId, watcher->result());
                finishWorker();
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([provider, from, to]() {
        UsageDatabase workerDatabase;
        workerDatabase.init();
        QVariantMap payload;
        payload.insert(QStringLiteral("snapshots"),
                       workerDatabase.getSnapshots(provider, from, to));
        payload.insert(QStringLiteral("dailyCosts"),
                       workerDatabase.getDailyCosts(provider, from, to));
        payload.insert(QStringLiteral("summary"),
                       workerDatabase.getSummary(provider, from, to));
        return payload;
    }));
}

void UsageDatabase::requestComparison(
    const QString &requestId, const QStringList &names, const QDateTime &from,
    const QDateTime &to, const QString &source, const QString &metric,
    int bucketMinutes)
{
    initDatabase();
    beginWorker();
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this,
            [this, watcher, requestId]() {
                Q_EMIT comparisonReady(requestId, watcher->result());
                finishWorker();
                watcher->deleteLater();
            });
    watcher->setFuture(
        QtConcurrent::run([names, from, to, source, metric, bucketMinutes]() {
            UsageDatabase workerDatabase;
            workerDatabase.init();
            return source == QLatin1String("tools")
                ? workerDatabase.getToolSeries(names, from, to, metric,
                                               bucketMinutes)
                : workerDatabase.getProviderSeries(names, from, to, metric,
                                                   bucketMinutes);
        }));
}

QVariantList UsageDatabase::getHistoryCatalog() const
{
    QVariantList catalog;
    if (!m_initialized) {
        return catalog;
    }

    QStringList providers = getProviders();
    QSqlQuery observedProviders(m_db);
    if (observedProviders.exec(QStringLiteral(
            "SELECT DISTINCT provider FROM observations ORDER BY provider"))) {
        while (observedProviders.next()) {
            const QString provider = observedProviders.value(0).toString();
            if (!providers.contains(provider)) {
                providers.append(provider);
            }
        }
    }
    providers.sort(Qt::CaseInsensitive);

    for (const QString &provider : std::as_const(providers)) {
        QSet<QString> observationKinds;
        QDateTime firstObservation;
        QDateTime lastObservation;
        int sampleCount = 0;

        QSqlQuery metricsQuery(m_db);
        metricsQuery.prepare(
            QStringLiteral("SELECT metric_kind, observed_at_utc FROM observations "
                           "WHERE provider = ? AND value IS NOT NULL "
                           "AND lower(source) != 'unknown' "
                           "AND lower(source) NOT LIKE '%connectivity%' "
                           "AND lower(source) NOT LIKE '%model_discovery%' "
                           "ORDER BY observed_at_utc"));
        metricsQuery.addBindValue(provider);
        if (metricsQuery.exec()) {
            while (metricsQuery.next()) {
                const QString kind = metricsQuery.value(0).toString();
                const QDateTime timestamp = parseSnapshotTimestamp(metricsQuery.value(1).toString());
                observationKinds.insert(kind);
                if (timestamp.isValid()) {
                    if (!firstObservation.isValid() || timestamp < firstObservation) {
                        firstObservation = timestamp;
                    }
                    if (!lastObservation.isValid() || timestamp > lastObservation) {
                        lastObservation = timestamp;
                    }
                }
                sampleCount++;
            }
        }

        // A schema-v4 database may contain legacy snapshots whose normalized
        // observations were intentionally marked unknown. Keep the source
        // discoverable, but do not advertise unsupported metrics for it.
        if (!firstObservation.isValid()) {
            QSqlQuery boundsQuery(m_db);
            boundsQuery.prepare(
                QStringLiteral("SELECT MIN(timestamp), MAX(timestamp), COUNT(*) "
                               "FROM usage_snapshots WHERE provider = ?"));
            boundsQuery.addBindValue(provider);
            if (boundsQuery.exec() && boundsQuery.next()) {
                firstObservation = parseSnapshotTimestamp(boundsQuery.value(0).toString());
                lastObservation = parseSnapshotTimestamp(boundsQuery.value(1).toString());
                sampleCount = boundsQuery.value(2).toInt();
            }
        }

        QStringList metrics;
        if (observationKinds.contains(QStringLiteral("cost"))) {
            metrics.append(QStringLiteral("cost"));
        }
        if (observationKinds.contains(QStringLiteral("input_tokens")) || observationKinds.contains(QStringLiteral("output_tokens"))) {
            metrics.append(QStringLiteral("tokens"));
        }
        if (observationKinds.contains(QStringLiteral("requests"))) {
            metrics.append(QStringLiteral("requests"));
        }

        QSqlQuery ratePairQuery(m_db);
        ratePairQuery.prepare(QStringLiteral(
            "SELECT 1 FROM observations limits "
            "JOIN observations remaining "
            "ON remaining.provider = limits.provider "
            "AND remaining.correlation_id = limits.correlation_id "
            "AND remaining.scope = limits.scope "
            "AND remaining.window = limits.window "
            "AND remaining.source = limits.source "
            "WHERE limits.provider = ? AND limits.value > 0 "
            "AND remaining.value IS NOT NULL "
            "AND lower(limits.source) != 'unknown' "
            "AND lower(remaining.source) != 'unknown' "
            "AND lower(limits.source) NOT LIKE '%connectivity%' "
            "AND lower(remaining.source) NOT LIKE '%connectivity%' "
            "AND ((limits.metric_kind = 'request_limit' "
            "AND remaining.metric_kind = 'request_remaining') "
            "OR (limits.metric_kind = 'token_limit' "
            "AND remaining.metric_kind = 'token_remaining')) "
            "LIMIT 1"));
        ratePairQuery.addBindValue(provider);
        if (ratePairQuery.exec() && ratePairQuery.next()) {
            metrics.append(QStringLiteral("rateLimitUsed"));
        }

        QVariantMap row;
        row.insert(QStringLiteral("historyId"),
                   QStringLiteral("provider:") + provider);
        row.insert(QStringLiteral("dbName"), provider);
        row.insert(QStringLiteral("displayName"), provider);
        row.insert(QStringLiteral("sourceKind"), QStringLiteral("provider"));
        row.insert(QStringLiteral("metricKinds"), metrics);
        row.insert(QStringLiteral("sampleCount"), sampleCount);
        row.insert(QStringLiteral("firstObservation"), firstObservation);
        row.insert(QStringLiteral("lastObservation"), lastObservation);
        catalog.append(row);
    }

    const QStringList tools = getToolNames();
    for (const QString &tool : tools) {
        QSqlQuery query(m_db);
        query.prepare(
            QStringLiteral("SELECT MIN(timestamp), MAX(timestamp), COUNT(*), "
                           "SUM(CASE WHEN usage_limit > 0 THEN 1 ELSE 0 END) "
                           "FROM subscription_tool_usage WHERE tool_name = ?"));
        query.addBindValue(tool);
        if (!query.exec() || !query.next()) {
            continue;
        }

        QStringList metrics{QStringLiteral("usageCount")};
        if (query.value(3).toInt() > 0) {
            metrics.append(QStringLiteral("percentUsed"));
            metrics.append(QStringLiteral("remaining"));
        }

        QVariantMap row;
        row.insert(QStringLiteral("historyId"), QStringLiteral("tool:") + tool);
        row.insert(QStringLiteral("dbName"), tool);
        row.insert(QStringLiteral("displayName"), tool);
        row.insert(QStringLiteral("sourceKind"), QStringLiteral("tool"));
        row.insert(QStringLiteral("metricKinds"), metrics);
        row.insert(QStringLiteral("sampleCount"), query.value(2).toInt());
        row.insert(QStringLiteral("firstObservation"),
                   parseSnapshotTimestamp(query.value(0).toString()));
        row.insert(QStringLiteral("lastObservation"),
                   parseSnapshotTimestamp(query.value(1).toString()));
        catalog.append(row);
    }

    return catalog;
}

void UsageDatabase::requestHistoryCatalog(const QString &requestId)
{
    initDatabase();
    m_latestHistoryCatalogRequestId = requestId;
    beginWorker();
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this,
            [this, watcher, requestId]() {
                if (m_latestHistoryCatalogRequestId == requestId) {
                    Q_EMIT historyCatalogReady(requestId, watcher->result());
                }
                finishWorker();
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([]() {
        UsageDatabase workerDatabase;
        workerDatabase.init();
        return workerDatabase.getHistoryCatalog();
    }));
}

QVariantMap UsageDatabase::getHistorySeries(const QVariantList &sources,
                                            const QDateTime &from,
                                            const QDateTime &to,
                                            const QString &metric,
                                            int bucketMinutes) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("errorKey"), QStringLiteral("invalid_request"));
    result.insert(QStringLiteral("metricKind"), metric);
    result.insert(QStringLiteral("series"), QVariantList());

    const QSet<QString> supportedMetrics{
        QStringLiteral("cost"), QStringLiteral("tokens"),
        QStringLiteral("requests"), QStringLiteral("rateLimitUsed"),
        QStringLiteral("usageCount"), QStringLiteral("percentUsed"),
        QStringLiteral("remaining")};
    const QDateTime fromUtc = from.toUTC();
    const QDateTime toUtc = to.toUTC();
    if (!m_initialized || sources.isEmpty() || !supportedMetrics.contains(metric) || !fromUtc.isValid() || !toUtc.isValid() || fromUtc >= toUtc) {
        return result;
    }

    QVariantList allSeries;
    for (const QVariant &sourceValue : sources) {
        QVariantMap descriptor = sourceValue.toMap();
        const QString sourceKind = descriptor.value(QStringLiteral("sourceKind")).toString();
        const QString dbName = descriptor.value(QStringLiteral("dbName")).toString();
        if (dbName.isEmpty() || (sourceKind != QLatin1String("provider") && sourceKind != QLatin1String("tool"))) {
            continue;
        }
        if (!descriptor.contains(QStringLiteral("historyId"))) {
            descriptor.insert(QStringLiteral("historyId"),
                              sourceKind + QLatin1Char(':') + dbName);
        }

        QList<HistorySample> samples;
        if (sourceKind == QLatin1String("tool")) {
            if (metric != QLatin1String("usageCount") && metric != QLatin1String("percentUsed") && metric != QLatin1String("remaining")) {
                continue;
            }
            QSqlQuery query(m_db);
            query.prepare(QStringLiteral(
                "SELECT timestamp, usage_count, usage_limit, period_type "
                "FROM subscription_tool_usage "
                "WHERE tool_name = ? AND timestamp >= ? AND timestamp <= ? "
                "ORDER BY timestamp"));
            query.addBindValue(dbName);
            query.addBindValue(toDbDateTimeString(fromUtc));
            query.addBindValue(toDbDateTimeString(toUtc));
            if (!query.exec()) {
                continue;
            }
            while (query.next()) {
                const int usageCount = query.value(1).toInt();
                const int usageLimit = query.value(2).toInt();
                if ((metric == QLatin1String("percentUsed") || metric == QLatin1String("remaining")) && usageLimit <= 0) {
                    continue;
                }
                HistorySample sample;
                sample.timestamp = parseSnapshotTimestamp(query.value(0).toString());
                sample.value = metric == QLatin1String("usageCount")
                    ? static_cast<double>(usageCount)
                    : metric == QLatin1String("remaining")
                    ? static_cast<double>(qMax(0, usageLimit - usageCount))
                    : static_cast<double>(usageCount) * 100.0 / static_cast<double>(usageLimit);
                sample.unit = metric == QLatin1String("percentUsed")
                    ? QStringLiteral("percent")
                    : QStringLiteral("count");
                sample.semantic = QStringLiteral("rolling_gauge");
                sample.source = QStringLiteral("local_tool");
                sample.quality = QStringLiteral("estimated");
                sample.scope = QStringLiteral("subscription");
                sample.window = query.value(3).toString();
                samples.append(sample);
            }
        } else {
            if (metric == QLatin1String("usageCount") || metric == QLatin1String("percentUsed") || metric == QLatin1String("remaining")) {
                continue;
            }

            if (metric == QLatin1String("cost")
                || metric == QLatin1String("requests")) {
                const QString expectedKind = metric == QLatin1String("cost")
                    ? QStringLiteral("cost")
                    : QStringLiteral("requests");
                const int bucketSecs =
                    effectiveBucketSeconds(fromUtc, toUtc, bucketMinutes);
                QSqlQuery query(m_db);
                query.prepare(QStringLiteral(
                    "WITH filtered AS ("
                    " SELECT id, observed_at_utc, unit, value, currency, semantic,"
                    " source, data_quality, scope, window, reset_at_utc,"
                    " CAST((unixepoch(observed_at_utc) - ?) / ? AS INTEGER)"
                    " AS bucket_index"
                    " FROM observations WHERE provider = ?"
                    " AND unixepoch(observed_at_utc) >= ?"
                    " AND unixepoch(observed_at_utc) <= ?"
                    " AND metric_kind = ? AND value IS NOT NULL"
                    " AND lower(source) != 'unknown'"
                    " AND lower(source) NOT LIKE '%connectivity%'"
                    " AND lower(source) NOT LIKE '%model_discovery%'"
                    "), ranked AS ("
                    " SELECT *,"
                    " SUM(value) OVER bucket AS bucket_sum,"
                    " COUNT(*) OVER bucket AS bucket_count,"
                    " MIN(unixepoch(observed_at_utc)) OVER segment"
                    " AS segment_first,"
                    " MAX(unixepoch(observed_at_utc)) OVER segment"
                    " AS segment_last,"
                    " ROW_NUMBER() OVER (bucket ORDER BY"
                    " unixepoch(observed_at_utc) DESC, id DESC) AS bucket_rank"
                    " FROM filtered"
                    " WINDOW bucket AS (PARTITION BY bucket_index, unit,"
                    " currency, semantic, source, data_quality, scope, window),"
                    " segment AS (PARTITION BY unit, currency, semantic, source,"
                    " scope, window)"
                    ")"
                    " SELECT bucket_index, unit,"
                    " CASE WHEN semantic = 'interval_total' THEN bucket_sum"
                    " ELSE value END,"
                    " currency, semantic, source, data_quality, scope, window,"
                    " reset_at_utc, bucket_count, segment_first, segment_last"
                    " FROM ranked WHERE bucket_rank = 1"
                    " ORDER BY bucket_index"));
                query.addBindValue(fromUtc.toSecsSinceEpoch());
                query.addBindValue(bucketSecs);
                query.addBindValue(dbName);
                query.addBindValue(fromUtc.toSecsSinceEpoch());
                query.addBindValue(toUtc.toSecsSinceEpoch());
                query.addBindValue(expectedKind);
                if (!query.exec()) {
                    qWarning() << "UsageDatabase: bucketed history query failed"
                               << query.lastError().text();
                    continue;
                }
                while (query.next()) {
                    HistorySample sample;
                    sample.timestamp =
                        fromUtc.addSecs(query.value(0).toLongLong() * bucketSecs);
                    sample.unit = metric == QLatin1String("cost")
                        ? QStringLiteral("currency")
                        : QStringLiteral("request");
                    sample.value = query.value(2).toDouble();
                    const QString rawCurrency =
                        query.value(3).toString().trimmed().toUpper();
                    const QString rawUnit =
                        query.value(1).toString().trimmed().toUpper();
                    sample.currency = metric == QLatin1String("cost")
                        ? (!rawCurrency.isEmpty() ? rawCurrency
                           : rawUnit.size() == 3 ? rawUnit
                                                 : QStringLiteral("UNKNOWN"))
                        : QString();
                    sample.semantic = query.value(4).toString();
                    sample.source = query.value(5).toString();
                    sample.quality = query.value(6).toString();
                    sample.scope = query.value(7).toString();
                    sample.window = query.value(8).toString();
                    sample.resetAt =
                        parseSnapshotTimestamp(query.value(9).toString());
                    sample.sampleCount = query.value(10).toInt();
                    sample.firstObservation =
                        QDateTime::fromSecsSinceEpoch(query.value(11).toLongLong(),
                                                     QTimeZone::utc());
                    sample.lastObservation =
                        QDateTime::fromSecsSinceEpoch(query.value(12).toLongLong(),
                                                     QTimeZone::utc());
                    samples.append(sample);
                }
            } else {
                QSqlQuery query(m_db);
                query.prepare(QStringLiteral(
                    "SELECT observed_at_utc, metric_kind, unit, value, "
                    "currency, "
                    "semantic, source, data_quality, scope, window, "
                    "reset_at_utc, "
                    "correlation_id "
                    "FROM observations WHERE provider = ? "
                    "AND unixepoch(observed_at_utc) >= ? "
                    "AND unixepoch(observed_at_utc) <= ? "
                    "AND value IS NOT NULL "
                    "AND lower(source) != 'unknown' "
                    "AND lower(source) NOT LIKE '%connectivity%' "
                    "AND lower(source) NOT LIKE '%model_discovery%' "
                    "ORDER BY observed_at_utc, id"));
                query.addBindValue(dbName);
                query.addBindValue(fromUtc.toSecsSinceEpoch());
                query.addBindValue(toUtc.toSecsSinceEpoch());
                if (!query.exec()) {
                    continue;
                }

                struct ObservationRow {
                    QDateTime timestamp;
                    QString kind;
                    QString unit;
                    double value = 0.0;
                    QString currency;
                    QString semantic;
                    QString source;
                    QString quality;
                    QString scope;
                    QString window;
                    QDateTime resetAt;
                    QString correlationId;
                };
                QList<ObservationRow> rows;
                while (query.next()) {
                    ObservationRow row;
                    row.timestamp =
                        parseSnapshotTimestamp(query.value(0).toString());
                    row.kind = query.value(1).toString();
                    row.unit = query.value(2).toString();
                    row.value = query.value(3).toDouble();
                    row.currency =
                        query.value(4).toString().trimmed().toUpper();
                    row.semantic = query.value(5).toString();
                    row.source = query.value(6).toString();
                    row.quality = query.value(7).toString();
                    row.scope = query.value(8).toString();
                    row.window = query.value(9).toString();
                    row.resetAt =
                        parseSnapshotTimestamp(query.value(10).toString());
                    row.correlationId = query.value(11).toString();
                    rows.append(row);
                }

                if (metric == QLatin1String("tokens")) {
                    QMap<QString, HistorySample> tokenGroups;
                    for (const ObservationRow &row : std::as_const(rows)) {
                        if (historyMetricForObservation(row.kind) !=
                            QLatin1String("tokens")) {
                            continue;
                        }
                        const QString groupKey =
                            QStringList{
                                row.correlationId,
                                row.timestamp.toString(Qt::ISODateWithMs),
                                row.semantic,
                                row.source,
                                row.quality,
                                row.scope,
                                row.window}
                                .join(QChar(0x1f));
                        HistorySample &sample = tokenGroups[groupKey];
                        sample.timestamp = row.timestamp;
                        sample.value += row.value;
                        sample.unit = QStringLiteral("token");
                        sample.semantic = row.semantic;
                        sample.source = row.source;
                        sample.quality = row.quality;
                        sample.scope = row.scope;
                        sample.window = row.window;
                        sample.resetAt = row.resetAt;
                    }
                    samples = tokenGroups.values();
                } else if (metric == QLatin1String("rateLimitUsed")) {
                    struct LimitPair {
                        ObservationRow limit;
                        ObservationRow remaining;
                        bool hasLimit = false;
                        bool hasRemaining = false;
                    };
                    QMap<QString, LimitPair> pairs;
                    for (const ObservationRow &row : std::as_const(rows)) {
                        const bool isRequest =
                            row.kind == QLatin1String("request_limit") ||
                            row.kind == QLatin1String("request_remaining");
                        const bool isToken =
                            row.kind == QLatin1String("token_limit") ||
                            row.kind == QLatin1String("token_remaining");
                        if (!isRequest && !isToken) {
                            continue;
                        }
                        const QString family = isRequest
                                                   ? QStringLiteral("request")
                                                   : QStringLiteral("token");
                        const QString key =
                            QStringList{row.correlationId, family, row.scope,
                                        row.window, row.source}
                                .join(QChar(0x1f));
                        LimitPair &pair = pairs[key];
                        if (row.kind.endsWith(QStringLiteral("_limit"))) {
                            pair.limit = row;
                            pair.hasLimit = true;
                        } else {
                            pair.remaining = row;
                            pair.hasRemaining = true;
                        }
                    }
                    for (const LimitPair &pair : std::as_const(pairs)) {
                        if (!pair.hasLimit || !pair.hasRemaining ||
                            pair.limit.value <= 0.0) {
                            continue;
                        }
                        HistorySample sample;
                        sample.timestamp = pair.remaining.timestamp;
                        sample.value =
                            qBound(0.0,
                                   (pair.limit.value - pair.remaining.value) *
                                       100.0 / pair.limit.value,
                                   100.0);
                        sample.unit = QStringLiteral("percent");
                        sample.semantic = QStringLiteral("rolling_gauge");
                        sample.source = pair.remaining.source;
                        sample.quality = pair.remaining.quality;
                        sample.scope = pair.remaining.scope;
                        sample.window = pair.remaining.window;
                        sample.resetAt = pair.remaining.resetAt;
                        samples.append(sample);
                    }
                }
            }
        }

        const QVariantMap sourcePayload = buildHistorySeriesPayload(
            descriptor, metric, samples, fromUtc, toUtc, bucketMinutes);
        allSeries.append(sourcePayload.value(QStringLiteral("series")).toList());
    }

    if (sources.size() > 1 && !allSeries.isEmpty()) {
        QSet<QString> units;
        QSet<QString> currencies;
        QSet<QString> semantics;
        for (const QVariant &seriesValue : std::as_const(allSeries)) {
            const QVariantMap series = seriesValue.toMap();
            units.insert(series.value(QStringLiteral("unit")).toString());
            const QString currency = series.value(QStringLiteral("currency")).toString();
            if (!currency.isEmpty()) {
                currencies.insert(currency);
            }
            semantics.insert(series.value(QStringLiteral("semantic")).toString());
        }
        if (units.size() > 1) {
            result.insert(QStringLiteral("errorKey"),
                          QStringLiteral("incompatible_units"));
            return result;
        }
        if (metric == QLatin1String("cost") && currencies.size() > 1) {
            result.insert(QStringLiteral("errorKey"),
                          QStringLiteral("mixed_currencies"));
            return result;
        }
        if (semantics.size() > 1) {
            result.insert(QStringLiteral("errorKey"),
                          QStringLiteral("incompatible_semantics"));
            return result;
        }
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("errorKey"), QString());
    result.insert(QStringLiteral("series"), allSeries);
    return result;
}

void UsageDatabase::requestHistorySeries(const QString &requestId,
                                         const QVariantList &sources,
                                         const QDateTime &from,
                                         const QDateTime &to,
                                         const QString &metric,
                                         int bucketMinutes)
{
    initDatabase();
    m_latestHistorySeriesRequestId = requestId;
    beginWorker();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId]() {
                if (m_latestHistorySeriesRequestId == requestId) {
                    Q_EMIT historySeriesReady(requestId, watcher->result());
                }
                finishWorker();
                watcher->deleteLater();
            });
    watcher->setFuture(
        QtConcurrent::run([sources, from, to, metric, bucketMinutes]() {
            UsageDatabase workerDatabase;
            workerDatabase.init();
            return workerDatabase.getHistorySeries(sources, from, to, metric,
                                                   bucketMinutes);
        }));
}

QStringList UsageDatabase::getProviders() const
{
    QStringList providers;

    if (!m_initialized)
        return providers;

    QSqlQuery query(m_db);
    query.exec(QStringLiteral(
        "SELECT DISTINCT provider FROM usage_snapshots ORDER BY provider"));

    while (query.next()) {
        providers.append(query.value(0).toString());
    }

    return providers;
}

QVariantList UsageDatabase::getToolSnapshots(const QString &toolName,
                                             const QDateTime &from,
                                             const QDateTime &to) const
{
    QVariantList results;

    if (!m_initialized)
        return results;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT timestamp, usage_count, usage_limit, period_type, "
        "plan_tier, limit_reached "
        "FROM subscription_tool_usage "
        "WHERE tool_name = ? AND timestamp >= ? AND timestamp <= ? "
        "ORDER BY timestamp ASC"));
    query.addBindValue(toolName);
    query.addBindValue(toDbDateTimeString(from));
    query.addBindValue(toDbDateTimeString(to));

    if (!query.exec()) {
        qWarning() << "UsageDatabase: getToolSnapshots query failed:"
                   << query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap row;
        row[QStringLiteral("timestamp")] = query.value(0).toString();
        row[QStringLiteral("usageCount")] = query.value(1).toInt();
        row[QStringLiteral("usageLimit")] = query.value(2).toInt();
        row[QStringLiteral("periodType")] = query.value(3).toString();
        row[QStringLiteral("planTier")] = query.value(4).toString();
        row[QStringLiteral("limitReached")] = query.value(5).toBool();
        int limit = query.value(2).toInt();
        row[QStringLiteral("percentUsed")] = limit > 0 ? qRound(query.value(1).toDouble() / limit * 100.0) : 0;
        results.append(row);
    }

    return results;
}

QStringList UsageDatabase::getToolNames() const
{
    QStringList names;

    if (!m_initialized)
        return names;

    QSqlQuery query(m_db);
    query.exec(QStringLiteral("SELECT DISTINCT tool_name FROM "
                              "subscription_tool_usage ORDER BY tool_name"));

    while (query.next()) {
        names.append(query.value(0).toString());
    }

    return names;
}

QVariantList UsageDatabase::getProviderSeries(const QStringList &providers,
                                              const QDateTime &from,
                                              const QDateTime &to,
                                              const QString &metric,
                                              int bucketMinutes) const
{
    QVariantList results;

    if (!m_initialized || providers.isEmpty()) {
        return results;
    }

    if (metric != QStringLiteral("cost") && metric != QStringLiteral("tokens") && metric != QStringLiteral("requests") && metric != QStringLiteral("rateLimitUsed")) {
        return results;
    }

    const QDateTime fromUtc = from.toUTC();
    const QDateTime toUtc = to.toUTC();
    if (!fromUtc.isValid() || !toUtc.isValid() || fromUtc >= toUtc) {
        return results;
    }

    const int bucketSecs = effectiveBucketSeconds(fromUtc, toUtc, bucketMinutes);

    for (const QString &provider : providers) {
        if (provider.isEmpty()) {
            continue;
        }

        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT timestamp, cost, input_tokens, output_tokens, request_count, "
            "rl_requests, rl_requests_remaining, currency "
            "FROM usage_snapshots "
            "WHERE provider = ? AND timestamp >= ? AND timestamp <= ? "
            "ORDER BY timestamp ASC"));
        query.addBindValue(provider);
        query.addBindValue(toDbDateTimeString(fromUtc));
        query.addBindValue(toDbDateTimeString(toUtc));

        if (!query.exec()) {
            qWarning() << "UsageDatabase: getProviderSeries query failed for"
                       << provider << ":" << query.lastError().text();
            continue;
        }

        QMap<qint64, BucketAggregate> buckets;
        int sampleCount = 0;
        QString currency;
        bool mixedCurrency = false;

        while (query.next()) {
            const QDateTime ts = parseSnapshotTimestamp(query.value(0).toString());
            if (!ts.isValid() || ts < fromUtc || ts > toUtc) {
                continue;
            }
            const QString rowCurrency = query.value(7).toString().trimmed().toUpper();
            if (!rowCurrency.isEmpty()) {
                if (currency.isEmpty())
                    currency = rowCurrency;
                else if (currency != rowCurrency)
                    mixedCurrency = true;
            }

            double value = 0.0;
            if (metric == QStringLiteral("cost")) {
                value = query.value(1).toDouble();
            } else if (metric == QStringLiteral("tokens")) {
                value = static_cast<double>(query.value(2).toLongLong() + query.value(3).toLongLong());
            } else if (metric == QStringLiteral("requests")) {
                value = static_cast<double>(query.value(4).toInt());
            } else if (metric == QStringLiteral("rateLimitUsed")) {
                const int limit = query.value(5).toInt();
                const int remaining = query.value(6).toInt();
                if (limit > 0) {
                    value = (static_cast<double>(limit - remaining) / static_cast<double>(limit)) * 100.0;
                }
            }

            const qint64 bucketIndex = fromUtc.secsTo(ts) / bucketSecs;
            BucketAggregate &bucket = buckets[bucketIndex];
            if (!bucket.bucketStart.isValid()) {
                bucket.bucketStart = fromUtc.addSecs(bucketIndex * bucketSecs);
            }
            bucket.sum += value;
            bucket.count++;
            sampleCount++;
        }

        const QVariantList points = bucketToPoints(buckets);
        QVariantMap series;
        series[QStringLiteral("name")] = provider;
        series[QStringLiteral("points")] = points;
        series[QStringLiteral("sampleCount")] = sampleCount;
        series[QStringLiteral("currency")] = currency.isEmpty() ? QStringLiteral("USD") : currency;
        series[QStringLiteral("mixedCurrency")] = mixedCurrency;

        double latestValue = 0.0;
        double change = 0.0;
        if (!points.isEmpty()) {
            const double first = points.first().toMap().value(QStringLiteral("value")).toDouble();
            latestValue = points.last().toMap().value(QStringLiteral("value")).toDouble();
            change = deltaPercent(first, latestValue);
        }

        series[QStringLiteral("latestValue")] = latestValue;
        series[QStringLiteral("deltaPercent")] = change;
        results.append(series);
    }

    return results;
}

QVariantList UsageDatabase::getToolSeries(const QStringList &tools,
                                          const QDateTime &from,
                                          const QDateTime &to,
                                          const QString &metric,
                                          int bucketMinutes) const
{
    QVariantList results;

    if (!m_initialized || tools.isEmpty()) {
        return results;
    }

    if (metric != QStringLiteral("percentUsed") && metric != QStringLiteral("usageCount") && metric != QStringLiteral("remaining")) {
        return results;
    }

    const QDateTime fromUtc = from.toUTC();
    const QDateTime toUtc = to.toUTC();
    if (!fromUtc.isValid() || !toUtc.isValid() || fromUtc >= toUtc) {
        return results;
    }

    const int bucketSecs = effectiveBucketSeconds(fromUtc, toUtc, bucketMinutes);

    for (const QString &tool : tools) {
        if (tool.isEmpty()) {
            continue;
        }

        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT timestamp, usage_count, usage_limit "
            "FROM subscription_tool_usage "
            "WHERE tool_name = ? AND timestamp >= ? AND timestamp <= ? "
            "ORDER BY timestamp ASC"));
        query.addBindValue(tool);
        query.addBindValue(toDbDateTimeString(fromUtc));
        query.addBindValue(toDbDateTimeString(toUtc));

        if (!query.exec()) {
            qWarning() << "UsageDatabase: getToolSeries query failed for" << tool
                       << ":" << query.lastError().text();
            continue;
        }

        QMap<qint64, BucketAggregate> buckets;
        int sampleCount = 0;

        while (query.next()) {
            const QDateTime ts = parseSnapshotTimestamp(query.value(0).toString());
            if (!ts.isValid() || ts < fromUtc || ts > toUtc) {
                continue;
            }

            const int usageCount = query.value(1).toInt();
            const int usageLimit = query.value(2).toInt();

            double value = 0.0;
            if (metric == QStringLiteral("usageCount")) {
                value = static_cast<double>(usageCount);
            } else if (metric == QStringLiteral("remaining")) {
                value = static_cast<double>(qMax(0, usageLimit - usageCount));
            } else if (metric == QStringLiteral("percentUsed")) {
                if (usageLimit > 0) {
                    value = (static_cast<double>(usageCount) / static_cast<double>(usageLimit)) * 100.0;
                }
            }

            const qint64 bucketIndex = fromUtc.secsTo(ts) / bucketSecs;
            BucketAggregate &bucket = buckets[bucketIndex];
            if (!bucket.bucketStart.isValid()) {
                bucket.bucketStart = fromUtc.addSecs(bucketIndex * bucketSecs);
            }
            bucket.sum += value;
            bucket.count++;
            sampleCount++;
        }

        const QVariantList points = bucketToPoints(buckets);
        QVariantMap series;
        series[QStringLiteral("name")] = tool;
        series[QStringLiteral("points")] = points;
        series[QStringLiteral("sampleCount")] = sampleCount;

        double latestValue = 0.0;
        double change = 0.0;
        if (!points.isEmpty()) {
            const double first = points.first().toMap().value(QStringLiteral("value")).toDouble();
            latestValue = points.last().toMap().value(QStringLiteral("value")).toDouble();
            change = deltaPercent(first, latestValue);
        }

        series[QStringLiteral("latestValue")] = latestValue;
        series[QStringLiteral("deltaPercent")] = change;
        results.append(series);
    }

    return results;
}

QString UsageDatabase::exportCsv(const QString &provider, const QDateTime &from,
                                 const QDateTime &to) const
{
    QString csv;
    csv += QStringLiteral(
        "timestamp,provider,model,input_tokens,output_tokens,request_count,"
        "cost,is_estimated_cost,daily_cost,monthly_cost,rl_requests,rl_requests_"
        "remaining,"
        "rl_tokens,rl_tokens_remaining,cost_source,usage_source,currency,data_"
        "quality\n");

    QVariantList snapshots = getSnapshots(provider, from, to);
    for (const QVariant &snap : snapshots) {
        QVariantMap row = snap.toMap();
        const QStringList fields = {
            row[QStringLiteral("timestamp")].toString(),
            provider,
            row[QStringLiteral("model")].toString(),
            QString::number(row[QStringLiteral("inputTokens")].toLongLong()),
            QString::number(row[QStringLiteral("outputTokens")].toLongLong()),
            QString::number(row[QStringLiteral("requestCount")].toInt()),
            QString::number(row[QStringLiteral("cost")].toDouble(), 'f', 6),
            row[QStringLiteral("isEstimatedCost")].toBool() ? QStringLiteral("1")
                                                            : QStringLiteral("0"),
            QString::number(row[QStringLiteral("dailyCost")].toDouble(), 'f', 6),
            QString::number(row[QStringLiteral("monthlyCost")].toDouble(), 'f', 6),
            QString::number(row[QStringLiteral("rlRequests")].toInt()),
            QString::number(row[QStringLiteral("rlRequestsRemaining")].toInt()),
            QString::number(row[QStringLiteral("rlTokens")].toInt()),
            QString::number(row[QStringLiteral("rlTokensRemaining")].toInt()),
            row[QStringLiteral("costSource")].toString(),
            row[QStringLiteral("usageSource")].toString(),
            row[QStringLiteral("currency")].toString(),
            row[QStringLiteral("dataQuality")].toString()};
        QStringList escapedFields;
        escapedFields.reserve(fields.size());
        for (const QString &field : fields) {
            escapedFields.append(csvField(field));
        }
        csv += escapedFields.join(QLatin1Char(',')) + QLatin1Char('\n');
    }

    return csv;
}

QString UsageDatabase::exportJson(const QString &provider,
                                  const QDateTime &from,
                                  const QDateTime &to) const
{
    QVariantList snapshots = getSnapshots(provider, from, to);

    QJsonArray arr;
    for (const QVariant &snap : snapshots) {
        arr.append(QJsonObject::fromVariantMap(snap.toMap()));
    }

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 5;
    root[QStringLiteral("provider")] = provider;
    root[QStringLiteral("from")] = from.toString(Qt::ISODate);
    root[QStringLiteral("to")] = to.toString(Qt::ISODate);
    root[QStringLiteral("snapshots")] = arr;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QStringList
UsageDatabase::exportAllToDirectory(const QString &dirPath,
                                    const QStringList &formats) const
{
    QStringList writtenFiles;

    if (!m_initialized || dirPath.trimmed().isEmpty()) {
        return writtenFiles;
    }

    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "UsageDatabase: Failed to create export directory:"
                   << dirPath;
        return writtenFiles;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss"));
    const QStringList requestedFormats = formats.isEmpty()
        ? QStringList{QStringLiteral("json"), QStringLiteral("csv")}
        : formats;

    if (requestedFormats.contains(QStringLiteral("json"))) {
        const QString jsonPath = dir.filePath(QStringLiteral("ai-usage-export-%1.json").arg(timestamp));
        QSaveFile jsonFile(jsonPath);
        if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            jsonFile.write("{\n  \"schemaVersion\": 5,\n  \"exportedAt\": ");
            const QByteArray exportedAt
                = QJsonDocument(QJsonArray { QDateTime::currentDateTimeUtc().toString(Qt::ISODate) })
                      .toJson(QJsonDocument::Compact);
            jsonFile.write(exportedAt.mid(1, exportedAt.size() - 2));
            jsonFile.write(",\n  \"providerSnapshots\": [\n");

            auto streamJsonRows = [&jsonFile](QSqlQuery &query) {
                bool first = true;
                while (query.next()) {
                    QJsonObject row;
                    const QSqlRecord record = query.record();
                    for (int i = 0; i < record.count(); ++i) {
                        row.insert(record.fieldName(i),
                            query.isNull(i) ? QJsonValue(QJsonValue::Null) : QJsonValue::fromVariant(query.value(i)));
                    }
                    if (!first)
                        jsonFile.write(",\n");
                    jsonFile.write("    ");
                    jsonFile.write(QJsonDocument(row).toJson(QJsonDocument::Compact));
                    first = false;
                }
            };

            QSqlQuery providerQuery(m_db);
            providerQuery.setForwardOnly(true);
            const bool providersOk
                = providerQuery.exec(QStringLiteral("SELECT "
                                                    "provider,timestamp,model,input_tokens,output_tokens,"
                                                    "request_count,cost,is_estimated_cost,"
                                                    "daily_cost,monthly_cost,rl_requests,rl_requests_"
                                                    "remaining,rl_tokens,rl_tokens_remaining,"
                                                    "cost_source,usage_source,currency,data_quality FROM "
                                                    "usage_snapshots ORDER BY id"));
            if (providersOk)
                streamJsonRows(providerQuery);

            jsonFile.write("\n  ],\n  \"observations\": [\n");
            QSqlQuery observationQuery(m_db);
            observationQuery.setForwardOnly(true);
            const bool observationsOk
                = observationQuery.exec(QStringLiteral("SELECT "
                                                       "provider,observed_at_utc,interval_start_utc,interval_end_utc,"
                                                       "metric_"
                                                       "kind,unit,value,currency,semantic,source,data_quality,scope,"
                                                       "window,"
                                                       "model_scope,project_scope,reset_at_utc,correlation_id "
                                                       "FROM observations ORDER BY id"));
            if (observationsOk)
                streamJsonRows(observationQuery);

            jsonFile.write("\n  ],\n  \"guardrailEvents\": [\n");
            QSqlQuery guardrailQuery(m_db);
            guardrailQuery.setForwardOnly(true);
            const bool guardrailsOk
                = guardrailQuery.exec(QStringLiteral("SELECT stable_id,source_id,source_kind,risk_kind,window_key,"
                                                     "scope,transition,observed_at_utc,predicted_at_utc,"
                                                     "period_end_utc,method_id,evidence_grade,current_value,"
                                                     "projected_value,limit_value,unit,currency,value_class,"
                                                     "reason_key FROM guardrail_events ORDER BY id"));
            if (guardrailsOk)
                streamJsonRows(guardrailQuery);

            jsonFile.write("\n  ],\n  \"toolSnapshots\": [\n");
            QSqlQuery toolQuery(m_db);
            toolQuery.setForwardOnly(true);
            const bool toolsOk = toolQuery.exec(QStringLiteral("SELECT "
                                                               "tool_name,timestamp,usage_count,usage_limit,period_"
                                                               "type,plan_tier,limit_reached "
                                                               "FROM subscription_tool_usage ORDER BY id"));
            if (toolsOk)
                streamJsonRows(toolQuery);
            jsonFile.write("\n  ]\n}\n");

            if (providersOk && observationsOk && guardrailsOk && toolsOk && jsonFile.commit()) {
                writtenFiles.append(jsonPath);
            } else {
                jsonFile.cancelWriting();
            }
        }
    }

    if (requestedFormats.contains(QStringLiteral("csv"))) {
        const QString observationsCsvPath = dir.filePath(
            QStringLiteral("ai-usage-observations-%1.csv").arg(timestamp));
        QSaveFile observationsFile(observationsCsvPath);
        if (observationsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            observationsFile.write(
                "provider,observed_at,interval_start,interval_end,metric_kind,unit,"
                "value,currency,semantic,source,data_quality,scope,window,model_"
                "scope,project_scope,reset_at,correlation_id\r\n");
            QSqlQuery query(m_db);
            query.setForwardOnly(true);
            const bool queryOk = query.exec(QStringLiteral(
                "SELECT "
                "provider,observed_at_utc,interval_start_utc,interval_end_utc,metric_"
                "kind,unit,value,currency,semantic,source,data_quality,scope,window,"
                "model_scope,project_scope,reset_at_utc,correlation_id FROM "
                "observations ORDER BY id"));
            while (queryOk && query.next()) {
                QStringList fields;
                for (int i = 0; i < 17; ++i)
                    fields.append(csvField(query.value(i).toString()));
                observationsFile.write(
                    (fields.join(QLatin1Char(',')) + QStringLiteral("\r\n")).toUtf8());
            }
            if (queryOk && observationsFile.commit())
                writtenFiles.append(observationsCsvPath);
            else
                observationsFile.cancelWriting();
        }

        const QString guardrailsCsvPath
            = dir.filePath(QStringLiteral("ai-usage-guardrail-events-%1.csv").arg(timestamp));
        QSaveFile guardrailsFile(guardrailsCsvPath);
        if (guardrailsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            guardrailsFile.write("stable_id,source_id,source_kind,risk_kind,window_key,scope,"
                                 "transition,observed_at,predicted_at,period_end,method_id,"
                                 "evidence_grade,current_value,projected_value,limit_value,unit,"
                                 "currency,value_class,reason_key\r\n");
            QSqlQuery query(m_db);
            query.setForwardOnly(true);
            const bool queryOk
                = query.exec(QStringLiteral("SELECT stable_id,source_id,source_kind,risk_kind,window_key,"
                                            "scope,transition,observed_at_utc,predicted_at_utc,"
                                            "period_end_utc,method_id,evidence_grade,current_value,"
                                            "projected_value,limit_value,unit,currency,value_class,"
                                            "reason_key FROM guardrail_events ORDER BY id"));
            while (queryOk && query.next()) {
                QStringList fields;
                for (int i = 0; i < 19; ++i)
                    fields.append(csvField(query.value(i).toString()));
                guardrailsFile.write((fields.join(QLatin1Char(',')) + QStringLiteral("\r\n")).toUtf8());
            }
            if (queryOk && guardrailsFile.commit()) {
                writtenFiles.append(guardrailsCsvPath);
            } else {
                guardrailsFile.cancelWriting();
            }
        }

        const QString providerCsvPath = dir.filePath(
            QStringLiteral("ai-usage-providers-%1.csv").arg(timestamp));
        QSaveFile providerFile(providerCsvPath);
        if (providerFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            providerFile.write("provider,timestamp,model,input_tokens,output_tokens,"
                               "request_count,cost,is_estimated_cost,"
                               "daily_cost,monthly_cost,rl_requests,rl_requests_"
                               "remaining,rl_tokens,rl_tokens_remaining,"
                               "cost_source,usage_source,currency,data_quality\r\n");
            QSqlQuery query(m_db);
            query.setForwardOnly(true);
            const bool queryOk = query.exec(
                QStringLiteral("SELECT "
                               "provider,timestamp,model,input_tokens,output_tokens,"
                               "request_count,cost,is_estimated_cost,"
                               "daily_cost,monthly_cost,rl_requests,rl_requests_"
                               "remaining,rl_tokens,rl_tokens_remaining,"
                               "cost_source,usage_source,currency,data_quality FROM "
                               "usage_snapshots ORDER BY id"));
            while (queryOk && query.next()) {
                QStringList fields;
                for (int i = 0; i < 18; ++i)
                    fields.append(csvField(query.value(i).toString()));
                providerFile.write(
                    (fields.join(QLatin1Char(',')) + QStringLiteral("\r\n")).toUtf8());
            }
            if (queryOk && providerFile.commit()) {
                writtenFiles.append(providerCsvPath);
            } else {
                providerFile.cancelWriting();
            }
        }

        const QString toolCsvPath = dir.filePath(QStringLiteral("ai-usage-tools-%1.csv").arg(timestamp));
        QSaveFile toolFile(toolCsvPath);
        if (toolFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            toolFile.write("tool,timestamp,usage_count,usage_limit,period_type,plan_"
                           "tier,limit_reached,percent_used\r\n");
            QSqlQuery query(m_db);
            query.setForwardOnly(true);
            const bool queryOk = query.exec(
                QStringLiteral("SELECT "
                               "tool_name,timestamp,usage_count,usage_limit,period_"
                               "type,plan_tier,limit_reached,"
                               "CASE WHEN usage_limit > 0 THEN (usage_count * 100.0 "
                               "/ usage_limit) ELSE 0 END "
                               "FROM subscription_tool_usage ORDER BY id"));
            while (queryOk && query.next()) {
                QStringList fields;
                for (int i = 0; i < 8; ++i)
                    fields.append(csvField(query.value(i).toString()));
                toolFile.write(
                    (fields.join(QLatin1Char(',')) + QStringLiteral("\r\n")).toUtf8());
            }
            if (queryOk && toolFile.commit()) {
                writtenFiles.append(toolCsvPath);
            } else {
                toolFile.cancelWriting();
            }
        }
    }

    return writtenFiles;
}

void UsageDatabase::requestExportAll(const QString &requestId,
                                     const QString &dirPath,
                                     const QStringList &formats)
{
    initDatabase();
    beginWorker();
    auto *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this,
            [this, watcher, requestId]() {
                Q_EMIT exportFinished(requestId, watcher->result());
                finishWorker();
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([dirPath, formats]() {
        UsageDatabase workerDatabase;
        workerDatabase.init();
        return workerDatabase.exportAllToDirectory(dirPath, formats);
    }));
}

void UsageDatabase::init()
{
    if (m_enabled) {
        initDatabase();
    }
}

void UsageDatabase::pruneOldData()
{
    if (!m_initialized)
        return;

    QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-m_retentionDays);
    QString cutoffStr = toDbDateTimeString(cutoff);

    // Wrap all deletes in a single transaction for atomicity and performance
    m_db.transaction();

    int totalDeleted = 0;

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral("DELETE FROM usage_snapshots WHERE timestamp < ?"));
    query.addBindValue(cutoffStr);
    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to prune snapshots:"
                   << query.lastError().text();
    } else {
        totalDeleted += query.numRowsAffected();
    }

    query.prepare(
        QStringLiteral("DELETE FROM rate_limit_events WHERE timestamp < ?"));
    query.addBindValue(cutoffStr);
    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to prune events:"
                   << query.lastError().text();
    } else {
        totalDeleted += query.numRowsAffected();
    }

    query.prepare(QStringLiteral(
        "DELETE FROM subscription_tool_usage WHERE timestamp < ?"));
    query.addBindValue(cutoffStr);
    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to prune tool usage:"
                   << query.lastError().text();
    } else {
        totalDeleted += query.numRowsAffected();
    }

    query.prepare(
        QStringLiteral("DELETE FROM observations WHERE observed_at_utc < ?"));
    query.addBindValue(cutoffStr);
    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to prune observations:"
                   << query.lastError().text();
    } else {
        totalDeleted += query.numRowsAffected();
    }

    query.prepare(QStringLiteral("DELETE FROM guardrail_events WHERE observed_at_utc < ?"));
    query.addBindValue(cutoffStr);
    if (!query.exec()) {
        qWarning() << "UsageDatabase: Failed to prune guardrail events:" << query.lastError().text();
    } else {
        totalDeleted += query.numRowsAffected();
    }

    m_db.commit();

    // Only vacuum if a meaningful number of rows were deleted
    if (totalDeleted > 100) {
        QSqlQuery vacuum(m_db);
        vacuum.exec(QStringLiteral("PRAGMA incremental_vacuum"));
    }
}

qint64 UsageDatabase::databaseSize() const
{
    if (!m_initialized)
        return 0;

    QFileInfo fi(m_db.databaseName());
    return fi.size();
}

QVariantMap UsageDatabase::getYearlyActivity(int mode) const
{
    QVariantMap result;
    QVariantList days;
    double maxIntensity = 0.0;

    if (!m_initialized) {
        result["maxIntensity"] = 0.0;
        result["days"] = days;
        return result;
    }

    QSqlQuery query(m_db);
    if (mode == 0) {
        QSqlQuery currencyQuery(m_db);
        currencyQuery.exec(QStringLiteral(
            "SELECT DISTINCT currency FROM observations WHERE metric_kind='cost' "
            "AND semantic='interval_total' AND observed_at_utc >= date('now','-365 "
            "days') "
            "ORDER BY currency"));
        QStringList currencies;
        while (currencyQuery.next())
            currencies.append(currencyQuery.value(0).toString());
        result[QStringLiteral("currencies")] = currencies;
        result[QStringLiteral("mixedCurrencies")] = currencies.size() > 1;
        if (currencies.size() > 1) {
            result[QStringLiteral("maxIntensity")] = 0.0;
            result[QStringLiteral("days")] = days;
            return result;
        }
        result[QStringLiteral("currency")] = currencies.value(0, QStringLiteral("USD"));
        query.prepare(QStringLiteral(
            "SELECT date(observed_at_utc), SUM(value) FROM observations "
            "WHERE metric_kind='cost' AND semantic='interval_total' "
            "AND observed_at_utc >= date('now','-365 days') "
            "GROUP BY date(observed_at_utc) ORDER BY date(observed_at_utc)"));
    } else {
        query.prepare(
            QStringLiteral("SELECT day, SUM(provider_tokens) FROM ("
                           " SELECT date(timestamp) AS day, provider, "
                           "MAX(input_tokens + output_tokens) AS provider_tokens "
                           " FROM usage_snapshots WHERE timestamp >= "
                           "date('now','-365 days') GROUP BY day, provider"
                           ") GROUP BY day ORDER BY day"));
    }

    if (!query.exec()) {
        qWarning() << "UsageDatabase: getYearlyActivity query failed:"
                   << query.lastError().text();
        result["maxIntensity"] = 0.0;
        result["days"] = days;
        return result;
    }

    while (query.next()) {
        QVariantMap day;
        day[QStringLiteral("date")] = query.value(0).toString();
        double val = query.value(1).toDouble();
        day[QStringLiteral("value")] = val;
        days.append(day);

        if (val > maxIntensity) {
            maxIntensity = val;
        }
    }

    result[QStringLiteral("maxIntensity")] = maxIntensity;
    result[QStringLiteral("days")] = days;
    return result;
}

QVariantList UsageDatabase::getEfficiencySeries(int daysCount) const
{
    QVariantList series;

    if (!m_initialized)
        return series;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT day, "
                                 "SUM(provider_input) as total_in, "
                                 "SUM(provider_output) as total_out "
                                 "FROM ("
                                 "  SELECT date(timestamp) as day, provider, "
                                 "         MAX(input_tokens) as provider_input, "
                                 "         MAX(output_tokens) as provider_output "
                                 "  FROM usage_snapshots "
                                 "  WHERE timestamp >= date('now', '-%1 days') "
                                 "  GROUP BY day, provider"
                                 ") "
                                 "GROUP BY day "
                                 "HAVING SUM(provider_input) > 0 "
                                 "ORDER BY day ASC")
                      .arg(daysCount));

    if (!query.exec()) {
        qWarning() << "UsageDatabase: getEfficiencySeries query failed:"
                   << query.lastError().text();
        return series;
    }

    while (query.next()) {
        QVariantMap entry;
        entry[QStringLiteral("date")] = query.value(0).toString();

        double input = query.value(1).toDouble();
        double output = query.value(2).toDouble();

        entry[QStringLiteral("value")] = output / input;
        series.append(entry);
    }

    return series;
}

QVariantMap UsageDatabase::getAnalystOverview(int days) const
{
    QVariantMap result;
    result[QStringLiteral("averageDailyCost")] = 0.0;
    result[QStringLiteral("currentDailyCost")] = 0.0;
    result[QStringLiteral("weekOverWeekPercent")] = 0.0;
    result[QStringLiteral("volatilityPercent")] = 0.0;
    result[QStringLiteral("anomalyCount")] = 0;
    result[QStringLiteral("anomalies")] = QVariantList{};
    result[QStringLiteral("topDrivers")] = QVariantList{};
    result[QStringLiteral("topModels")] = QVariantList{};
    result[QStringLiteral("days")] = QVariantList{};
    result[QStringLiteral("hasEstimatedData")] = false;
    result[QStringLiteral("hasProbeOnlyData")] = false;

    if (!m_initialized) {
        return result;
    }

    const int clampedDays = qBound(7, days, 365);
    QSqlQuery currencyQuery(m_db);
    currencyQuery.prepare(
        QStringLiteral(
            "SELECT DISTINCT currency FROM observations WHERE metric_kind='cost' "
            "AND source != 'connectivity_probe' AND observed_at_utc >= "
            "date('now','-%1 days') "
            "ORDER BY currency")
            .arg(clampedDays));
    QStringList currencies;
    if (currencyQuery.exec()) {
        while (currencyQuery.next())
            currencies.append(currencyQuery.value(0).toString());
    }
    result[QStringLiteral("currencies")] = currencies;
    result[QStringLiteral("mixedCurrencies")] = currencies.size() > 1;
    result[QStringLiteral("currency")] = currencies.value(0, QStringLiteral("USD"));
    if (currencies.size() > 1) {
        return result;
    }
    QList<DailyOverviewRow> dailyRows;

    QSqlQuery dayQuery(m_db);
    dayQuery.prepare(
        QStringLiteral("SELECT date(observed_at_utc) AS day, SUM(value) AS "
                       "total_cost, 0, COUNT(DISTINCT provider) "
                       "FROM observations WHERE metric_kind='cost' AND "
                       "semantic='interval_total' "
                       "AND source != 'connectivity_probe' AND observed_at_utc "
                       ">= date('now','-%1 days') "
                       "GROUP BY date(observed_at_utc) ORDER BY day")
            .arg(clampedDays));

    if (!dayQuery.exec()) {
        qWarning() << "UsageDatabase: getAnalystOverview daily query failed:"
                   << dayQuery.lastError().text();
        return result;
    }

    QVariantList dayMaps;
    while (dayQuery.next()) {
        DailyOverviewRow row;
        row.day = dayQuery.value(0).toString();
        row.totalCost = dayQuery.value(1).toDouble();
        row.totalTokens = dayQuery.value(2).toDouble();
        row.providerCount = dayQuery.value(3).toInt();
        dailyRows.append(row);

        QVariantMap map;
        map[QStringLiteral("date")] = row.day;
        map[QStringLiteral("totalCost")] = row.totalCost;
        map[QStringLiteral("totalTokens")] = row.totalTokens;
        map[QStringLiteral("providerCount")] = row.providerCount;
        dayMaps.append(map);
    }
    result[QStringLiteral("days")] = dayMaps;

    if (!dailyRows.isEmpty()) {
        double sum = 0.0;
        QList<double> costs;
        costs.reserve(dailyRows.size());
        for (const DailyOverviewRow &row : std::as_const(dailyRows)) {
            sum += row.totalCost;
            costs.append(row.totalCost);
        }

        const double averageDailyCost = sum / static_cast<double>(dailyRows.size());
        const double currentDailyCost = dailyRows.last().totalCost;
        result[QStringLiteral("averageDailyCost")] = averageDailyCost;
        result[QStringLiteral("currentDailyCost")] = currentDailyCost;

        double variance = 0.0;
        if (!qFuzzyIsNull(averageDailyCost)) {
            for (double cost : std::as_const(costs)) {
                const double delta = cost - averageDailyCost;
                variance += delta * delta;
            }
            variance /= static_cast<double>(costs.size());
            result[QStringLiteral("volatilityPercent")] = (std::sqrt(variance) / averageDailyCost) * 100.0;
        }

        if (dailyRows.size() >= 14) {
            double previousWeek = 0.0;
            double currentWeek = 0.0;
            const int split = dailyRows.size() - 7;
            for (int i = std::max(0, split - 7); i < split; ++i) {
                previousWeek += dailyRows.at(i).totalCost;
            }
            for (int i = split; i < dailyRows.size(); ++i) {
                currentWeek += dailyRows.at(i).totalCost;
            }
            result[QStringLiteral("weekOverWeekPercent")] = percentChange(previousWeek, currentWeek);
        }

        QVariantList anomalies;
        const double baseline = averageDailyCost;
        for (const DailyOverviewRow &row : std::as_const(dailyRows)) {
            if (baseline <= 0.0) {
                continue;
            }
            if (row.totalCost < baseline * 1.75 || row.totalCost < baseline + 0.25) {
                continue;
            }

            QVariantMap anomaly;
            anomaly[QStringLiteral("date")] = row.day;
            anomaly[QStringLiteral("value")] = row.totalCost;
            anomaly[QStringLiteral("deltaPercent")] = percentChange(baseline, row.totalCost);
            anomalies.append(anomaly);
        }
        result[QStringLiteral("anomalies")] = anomalies;
        result[QStringLiteral("anomalyCount")] = anomalies.size();
    }

    QSqlQuery latestQuery(m_db);
    latestQuery.prepare(
        QStringLiteral("SELECT provider, model, cost, is_estimated_cost, "
                       "daily_cost, monthly_cost, "
                       "       cost_source, usage_source, data_quality "
                       "FROM usage_snapshots "
                       "WHERE id IN ("
                       "  SELECT MAX(id) FROM usage_snapshots "
                       "  WHERE timestamp >= date('now', '-%1 days') "
                       "  GROUP BY provider"
                       ")")
            .arg(clampedDays));

    if (!latestQuery.exec()) {
        qWarning() << "UsageDatabase: getAnalystOverview driver query failed:"
                   << latestQuery.lastError().text();
        return result;
    }

    struct DriverRow {
        QString provider;
        QString model;
        double value = 0.0;
        bool estimated = false;
        QString costSource;
        QString usageSource;
        QString dataQuality;
    };

    QList<DriverRow> drivers;
    QMap<QString, double> modelTotals;
    QMap<QString, bool> modelEstimated;
    const int daysInMonth = QDate::currentDate().daysInMonth();

    while (latestQuery.next()) {
        DriverRow row;
        row.provider = latestQuery.value(0).toString();
        row.model = latestQuery.value(1).toString().trimmed();
        const double cost = latestQuery.value(2).toDouble();
        row.estimated = latestQuery.value(3).toBool();
        const double dailyCost = latestQuery.value(4).toDouble();
        const double monthlyCost = latestQuery.value(5).toDouble();
        row.costSource = latestQuery.value(6).toString();
        row.usageSource = latestQuery.value(7).toString();
        row.dataQuality = latestQuery.value(8).toString();
        if (row.estimated || row.costSource == QLatin1String("estimated_from_usage")) {
            result[QStringLiteral("hasEstimatedData")] = true;
        }
        if (row.costSource == QLatin1String("connectivity_probe") || row.usageSource == QLatin1String("connectivity_probe")) {
            result[QStringLiteral("hasProbeOnlyData")] = true;
            continue;
        }

        row.value = monthlyCost > 0.0
            ? monthlyCost
            : (dailyCost > 0.0 ? dailyCost * static_cast<double>(daysInMonth)
                               : cost);
        if (row.value <= 0.0) {
            continue;
        }

        if (row.model.isEmpty()) {
            row.model = row.provider;
        }

        drivers.append(row);
        modelTotals[row.model] += row.value;
        modelEstimated[row.model] = modelEstimated.value(row.model, false) || row.estimated || row.costSource == QLatin1String("estimated_from_usage");
    }

    std::sort(drivers.begin(), drivers.end(),
              [](const DriverRow &lhs, const DriverRow &rhs) {
                  return lhs.value > rhs.value;
              });

    QVariantList topDrivers;
    const int driverLimit = std::min(5, static_cast<int>(drivers.size()));
    for (int i = 0; i < driverLimit; ++i) {
        const DriverRow &driver = drivers.at(i);
        QVariantMap map;
        map[QStringLiteral("provider")] = driver.provider;
        map[QStringLiteral("model")] = driver.model;
        map[QStringLiteral("value")] = driver.value;
        map[QStringLiteral("estimated")] = driver.estimated;
        map[QStringLiteral("costSource")] = driver.costSource;
        map[QStringLiteral("usageSource")] = driver.usageSource;
        map[QStringLiteral("dataQuality")] = driver.dataQuality;
        topDrivers.append(map);
    }
    result[QStringLiteral("topDrivers")] = topDrivers;

    struct ModelRow {
        QString model;
        double value = 0.0;
        bool estimated = false;
    };

    QList<ModelRow> models;
    for (auto it = modelTotals.constBegin(); it != modelTotals.constEnd(); ++it) {
        ModelRow row;
        row.model = it.key();
        row.value = it.value();
        row.estimated = modelEstimated.value(it.key(), false);
        models.append(row);
    }

    std::sort(models.begin(), models.end(),
              [](const ModelRow &lhs, const ModelRow &rhs) {
                  return lhs.value > rhs.value;
              });

    QVariantList topModels;
    const int modelLimit = std::min(5, static_cast<int>(models.size()));
    for (int i = 0; i < modelLimit; ++i) {
        const ModelRow &model = models.at(i);
        QVariantMap map;
        map[QStringLiteral("model")] = model.model;
        map[QStringLiteral("value")] = model.value;
        map[QStringLiteral("estimated")] = model.estimated;
        topModels.append(map);
    }
    result[QStringLiteral("topModels")] = topModels;

    return result;
}

QVariantMap UsageDatabase::getAnalystSnapshot(const QDateTime &fromInclusive,
                                              const QDateTime &toExclusive,
                                              const QString &requestedCurrency) const
{
    return AnalystQuery::execute(m_db, m_initialized, fromInclusive,
                                 toExclusive, requestedCurrency);
}

void UsageDatabase::requestAnalyst(const QString &requestId,
                                   const QDateTime &fromInclusive,
                                   const QDateTime &toExclusive,
                                   const QString &currency)
{
    initDatabase();
    m_latestAnalystRequestId = requestId;
    beginWorker();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, requestId]() {
        if (m_latestAnalystRequestId == requestId) {
            QVariantMap snapshot = watcher->result();
            snapshot.insert(QStringLiteral("requestId"), requestId);
            Q_EMIT analystReady(requestId, snapshot);
        }
        finishWorker();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([fromInclusive, toExclusive, currency]() {
        UsageDatabase workerDatabase;
        workerDatabase.init();
        return workerDatabase.getAnalystSnapshot(fromInclusive, toExclusive,
                                                 currency);
    }));
}
