#include "budgetpolicymodel.h"

#include "budgetpolicyrepository.h"

BudgetPolicyModel::BudgetPolicyModel(QObject *parent)
    : QAbstractListModel(parent) {}

int BudgetPolicyModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_rows.size();
}

QVariant BudgetPolicyModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
    return {};
  const QVariantMap row = m_rows.at(index.row()).toMap();
  switch (role) {
  case PolicyRole:
    return row;
  case PolicyIdRole:
    return row.value(QStringLiteral("policyId"));
  case SourceIdRole:
    return row.value(QStringLiteral("sourceId"));
  case EnabledRole:
    return row.value(QStringLiteral("enabled"));
  case RiskScopeRole:
    return row.value(QStringLiteral("scopeMode"));
  default:
    return {};
  }
}

QHash<int, QByteArray> BudgetPolicyModel::roleNames() const {
  return {{PolicyRole, "policy"},
          {PolicyIdRole, "policyId"},
          {SourceIdRole, "sourceId"},
          {EnabledRole, "enabled"},
          {RiskScopeRole, "scopeMode"}};
}

BudgetPolicyRepository *BudgetPolicyModel::repository() const {
  return m_repository;
}

void BudgetPolicyModel::setRepository(BudgetPolicyRepository *repository) {
  if (m_repository == repository)
    return;
  if (m_repository)
    disconnect(m_repository, nullptr, this, nullptr);
  m_repository = repository;
  if (m_repository)
    connect(m_repository, &BudgetPolicyRepository::policiesChanged, this,
            &BudgetPolicyModel::reload);
  reload();
  Q_EMIT repositoryChanged();
}

void BudgetPolicyModel::reload() {
  beginResetModel();
  m_rows = m_repository ? m_repository->policies() : QVariantList{};
  endResetModel();
}
