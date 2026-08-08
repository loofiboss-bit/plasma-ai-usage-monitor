#ifndef BUDGETPOLICYMODEL_H
#define BUDGETPOLICYMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

#include "budgetpolicyrepository.h"

class BudgetPolicyModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(BudgetPolicyRepository *repository READ repository WRITE
                 setRepository NOTIFY repositoryChanged)

public:
  enum Role {
    PolicyRole = Qt::UserRole + 1,
    PolicyIdRole,
    SourceIdRole,
    EnabledRole,
    RiskScopeRole
  };
  Q_ENUM(Role)

  explicit BudgetPolicyModel(QObject *parent = nullptr);
  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  BudgetPolicyRepository *repository() const;
  void setRepository(BudgetPolicyRepository *repository);

Q_SIGNALS:
  void repositoryChanged();

private Q_SLOTS:
  void reload();

private:
  BudgetPolicyRepository *m_repository = nullptr;
  QVariantList m_rows;
};

#endif
