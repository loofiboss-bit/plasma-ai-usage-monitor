#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QVariantMap>

class AnalystQuery final {
public:
  static QVariantMap execute(const QSqlDatabase &database, bool initialized,
                             const QDateTime &fromInclusive,
                             const QDateTime &toExclusive,
                             const QString &requestedCurrency = {});
};
