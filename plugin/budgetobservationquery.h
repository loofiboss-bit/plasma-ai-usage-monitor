#ifndef BUDGETOBSERVATIONQUERY_H
#define BUDGETOBSERVATIONQUERY_H

#include "budgetpacingquery.h"

#include <QSqlDatabase>
#include <QVariantMap>

class BudgetObservationQuery final {
public:
  static BudgetPacingQuery::Request
  requestFromPolicy(const QVariantMap &policy, const QDateTime &generatedAt,
                    const QSqlDatabase &database);
};

#endif
