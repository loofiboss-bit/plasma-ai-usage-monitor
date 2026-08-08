#include "budgetpolicyschema.h"

#include <QFile>
#include <QFileInfo>
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
  if (currentVersion >= Version) {
    return true;
  }
  versionQuery.finish();
  if (currentVersion != 5) {
    if (error) {
      *error = QStringLiteral("Schema v6 migration requires schema v5");
    }
    return false;
  }

  const QString databasePath = database.databaseName();
  const QString backupPath = databasePath + QStringLiteral(".v18-backup");
  if (QFileInfo::exists(databasePath) && !QFileInfo::exists(backupPath)) {
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
  const QStringList statements = {
      QStringLiteral(
          "CREATE TABLE budget_policies ("
          " owner_id TEXT NOT NULL, policy_id TEXT PRIMARY KEY,"
          " source_id TEXT NOT NULL, source_kind TEXT NOT NULL,"
          " scope_mode TEXT NOT NULL CHECK(scope_mode IN "
          "('aggregate','scoped')),"
          " scope_kind TEXT NOT NULL DEFAULT '', scope_identity TEXT NOT NULL "
          "DEFAULT '',"
          " scope_label TEXT NOT NULL DEFAULT '',"
          " value_class TEXT NOT NULL CHECK(value_class IN "
          "('actual','estimated')),"
          " limit_minor INTEGER NOT NULL CHECK(limit_minor > 0),"
          " currency TEXT NOT NULL CHECK(length(currency) = 3),"
          " period_type TEXT NOT NULL CHECK(period_type IN "
          "('calendar_day','iso_week','calendar_month','anchored_month','"
          "provider_reset')),"
          " anchor_day INTEGER CHECK(anchor_day IS NULL OR anchor_day BETWEEN "
          "1 AND 28),"
          " time_zone_id TEXT NOT NULL,"
          " warning_percent INTEGER NOT NULL CHECK(warning_percent > 0 AND "
          "warning_percent <= 100),"
          " critical_percent INTEGER NOT NULL CHECK(critical_percent >= "
          "warning_percent AND critical_percent <= 100),"
          " notify_enabled INTEGER NOT NULL DEFAULT 1 CHECK(notify_enabled IN "
          "(0,1)),"
          " enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0,1)),"
          " created_at_utc TEXT NOT NULL, updated_at_utc TEXT NOT NULL, "
          "snoozed_until_utc TEXT,"
          " CHECK((scope_mode = 'aggregate' AND scope_kind = '' AND "
          "scope_identity = '') OR "
          "       (scope_mode = 'scoped' AND scope_kind <> '' AND "
          "scope_identity <> '')),"
          " CHECK((period_type = 'anchored_month' AND anchor_day IS NOT NULL) "
          "OR "
          "       (period_type <> 'anchored_month' AND anchor_day IS NULL))"
          ")"),
      QStringLiteral(
          "CREATE TABLE budget_policy_migrations ("
          " owner_id TEXT NOT NULL, legacy_key TEXT NOT NULL, policy_id TEXT "
          "NOT NULL,"
          " migrated_at_utc TEXT NOT NULL, PRIMARY KEY(owner_id, legacy_key))"),
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
  };

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
  if (!exec(query, QStringLiteral("PRAGMA user_version = 6"), error) ||
      !database.commit()) {
    if (error && error->isEmpty()) {
      *error = database.lastError().text();
    }
    database.rollback();
    return false;
  }
  return true;
}
