#ifndef SCOPEBREAKDOWNQUERY_H
#define SCOPEBREAKDOWNQUERY_H

#include <QVariantList>
#include <QVariantMap>

/**
 * Normalizes and reconciles provider-reported scope dimensions.
 *
 * Aggregate and scoped rows remain separate. The query never invents a
 * dimension and never combines actual and estimated values or currencies.
 */
class ScopeBreakdownQuery final {
public:
    static QVariantMap run(const QVariantList &metrics);
    static QVariantMap annotateMetric(const QVariantMap &metric);
    static bool isScopedMetric(const QVariantMap &metric);
};

#endif // SCOPEBREAKDOWNQUERY_H
