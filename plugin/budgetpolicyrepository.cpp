#include "budgetpolicyrepository.h"

#include "budgetpolicyschema.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>

namespace {
const QStringList PolicyKeys = {
    QStringLiteral("policyId"),        QStringLiteral("sourceId"),
    QStringLiteral("sourceKind"),      QStringLiteral("scopeMode"),
    QStringLiteral("scopeKind"),       QStringLiteral("scopeIdentity"),
    QStringLiteral("scopeLabel"),      QStringLiteral("valueClass"),
    QStringLiteral("limitMinor"),      QStringLiteral("currency"),
    QStringLiteral("periodType"),      QStringLiteral("anchorDay"),
    QStringLiteral("timeZoneId"),      QStringLiteral("warningPercent"),
    QStringLiteral("criticalPercent"), QStringLiteral("notifyEnabled"),
    QStringLiteral("enabled"),         QStringLiteral("createdAtUtc"),
    QStringLiteral("updatedAtUtc"),    QStringLiteral("snoozedUntilUtc"),
};

QVariantMap result(bool ok, const QString &error = {},
                   const QVariantMap &policy = {}) {
  return {{QStringLiteral("ok"), ok},
          {QStringLiteral("error"), error},
          {QStringLiteral("policy"), policy}};
}
} // namespace

BudgetPolicyRepository::BudgetPolicyRepository(QObject *parent)
    : QObject(parent),
      m_connectionName(QStringLiteral("budget_policy_repository_%1")
                           .arg(reinterpret_cast<quintptr>(this))) {
  const QString root =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  m_databasePath =
      root + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
}

BudgetPolicyRepository::~BudgetPolicyRepository() {
  if (m_database.isValid()) {
    m_database.close();
    m_database = {};
  }
  QSqlDatabase::removeDatabase(m_connectionName);
}

QString BudgetPolicyRepository::ownerId() const { return m_ownerId; }
QString BudgetPolicyRepository::databasePath() const { return m_databasePath; }
QVariantList BudgetPolicyRepository::policies() const { return m_policies; }
qulonglong BudgetPolicyRepository::revision() const { return m_revision; }
QString BudgetPolicyRepository::errorString() const { return m_errorString; }

void BudgetPolicyRepository::setOwnerId(const QString &ownerId) {
  if (m_ownerId == ownerId)
    return;
  m_ownerId = ownerId.trimmed();
  Q_EMIT ownerIdChanged();
  if (m_database.isOpen())
    reload();
}

void BudgetPolicyRepository::setDatabasePath(const QString &databasePath) {
  if (m_database.isOpen() || m_databasePath == databasePath)
    return;
  m_databasePath = databasePath;
  Q_EMIT databasePathChanged();
}

void BudgetPolicyRepository::setError(const QString &error) {
  if (m_errorString == error)
    return;
  m_errorString = error;
  Q_EMIT errorStringChanged();
}

bool BudgetPolicyRepository::ensureOpen() {
  if (m_ownerId.isEmpty()) {
    setError(QStringLiteral("ownerId is required"));
    return false;
  }
  if (m_database.isOpen())
    return true;
  QDir().mkpath(QFileInfo(m_databasePath).absolutePath());
  m_database =
      QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
  m_database.setDatabaseName(m_databasePath);
  if (!m_database.open()) {
    setError(m_database.lastError().text());
    return false;
  }
  QSqlQuery pragma(m_database);
  pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
  QString error;
  const bool inject = qEnvironmentVariableIntValue(
                          "PLASMA_AI_MONITOR_INJECT_SCHEMA_V6_FAILURE") != 0;
  if (!BudgetPolicySchema::migrate(m_database, &error, inject)) {
    setError(error);
    m_database.close();
    return false;
  }
  setError({});
  return true;
}

bool BudgetPolicyRepository::init() {
  if (!ensureOpen())
    return false;
  reload();
  return true;
}

void BudgetPolicyRepository::reload() {
  if (!ensureOpen())
    return;
  QVariantList rows;
  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("SELECT "
                     "policy_id,source_id,source_kind,scope_mode,scope_kind,"
                     "scope_identity,scope_label,"
                     "value_class,limit_minor,currency,period_type,anchor_day,"
                     "time_zone_id,warning_percent,"
                     "critical_percent,notify_enabled,enabled,created_at_utc,"
                     "updated_at_utc,snoozed_until_utc "
                     "FROM budget_policies WHERE owner_id=? ORDER BY "
                     "created_at_utc,policy_id"));
  query.addBindValue(m_ownerId);
  if (!query.exec()) {
    setError(query.lastError().text());
    return;
  }
  while (query.next()) {
    QVariantMap row;
    for (int i = 0; i < PolicyKeys.size(); ++i)
      row.insert(PolicyKeys.at(i), query.value(i));
    row[QStringLiteral("notifyEnabled")] = query.value(15).toBool();
    row[QStringLiteral("enabled")] = query.value(16).toBool();
    rows.append(row);
  }
  m_policies = rows;
  ++m_revision;
  setError({});
  Q_EMIT policiesChanged();
  Q_EMIT revisionChanged();
}

QVariantMap BudgetPolicyRepository::normalizedPolicy(const QVariantMap &input,
                                                     bool newPolicy,
                                                     QString *error) const {
  QVariantMap p = input;
  const QDateTime now = QDateTime::currentDateTimeUtc();
  if (newPolicy && p.value(QStringLiteral("policyId")).toString().isEmpty())
    p[QStringLiteral("policyId")] =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (!p.contains(QStringLiteral("scopeMode")))
    p[QStringLiteral("scopeMode")] = QStringLiteral("aggregate");
  if (!p.contains(QStringLiteral("scopeKind")))
    p[QStringLiteral("scopeKind")] = QStringLiteral("");
  if (!p.contains(QStringLiteral("scopeIdentity")))
    p[QStringLiteral("scopeIdentity")] = QStringLiteral("");
  if (!p.contains(QStringLiteral("scopeLabel")))
    p[QStringLiteral("scopeLabel")] = QStringLiteral("");
  if (!p.contains(QStringLiteral("valueClass")))
    p[QStringLiteral("valueClass")] = QStringLiteral("actual");
  p[QStringLiteral("currency")] =
      p.value(QStringLiteral("currency"), QStringLiteral("USD"))
          .toString()
          .toUpper();
  if (!p.contains(QStringLiteral("periodType")))
    p[QStringLiteral("periodType")] = QStringLiteral("calendar_month");
  if (!p.contains(QStringLiteral("timeZoneId")))
    p[QStringLiteral("timeZoneId")] =
        QString::fromUtf8(QTimeZone::systemTimeZoneId());
  if (!p.contains(QStringLiteral("warningPercent")))
    p[QStringLiteral("warningPercent")] = 80;
  if (!p.contains(QStringLiteral("criticalPercent")))
    p[QStringLiteral("criticalPercent")] = 90;
  if (!p.contains(QStringLiteral("notifyEnabled")))
    p[QStringLiteral("notifyEnabled")] = true;
  if (!p.contains(QStringLiteral("enabled")))
    p[QStringLiteral("enabled")] = true;
  if (!p.contains(QStringLiteral("createdAtUtc")))
    p[QStringLiteral("createdAtUtc")] = now.toString(Qt::ISODateWithMs);
  p[QStringLiteral("updatedAtUtc")] = now.toString(Qt::ISODateWithMs);
  if (!p.contains(QStringLiteral("snoozedUntilUtc")))
    p[QStringLiteral("snoozedUntilUtc")] = QVariant();
  if (p.value(QStringLiteral("periodType")).toString() !=
      QLatin1String("anchored_month"))
    p[QStringLiteral("anchorDay")] = QVariant();

  const QString scopeMode = p.value(QStringLiteral("scopeMode")).toString();
  const QString periodType = p.value(QStringLiteral("periodType")).toString();
  const int warning = p.value(QStringLiteral("warningPercent")).toInt();
  const int critical = p.value(QStringLiteral("criticalPercent")).toInt();
  QString message;
  if (m_ownerId.isEmpty())
    message = QStringLiteral("ownerId is required");
  else if (QUuid(p.value(QStringLiteral("policyId")).toString()).isNull())
    message = QStringLiteral("policyId must be a UUID");
  else if (p.value(QStringLiteral("sourceId")).toString().trimmed().isEmpty())
    message = QStringLiteral("sourceId is required");
  else if (p.value(QStringLiteral("sourceKind")).toString().trimmed().isEmpty())
    message = QStringLiteral("sourceKind is required");
  else if (!QStringList{QStringLiteral("aggregate"), QStringLiteral("scoped")}
                .contains(scopeMode))
    message = QStringLiteral("invalid scopeMode");
  else if (scopeMode == QLatin1String("scoped") &&
           (p.value(QStringLiteral("scopeKind")).toString().isEmpty() ||
            p.value(QStringLiteral("scopeIdentity")).toString().isEmpty()))
    message =
        QStringLiteral("scoped policies require scopeKind and scopeIdentity");
  else if (!QStringList{QStringLiteral("actual"), QStringLiteral("estimated")}
                .contains(p.value(QStringLiteral("valueClass")).toString()))
    message = QStringLiteral("invalid valueClass");
  else if (p.value(QStringLiteral("limitMinor")).toLongLong() <= 0)
    message = QStringLiteral("limitMinor must be positive");
  else if (!QRegularExpression(QStringLiteral("^[A-Z]{3}$"))
                .match(p.value(QStringLiteral("currency")).toString())
                .hasMatch())
    message = QStringLiteral("currency must be an ISO 4217 code");
  else if (!QStringList{QStringLiteral("calendar_day"),
                        QStringLiteral("iso_week"),
                        QStringLiteral("calendar_month"),
                        QStringLiteral("anchored_month"),
                        QStringLiteral("provider_reset")}
                .contains(periodType))
    message = QStringLiteral("invalid periodType");
  else if (periodType == QLatin1String("anchored_month") &&
           (p.value(QStringLiteral("anchorDay")).toInt() < 1 ||
            p.value(QStringLiteral("anchorDay")).toInt() > 28))
    message = QStringLiteral("anchorDay must be between 1 and 28");
  else if (!QTimeZone(p.value(QStringLiteral("timeZoneId")).toString().toUtf8())
                .isValid())
    message = QStringLiteral("invalid timeZoneId");
  else if (warning <= 0 || warning > critical || critical > 100)
    message = QStringLiteral(
        "thresholds must satisfy 0 < warning <= critical <= 100");
  if (error)
    *error = message;
  return p;
}

QVariantMap
BudgetPolicyRepository::validatePolicy(const QVariantMap &policy) const {
  QString error;
  const QVariantMap normalized = normalizedPolicy(policy, true, &error);
  return result(error.isEmpty(), error, normalized);
}

bool BudgetPolicyRepository::writePolicy(const QVariantMap &p, bool update) {
  QSqlQuery query(m_database);
  query.prepare(update
                    ? QStringLiteral(
                          "UPDATE budget_policies SET "
                          "source_id=?,source_kind=?,scope_mode=?,scope_kind=?,"
                          "scope_identity=?,"
                          "scope_label=?,value_class=?,limit_minor=?,currency=?"
                          ",period_type=?,anchor_day=?,time_zone_id=?,"
                          "warning_percent=?,critical_percent=?,notify_enabled="
                          "?,enabled=?,updated_at_utc=?,snoozed_until_utc=? "
                          "WHERE owner_id=? AND policy_id=?")
                    : QStringLiteral(
                          "INSERT INTO "
                          "budget_policies(source_id,source_kind,scope_mode,"
                          "scope_kind,scope_identity,scope_label,"
                          "value_class,limit_minor,currency,period_type,anchor_"
                          "day,time_zone_id,warning_percent,critical_percent,"
                          "notify_enabled,enabled,updated_at_utc,snoozed_until_"
                          "utc,owner_id,policy_id,created_at_utc) "
                          "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  const QStringList fields = {
      QStringLiteral("sourceId"),       QStringLiteral("sourceKind"),
      QStringLiteral("scopeMode"),      QStringLiteral("scopeKind"),
      QStringLiteral("scopeIdentity"),  QStringLiteral("scopeLabel"),
      QStringLiteral("valueClass"),     QStringLiteral("limitMinor"),
      QStringLiteral("currency"),       QStringLiteral("periodType"),
      QStringLiteral("anchorDay"),      QStringLiteral("timeZoneId"),
      QStringLiteral("warningPercent"), QStringLiteral("criticalPercent"),
      QStringLiteral("notifyEnabled"),  QStringLiteral("enabled"),
      QStringLiteral("updatedAtUtc"),   QStringLiteral("snoozedUntilUtc")};
  const QStringList requiredStringFields = {
      QStringLiteral("sourceId"),      QStringLiteral("sourceKind"),
      QStringLiteral("scopeMode"),     QStringLiteral("scopeKind"),
      QStringLiteral("scopeIdentity"), QStringLiteral("scopeLabel"),
      QStringLiteral("valueClass"),    QStringLiteral("currency"),
      QStringLiteral("periodType"),    QStringLiteral("timeZoneId"),
      QStringLiteral("updatedAtUtc")};
  for (const QString &field : fields) {
    if (requiredStringFields.contains(field)) {
      const QString value = p.value(field).toString();
      query.addBindValue(value.isEmpty() ? QStringLiteral("") : value);
    } else {
      query.addBindValue(p.value(field));
    }
  }
  query.addBindValue(m_ownerId);
  query.addBindValue(p.value(QStringLiteral("policyId")));
  if (!update)
    query.addBindValue(p.value(QStringLiteral("createdAtUtc")));
  if (!query.exec() || (update && query.numRowsAffected() != 1)) {
    setError(query.lastError().text().isEmpty()
                 ? QStringLiteral("policy not found")
                 : query.lastError().text());
    return false;
  }
  return true;
}

QVariantMap BudgetPolicyRepository::createPolicy(const QVariantMap &policy) {
  if (!ensureOpen())
    return result(false, m_errorString);
  QString error;
  const QVariantMap p = normalizedPolicy(policy, true, &error);
  if (!error.isEmpty() || !writePolicy(p, false))
    return result(false, error.isEmpty() ? m_errorString : error);
  reload();
  return result(true, {}, p);
}

QVariantMap BudgetPolicyRepository::policyById(const QString &policyId) const {
  for (const QVariant &value : m_policies) {
    const QVariantMap policy = value.toMap();
    if (policy.value(QStringLiteral("policyId")).toString() == policyId)
      return policy;
  }
  return {};
}

QVariantMap BudgetPolicyRepository::updatePolicy(const QString &policyId,
                                                 const QVariantMap &changes) {
  if (!ensureOpen())
    return result(false, m_errorString);
  QVariantMap p = policyById(policyId);
  if (p.isEmpty())
    return result(false, QStringLiteral("policy not found"));
  for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
    if (it.key() != QLatin1String("policyId") &&
        it.key() != QLatin1String("createdAtUtc"))
      p[it.key()] = it.value();
  }
  QString error;
  p = normalizedPolicy(p, false, &error);
  if (!error.isEmpty() || !writePolicy(p, true))
    return result(false, error.isEmpty() ? m_errorString : error);
  reload();
  return result(true, {}, p);
}

QVariantMap BudgetPolicyRepository::duplicatePolicy(const QString &policyId) {
  QVariantMap p = policyById(policyId);
  if (p.isEmpty())
    return result(false, QStringLiteral("policy not found"));
  p.remove(QStringLiteral("policyId"));
  p.remove(QStringLiteral("createdAtUtc"));
  p.remove(QStringLiteral("updatedAtUtc"));
  p[QStringLiteral("snoozedUntilUtc")] = QVariant();
  return createPolicy(p);
}

bool BudgetPolicyRepository::deletePolicy(const QString &policyId) {
  if (!ensureOpen())
    return false;
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "DELETE FROM budget_policies WHERE owner_id=? AND policy_id=?"));
  query.addBindValue(m_ownerId);
  query.addBindValue(policyId);
  if (!query.exec() || query.numRowsAffected() != 1) {
    setError(QStringLiteral("policy not found"));
    return false;
  }
  reload();
  return true;
}

bool BudgetPolicyRepository::setPolicyEnabled(const QString &policyId,
                                              bool enabled) {
  return updatePolicy(policyId, {{QStringLiteral("enabled"), enabled}})
      .value(QStringLiteral("ok"))
      .toBool();
}

bool BudgetPolicyRepository::snoozePolicy(const QString &policyId,
                                          const QDateTime &untilUtc) {
  if (untilUtc.isValid() && untilUtc <= QDateTime::currentDateTimeUtc()) {
    setError(QStringLiteral("snooze must end in the future"));
    return false;
  }
  return updatePolicy(
             policyId,
             {{QStringLiteral("snoozedUntilUtc"),
               untilUtc.isValid() ? untilUtc.toUTC().toString(Qt::ISODateWithMs)
                                  : QVariant()}})
      .value(QStringLiteral("ok"))
      .toBool();
}

bool BudgetPolicyRepository::replacePolicies(const QVariantList &policies) {
  if (!ensureOpen())
    return false;
  QVariantList normalized;
  for (const QVariant &value : policies) {
    QString error;
    QVariantMap p = normalizedPolicy(value.toMap(), true, &error);
    if (!error.isEmpty()) {
      setError(error);
      return false;
    }
    normalized.append(p);
  }
  if (!m_database.transaction()) {
    setError(m_database.lastError().text());
    return false;
  }
  QSqlQuery remove(m_database);
  remove.prepare(
      QStringLiteral("DELETE FROM budget_policies WHERE owner_id=?"));
  remove.addBindValue(m_ownerId);
  if (!remove.exec()) {
    setError(remove.lastError().text());
    m_database.rollback();
    return false;
  }
  for (const QVariant &value : normalized) {
    if (!writePolicy(value.toMap(), false)) {
      m_database.rollback();
      reload();
      return false;
    }
  }
  if (!m_database.commit()) {
    setError(m_database.lastError().text());
    m_database.rollback();
    reload();
    return false;
  }
  reload();
  return true;
}

QString
BudgetPolicyRepository::deterministicPolicyId(const QString &ownerId,
                                              const QString &legacyKey) {
  QByteArray bytes = QCryptographicHash::hash(
                         (ownerId + QLatin1Char('\n') + legacyKey).toUtf8(),
                         QCryptographicHash::Sha256)
                         .toHex()
                         .left(32);
  return QStringLiteral("%1-%2-%3-%4-%5")
      .arg(QString::fromLatin1(bytes.mid(0, 8)),
           QString::fromLatin1(bytes.mid(8, 4)),
           QString::fromLatin1(bytes.mid(12, 4)),
           QString::fromLatin1(bytes.mid(16, 4)),
           QString::fromLatin1(bytes.mid(20, 12)));
}

bool BudgetPolicyRepository::migrateLegacyBudgets(
    const QVariantList &legacyBudgets) {
  if (!ensureOpen())
    return false;
  struct Pending {
    QString key;
    QVariantMap policy;
  };
  QList<Pending> pending;
  for (const QVariant &value : legacyBudgets) {
    const QVariantMap legacy = value.toMap();
    const QString key = legacy.value(QStringLiteral("legacyKey")).toString();
    if (key.isEmpty()) {
      setError(QStringLiteral("legacyKey is required"));
      return false;
    }
    QSqlQuery exists(m_database);
    exists.prepare(QStringLiteral("SELECT 1 FROM budget_policy_migrations "
                                  "WHERE owner_id=? AND legacy_key=?"));
    exists.addBindValue(m_ownerId);
    exists.addBindValue(key);
    if (!exists.exec()) {
      setError(exists.lastError().text());
      return false;
    }
    if (exists.next())
      continue;
    if (legacy.value(QStringLiteral("limitMinor")).toLongLong() <= 0)
      continue;
    QVariantMap p = legacy;
    p.remove(QStringLiteral("legacyKey"));
    p[QStringLiteral("policyId")] = deterministicPolicyId(m_ownerId, key);
    p[QStringLiteral("timeZoneId")] = QStringLiteral("UTC");
    p[QStringLiteral("valueClass")] = QStringLiteral("actual");
    const int warning = p.value(QStringLiteral("warningPercent"), 80).toInt();
    p[QStringLiteral("criticalPercent")] = qMin(100, qMax(90, warning + 10));
    QString error;
    p = normalizedPolicy(p, true, &error);
    if (!error.isEmpty()) {
      setError(error);
      return false;
    }
    pending.append({key, p});
  }
  if (!m_database.transaction()) {
    setError(m_database.lastError().text());
    return false;
  }
  for (const Pending &item : pending) {
    if (!writePolicy(item.policy, false)) {
      m_database.rollback();
      reload();
      return false;
    }
    QSqlQuery marker(m_database);
    marker.prepare(
        QStringLiteral("INSERT INTO "
                       "budget_policy_migrations(owner_id,legacy_key,policy_id,"
                       "migrated_at_utc) VALUES(?,?,?,?)"));
    marker.addBindValue(m_ownerId);
    marker.addBindValue(item.key);
    marker.addBindValue(item.policy.value(QStringLiteral("policyId")));
    marker.addBindValue(
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!marker.exec()) {
      setError(marker.lastError().text());
      m_database.rollback();
      reload();
      return false;
    }
  }
  if (!m_database.commit()) {
    setError(m_database.lastError().text());
    m_database.rollback();
    return false;
  }
  reload();
  return true;
}

QVariantList BudgetPolicyRepository::exportPolicies() const {
  return m_policies;
}
