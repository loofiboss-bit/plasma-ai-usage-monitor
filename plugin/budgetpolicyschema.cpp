#include "budgetpolicyschema.h"

#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>

namespace {
bool exec(QSqlQuery &query, const QString &sql, QString *error) {
  if (query.exec(sql)) {
    return true;
  }
  if (error) {
    *error = query.lastError().text();
  }
  return false;
}

bool tableExists(QSqlDatabase &database, const QString &table) {
  QSqlQuery query(database);
  query.prepare(QStringLiteral(
      "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
  query.addBindValue(table);
  return query.exec() && query.next();
}

bool columnExists(QSqlDatabase &database, const QString &table,
                  const QString &column) {
  QSqlQuery query(database);
  if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
    return false;
  }
  while (query.next()) {
    if (query.value(1).toString() == column) {
      return true;
    }
  }
  return false;
}

bool verifySqliteIntegrity(const QString &path) {
  const QString connection = QStringLiteral("session_backup_verify_%1")
                                 .arg(QRandomGenerator::global()->generate());
  auto backup =
      QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
  backup.setDatabaseName(path);
  const bool opened = backup.open();
  bool valid = false;
  if (opened) {
    QSqlQuery integrity(backup);
    valid = integrity.exec(QStringLiteral("PRAGMA integrity_check")) &&
            integrity.next() &&
            integrity.value(0).toString() == QLatin1String("ok");
    integrity.finish();
    backup.close();
  }
  backup = {};
  QSqlDatabase::removeDatabase(connection);
  return valid;
}
} // namespace

bool BudgetPolicySchema::migrate(QSqlDatabase &database, QString *error,
                                 bool injectFailure) {
  QSqlQuery versionQuery(database);
  if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) ||
      !versionQuery.next()) {
    if (error) {
      *error = versionQuery.lastError().text();
    }
    return false;
  }
  const int currentVersion = versionQuery.value(0).toInt();
  if (currentVersion > Version) {
    return true;
  }
  versionQuery.finish();
  if (currentVersion != 5 && currentVersion != Version) {
    if (error) {
      *error = QStringLiteral("Schema v6 migration requires schema v5");
    }
    return false;
  }

  const QString databasePath = database.databaseName();
  const QString backupPath = databasePath + QStringLiteral(".v18-backup");
  if (currentVersion == 5 && QFileInfo::exists(databasePath) &&
      !QFileInfo::exists(backupPath)) {
    QSqlQuery checkpoint(database);
    if (!checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"))) {
      if (error) {
        *error = checkpoint.lastError().text();
      }
      return false;
    }
    if (!QFile::copy(databasePath, backupPath)) {
      if (error) {
        *error = QStringLiteral("Unable to create v18 database backup");
      }
      return false;
    }
  }

  if (!database.transaction()) {
    if (error) {
      *error = database.lastError().text();
    }
    return false;
  }

  QSqlQuery query(database);
  QStringList statements;
  if (tableExists(database, QStringLiteral("observations"))) {
    if (!columnExists(database, QStringLiteral("observations"),
                      QStringLiteral("service_tier_scope"))) {
      statements.append(QStringLiteral(
          "ALTER TABLE observations ADD COLUMN service_tier_scope TEXT "
          "DEFAULT ''"));
    }
    if (!columnExists(database, QStringLiteral("observations"),
                      QStringLiteral("line_item_scope"))) {
      statements.append(QStringLiteral(
          "ALTER TABLE observations ADD COLUMN line_item_scope TEXT DEFAULT "
          "''"));
    }
  }
  if (currentVersion == 5) {
    statements.append({
        QStringLiteral(
            "CREATE TABLE budget_policies ("
            " owner_id TEXT NOT NULL, policy_id TEXT PRIMARY KEY,"
            " source_id TEXT NOT NULL, source_kind TEXT NOT NULL,"
            " scope_mode TEXT NOT NULL CHECK(scope_mode IN "
            "('aggregate','scoped')),"
            " scope_kind TEXT NOT NULL DEFAULT '', scope_identity TEXT NOT "
            "NULL "
            "DEFAULT '',"
            " scope_label TEXT NOT NULL DEFAULT '',"
            " value_class TEXT NOT NULL CHECK(value_class IN "
            "('actual','estimated')),"
            " limit_minor INTEGER NOT NULL CHECK(limit_minor > 0),"
            " currency TEXT NOT NULL CHECK(length(currency) = 3),"
            " period_type TEXT NOT NULL CHECK(period_type IN "
            "('calendar_day','iso_week','calendar_month','anchored_month','"
            "provider_reset')),"
            " anchor_day INTEGER CHECK(anchor_day IS NULL OR anchor_day "
            "BETWEEN "
            "1 AND 28),"
            " time_zone_id TEXT NOT NULL,"
            " warning_percent INTEGER NOT NULL CHECK(warning_percent > 0 AND "
            "warning_percent <= 100),"
            " critical_percent INTEGER NOT NULL CHECK(critical_percent >= "
            "warning_percent AND critical_percent <= 100),"
            " notify_enabled INTEGER NOT NULL DEFAULT 1 CHECK(notify_enabled "
            "IN "
            "(0,1)),"
            " enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0,1)),"
            " created_at_utc TEXT NOT NULL, updated_at_utc TEXT NOT NULL, "
            "snoozed_until_utc TEXT,"
            " CHECK((scope_mode = 'aggregate' AND scope_kind = '' AND "
            "scope_identity = '') OR "
            "       (scope_mode = 'scoped' AND scope_kind <> '' AND "
            "scope_identity <> '')),"
            " CHECK((period_type = 'anchored_month' AND anchor_day IS NOT "
            "NULL) "
            "OR "
            "       (period_type <> 'anchored_month' AND anchor_day IS NULL))"
            ")"),
        QStringLiteral(
            "CREATE TABLE budget_policy_migrations ("
            " owner_id TEXT NOT NULL, legacy_key TEXT NOT NULL, policy_id TEXT "
            "NOT NULL,"
            " migrated_at_utc TEXT NOT NULL, PRIMARY KEY(owner_id, "
            "legacy_key))"),
        QStringLiteral(
            "CREATE TABLE budget_policy_state ("
            " policy_id TEXT PRIMARY KEY, period_start_utc TEXT NOT NULL,"
            " period_end_utc TEXT NOT NULL, risk_level TEXT NOT NULL "
            "CHECK(risk_level IN "
            "('unavailable','safe','warning','critical','exceeded')),"
            " deduplication_key TEXT NOT NULL DEFAULT '', updated_at_utc TEXT "
            "NOT NULL,"
            " FOREIGN KEY(policy_id) REFERENCES budget_policies(policy_id) ON "
            "DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE budget_policy_events ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT, policy_id TEXT NOT NULL,"
            " period_start_utc TEXT NOT NULL, period_end_utc TEXT NOT NULL,"
            " transition TEXT NOT NULL CHECK(transition IN "
            "('warning','critical','exceeded','recovered','period_reset')),"
            " deduplication_key TEXT NOT NULL UNIQUE,"
            " delivery_status TEXT NOT NULL CHECK(delivery_status IN "
            "('pending','delivered','suppressed','failed')),"
            " created_at_utc TEXT NOT NULL, delivered_at_utc TEXT, reason_key "
            "TEXT NOT NULL DEFAULT '',"
            " FOREIGN KEY(policy_id) REFERENCES budget_policies(policy_id) ON "
            "DELETE CASCADE)"),
        QStringLiteral("CREATE INDEX idx_budget_policies_owner ON "
                       "budget_policies(owner_id)"),
        QStringLiteral("CREATE INDEX idx_budget_policies_source ON "
                       "budget_policies(owner_id, source_id)"),
        QStringLiteral("CREATE INDEX idx_budget_policies_scope ON "
                       "budget_policies(owner_id, scope_mode, scope_kind)"),
        QStringLiteral("CREATE INDEX idx_budget_policies_enabled ON "
                       "budget_policies(owner_id, enabled)"),
        QStringLiteral("CREATE INDEX idx_budget_events_policy_period ON "
                       "budget_policy_events(policy_id, period_start_utc)"),
    });
  }

  for (const QString &statement : statements) {
    if (!exec(query, statement, error)) {
      database.rollback();
      return false;
    }
  }
  if (injectFailure) {
    if (error) {
      *error = QStringLiteral("Injected schema v6 migration failure");
    }
    database.rollback();
    return false;
  }
  if ((currentVersion == 5 &&
       !exec(query, QStringLiteral("PRAGMA user_version = 6"), error)) ||
      !database.commit()) {
    if (error && error->isEmpty()) {
      *error = database.lastError().text();
    }
    database.rollback();
    return false;
  }
  return true;
}

bool BudgetPolicySchema::migrateDeliverySchema(QSqlDatabase &database,
                                               QString *error,
                                               bool injectFailure) {
  QSqlQuery version(database);
  if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())
    return false;
  const int current = version.value(0).toInt();
  version.finish();
  if (current == 9) {
    if (tableExists(database, QStringLiteral("budget_policy_deliveries")) &&
        tableExists(database, QStringLiteral("history_operations")))
      return true;
    if (error)
      *error = QStringLiteral("Incomplete schema v9 delivery tables");
    return false;
  }
  if (current == 8) {
    if (tableExists(database, QStringLiteral("budget_policy_deliveries")) &&
        tableExists(database, QStringLiteral("history_operations")))
      return true;
    if (error)
      *error = QStringLiteral("Incomplete schema v8");
    return false;
  }
  if (current != 7) {
    if (error)
      *error = QStringLiteral("Delivery migration requires schema v7");
    return false;
  }
  const QString path = database.databaseName();
  const QString backupPath = path + QStringLiteral(".v21-backup");
  if (current == 7 && path != QLatin1String(":memory:") &&
      QFileInfo::exists(path)) {
    QSqlQuery checkpoint(database);
    if (!checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)")) ||
        !checkpoint.next() || checkpoint.value(0).toInt() != 0 ||
        checkpoint.value(1).toInt() != checkpoint.value(2).toInt()) {
      if (error)
        *error = QStringLiteral("Unable to checkpoint pre-v22 database");
      return false;
    }
    checkpoint.finish();
    // Refresh the rollback snapshot on every v7 upgrade. A user may have
    // restored an earlier snapshot and added history before upgrading again.
    QFile source(path);
    QSaveFile backup(backupPath);
    backup.setDirectWriteFallback(false);
    if (!source.open(QIODevice::ReadOnly) ||
        !backup.open(QIODevice::WriteOnly)) {
      if (error)
        *error = QStringLiteral("Unable to create pre-v22 database backup");
      return false;
    }
    while (!source.atEnd()) {
      const QByteArray chunk = source.read(64 * 1024);
      if (source.error() != QFileDevice::NoError ||
          backup.write(chunk) != chunk.size()) {
        backup.cancelWriting();
        if (error)
          *error = QStringLiteral("Unable to write pre-v22 database backup");
        return false;
      }
    }
    if (!backup.commit()) {
      if (error)
        *error = QStringLiteral("Unable to commit pre-v22 database backup");
      return false;
    }
  }
  if (!database.transaction())
    return false;
  QSqlQuery query(database);
  const QStringList statements{
      QStringLiteral(
          "CREATE TABLE IF NOT EXISTS budget_policy_deliveries ("
          "event_id INTEGER NOT NULL, channel TEXT NOT NULL CHECK(channel IN "
          "('kde','slack','discord')),"
          "status TEXT NOT NULL CHECK(status IN "
          "('pending','in_flight','delivered','failed','suppressed')),"
          "attempts INTEGER NOT NULL DEFAULT 0, next_attempt_at_utc TEXT, "
          "delivered_at_utc TEXT,"
          "reason_key TEXT NOT NULL DEFAULT '', PRIMARY KEY(event_id,channel),"
          "FOREIGN KEY(event_id) REFERENCES budget_policy_events(id) ON DELETE "
          "CASCADE)"),
      QStringLiteral(
          "CREATE TABLE IF NOT EXISTS history_operations (kind TEXT PRIMARY "
          "KEY,"
          "status TEXT NOT NULL,error_key TEXT NOT NULL DEFAULT "
          "'',completed_at_utc TEXT,result_json TEXT NOT NULL DEFAULT '{}')"),
      QStringLiteral("PRAGMA user_version=8")};
  for (const auto &statement : statements) {
    if (!exec(query, statement, error)) {
      database.rollback();
      return false;
    }
  }
  if (injectFailure) {
    if (error)
      *error = QStringLiteral("Injected schema v8 migration failure");
    database.rollback();
    return false;
  }
  if (!database.commit()) {
    database.rollback();
    return false;
  }
  return true;
}

bool BudgetPolicySchema::migrateSessionSchema(QSqlDatabase &database,
                                              QString *error,
                                              bool injectFailure) {
  QSqlQuery version(database);
  if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())
    return false;
  const int current = version.value(0).toInt();
  version.finish();
  if (current == 9) {
    QSqlQuery conflicts(database);
    return tableExists(database, QStringLiteral("local_sessions")) &&
           tableExists(database, QStringLiteral("local_usage_events")) &&
           exec(conflicts,
                QStringLiteral(
                    "CREATE TABLE IF NOT EXISTS local_usage_conflicts "
                    "(event_id TEXT PRIMARY KEY)"),
                error);
  }
  if (current != 8) {
    if (error)
      *error = QStringLiteral("Session migration requires schema v8");
    return false;
  }

  const QString path = database.databaseName();
  if (path != QLatin1String(":memory:") && QFileInfo::exists(path)) {
    QSqlQuery checkpoint(database);
    if (!checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)")) ||
        !checkpoint.next() || checkpoint.value(0).toInt() != 0 ||
        checkpoint.value(1).toInt() != checkpoint.value(2).toInt()) {
      if (error)
        *error = QStringLiteral("Unable to checkpoint pre-v9 database");
      return false;
    }
    checkpoint.finish();
    QFile source(path);
    QSaveFile backup(path + QStringLiteral(".v24-backup"));
    backup.setDirectWriteFallback(false);
    if (!source.open(QIODevice::ReadOnly) ||
        !backup.open(QIODevice::WriteOnly)) {
      if (error)
        *error = QStringLiteral("Unable to create pre-v9 database backup");
      return false;
    }
    while (!source.atEnd()) {
      const QByteArray chunk = source.read(64 * 1024);
      if (source.error() != QFileDevice::NoError ||
          backup.write(chunk) != chunk.size()) {
        backup.cancelWriting();
        if (error)
          *error = QStringLiteral("Unable to write pre-v9 database backup");
        return false;
      }
    }
    if (!backup.commit()) {
      if (error)
        *error = QStringLiteral("Unable to commit pre-v9 database backup");
      return false;
    }
    if (!verifySqliteIntegrity(path + QStringLiteral(".v24-backup"))) {
      if (error)
        *error =
            QStringLiteral("Pre-v9 database backup failed integrity check");
      return false;
    }
  }
  if (!database.transaction()) {
    if (error)
      *error = database.lastError().text();
    return false;
  }
  QSqlQuery query(database);
  const QStringList statements{
      QStringLiteral(
          "CREATE TABLE local_sessions (session_id TEXT PRIMARY KEY, tool_id "
          "TEXT NOT NULL CHECK(tool_id IN ('codex','claude-code')), project_id "
          "TEXT NOT NULL, started_at_utc TEXT NOT NULL, last_seen_at_utc TEXT "
          "NOT NULL, coverage TEXT NOT NULL DEFAULT 'reported', source_id TEXT "
          "NOT NULL DEFAULT '')"),
      QStringLiteral("CREATE INDEX idx_local_sessions_tool_time ON "
                     "local_sessions(tool_id,last_seen_at_utc)"),
      QStringLiteral("CREATE INDEX idx_local_sessions_project_time ON "
                     "local_sessions(project_id,last_seen_at_utc)"),
      QStringLiteral(
          "CREATE TABLE local_usage_events (event_id TEXT PRIMARY KEY, "
          "session_id TEXT NOT NULL, observed_at_utc TEXT NOT NULL, model_id "
          "TEXT NOT NULL DEFAULT '', input_tokens INTEGER, output_tokens "
          "INTEGER, cache_read_tokens INTEGER, cache_write_tokens INTEGER, "
          "parser_revision INTEGER NOT NULL, provenance TEXT NOT NULL DEFAULT "
          "'', FOREIGN KEY(session_id) REFERENCES local_sessions(session_id) "
          "ON DELETE CASCADE)"),
      QStringLiteral("CREATE INDEX idx_local_events_session_time ON "
                     "local_usage_events(session_id,observed_at_utc)"),
      QStringLiteral(
          "CREATE TABLE local_import_checkpoints (tool_id TEXT PRIMARY KEY, "
          "generation INTEGER NOT NULL DEFAULT 0, cutoff_utc TEXT NOT NULL "
          "DEFAULT '', updated_at_utc TEXT NOT NULL DEFAULT '', coverage TEXT "
          "NOT NULL DEFAULT 'reported', rebaseline_pending INTEGER NOT NULL "
          "DEFAULT 0)"),
      QStringLiteral(
          "CREATE TABLE local_file_checkpoints (tool_id TEXT NOT NULL, file_id "
          "TEXT NOT NULL, file_offset INTEGER NOT NULL DEFAULT 0, file_size "
          "INTEGER NOT NULL DEFAULT 0, modified_at_utc TEXT NOT NULL DEFAULT "
          "'', PRIMARY KEY(tool_id,file_id))"),
      QStringLiteral(
          "CREATE TABLE local_session_counters (session_id TEXT PRIMARY KEY, "
          "observed_at_utc TEXT NOT NULL, input_tokens INTEGER, output_tokens "
          "INTEGER, cache_read_tokens INTEGER, cache_write_tokens INTEGER, "
          "FOREIGN KEY(session_id) REFERENCES local_sessions(session_id) ON "
          "DELETE CASCADE)"),
      QStringLiteral("CREATE TABLE local_project_aliases (project_id TEXT "
                     "PRIMARY KEY, alias TEXT NOT NULL)"),
      QStringLiteral("CREATE TABLE observed_account_quotas (tool_id TEXT NOT "
                     "NULL, observed_at_utc TEXT NOT NULL, quota_json TEXT NOT "
                     "NULL, PRIMARY KEY(tool_id,observed_at_utc))"),
      QStringLiteral(
          "CREATE TABLE local_usage_conflicts (event_id TEXT PRIMARY KEY)"),
      QStringLiteral("PRAGMA user_version=9")};
  for (const QString &statement : statements) {
    if (!exec(query, statement, error)) {
      database.rollback();
      return false;
    }
  }
  if (injectFailure) {
    if (error)
      *error = QStringLiteral("Injected schema v9 migration failure");
    database.rollback();
    return false;
  }
  if (!database.commit()) {
    if (error)
      *error = database.lastError().text();
    database.rollback();
    return false;
  }
  return true;
}
