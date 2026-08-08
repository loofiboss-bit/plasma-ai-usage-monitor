#ifndef BUDGETPOLICYSCHEMA_H
#define BUDGETPOLICYSCHEMA_H

#include <QSqlDatabase>
#include <QString>

namespace BudgetPolicySchema {
constexpr int Version = 6;

bool migrate(QSqlDatabase &database, QString *error = nullptr,
             bool injectFailure = false);
} // namespace BudgetPolicySchema

#endif
