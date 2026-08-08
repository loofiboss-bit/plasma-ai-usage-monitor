#ifndef BUDGETPOLICYREPOSITORY_H
#define BUDGETPOLICYREPOSITORY_H

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class BudgetPolicyRepository : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(
      QString ownerId READ ownerId WRITE setOwnerId NOTIFY ownerIdChanged)
  Q_PROPERTY(QString databasePath READ databasePath WRITE setDatabasePath NOTIFY
                 databasePathChanged)
  Q_PROPERTY(QVariantList policies READ policies NOTIFY policiesChanged)
  Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
  explicit BudgetPolicyRepository(QObject *parent = nullptr);
  ~BudgetPolicyRepository() override;

  QString ownerId() const;
  void setOwnerId(const QString &ownerId);
  QString databasePath() const;
  void setDatabasePath(const QString &databasePath);
  QVariantList policies() const;
  qulonglong revision() const;
  QString errorString() const;

  Q_INVOKABLE bool init();
  Q_INVOKABLE QVariantMap createPolicy(const QVariantMap &policy);
  Q_INVOKABLE QVariantMap updatePolicy(const QString &policyId,
                                       const QVariantMap &changes);
  Q_INVOKABLE QVariantMap duplicatePolicy(const QString &policyId);
  Q_INVOKABLE bool deletePolicy(const QString &policyId);
  Q_INVOKABLE bool setPolicyEnabled(const QString &policyId, bool enabled);
  Q_INVOKABLE bool snoozePolicy(const QString &policyId,
                                const QDateTime &untilUtc);
  Q_INVOKABLE bool replacePolicies(const QVariantList &policies);
  Q_INVOKABLE bool migrateLegacyBudgets(const QVariantList &legacyBudgets);
  Q_INVOKABLE QVariantMap validatePolicy(const QVariantMap &policy) const;
  Q_INVOKABLE QVariantList exportPolicies() const;

Q_SIGNALS:
  void ownerIdChanged();
  void databasePathChanged();
  void policiesChanged();
  void revisionChanged();
  void errorStringChanged();

private:
  bool ensureOpen();
  void reload();
  void setError(const QString &error);
  QVariantMap normalizedPolicy(const QVariantMap &policy, bool newPolicy,
                               QString *error) const;
  QVariantMap policyById(const QString &policyId) const;
  bool writePolicy(const QVariantMap &policy, bool update);
  static QString deterministicPolicyId(const QString &ownerId,
                                       const QString &legacyKey);

  QString m_ownerId;
  QString m_databasePath;
  QString m_connectionName;
  QSqlDatabase m_database;
  QVariantList m_policies;
  qulonglong m_revision = 0;
  QString m_errorString;
};

#endif
