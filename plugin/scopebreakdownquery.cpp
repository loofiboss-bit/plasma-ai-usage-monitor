#include "scopebreakdownquery.h"

#include <QDateTime>
#include <QMap>
#include <QSet>

#include <cmath>

namespace {
bool finiteAvailableValue(const QVariantMap &metric, double *value)
{
    const QVariant raw = metric.value(QStringLiteral("value"));
    bool ok = false;
    const double number = raw.toDouble(&ok);
    if (!metric.value(QStringLiteral("available")).toBool() || !raw.isValid() || raw.isNull() || !ok
        || !std::isfinite(number)) {
        return false;
    }
    *value = number;
    return true;
}

QString dimensionFromScope(const QString &scope, const QString &marker)
{
    const QString prefix = QStringLiteral("organization_scoped:") + marker + QLatin1Char(':');
    return scope.startsWith(prefix) ? scope.mid(prefix.size()) : QString();
}

QString valueClass(const QVariantMap &metric)
{
    const QString source = metric.value(QStringLiteral("source")).toString().toLower();
    const QString quality = metric.value(QStringLiteral("quality")).toString().toLower();
    static const QSet<QString> estimatedSources {
        QStringLiteral("estimated_pricing"),
        QStringLiteral("estimated_from_usage"),
        QStringLiteral("local_observation"),
        QStringLiteral("self_tracked"),
    };
    return estimatedSources.contains(source) || quality == QLatin1String("estimated")
            || quality == QLatin1String("local_estimate")
        ? QStringLiteral("estimated")
        : QStringLiteral("actual");
}

QString localDisplayKind(const QString &identifier)
{
    if (identifier.trimmed().isEmpty()) {
        return QStringLiteral("unattributed");
    }
    const QString lowered = identifier.trimmed().toLower();
    if (lowered == QLatin1String("deleted") || lowered.startsWith(QLatin1String("deleted_"))) {
        return QStringLiteral("deleted");
    }
    return QStringLiteral("opaque");
}

QString localDisplaySuffix(const QString &identifier)
{
    const QString trimmed = identifier.trimmed();
    return trimmed.isEmpty() ? QString() : trimmed.right(8);
}

QString dateTimeKey(const QVariant &value)
{
    const QDateTime dateTime = value.toDateTime();
    return dateTime.isValid() ? dateTime.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QString compatibilityKey(const QVariantMap &metric)
{
    return QStringList {
        metric.value(QStringLiteral("kind")).toString(),
        metric.value(QStringLiteral("unit")).toString(),
        metric.value(QStringLiteral("currency")).toString().toUpper(),
        metric.value(QStringLiteral("source")).toString(),
        metric.value(QStringLiteral("quality")).toString(),
        metric.value(QStringLiteral("window")).toString(),
        dateTimeKey(metric.value(QStringLiteral("periodStart"))),
        dateTimeKey(metric.value(QStringLiteral("periodEnd"))),
        metric.value(QStringLiteral("valueClass")).toString(),
    }
        .join(QChar(0x1f));
}

QString scopedRowKey(const QVariantMap &metric)
{
    return QStringList {
        compatibilityKey(metric),
        metric.value(QStringLiteral("aggregationLevel")).toString(),
        metric.value(QStringLiteral("scope")).toString(),
        metric.value(QStringLiteral("modelScope")).toString(),
        metric.value(QStringLiteral("projectScope")).toString(),
        metric.value(QStringLiteral("serviceTierScope")).toString(),
        metric.value(QStringLiteral("lineItemScope")).toString(),
    }
        .join(QChar(0x1f));
}
} // namespace

bool ScopeBreakdownQuery::isScopedMetric(const QVariantMap &metric)
{
    const QString explicitLevel = metric.value(QStringLiteral("aggregationLevel")).toString();
    if (explicitLevel == QLatin1String("scoped")) {
        return true;
    }
    if (explicitLevel == QLatin1String("aggregate")) {
        return false;
    }
    return !metric.value(QStringLiteral("modelScope")).toString().isEmpty()
        || !metric.value(QStringLiteral("projectScope")).toString().isEmpty()
        || metric.value(QStringLiteral("scope")).toString().startsWith(QLatin1String("organization_scoped"));
}

QVariantMap ScopeBreakdownQuery::annotateMetric(const QVariantMap &metric)
{
    QVariantMap result = metric;
    const QString model = metric.value(QStringLiteral("modelScope")).toString().trimmed();
    const QString project = metric.value(QStringLiteral("projectScope")).toString().trimmed();
    const QString scope = metric.value(QStringLiteral("scope")).toString();
    const QString serviceTier = dimensionFromScope(scope, QStringLiteral("service_tier"));
    const QString lineItem = dimensionFromScope(scope, QStringLiteral("line_item"));

    result.insert(QStringLiteral("modelScope"), model);
    result.insert(QStringLiteral("modelScopeAvailable"), !model.isEmpty());
    result.insert(QStringLiteral("projectScope"), project);
    result.insert(QStringLiteral("projectScopeAvailable"), !project.isEmpty());
    result.insert(QStringLiteral("projectDisplayKind"), localDisplayKind(project));
    result.insert(QStringLiteral("projectDisplaySuffix"), localDisplaySuffix(project));
    result.insert(QStringLiteral("serviceTierScope"), serviceTier);
    result.insert(QStringLiteral("serviceTierAvailable"), !serviceTier.isEmpty());
    result.insert(QStringLiteral("lineItemScope"), lineItem);
    result.insert(QStringLiteral("lineItemAvailable"), !lineItem.isEmpty());
    result.insert(QStringLiteral("aggregationLevel"),
        isScopedMetric(metric) ? QStringLiteral("scoped") : QStringLiteral("aggregate"));
    result.insert(QStringLiteral("valueClass"), valueClass(metric));
    return result;
}

QVariantMap ScopeBreakdownQuery::run(const QVariantList &metrics)
{
    QVariantList annotatedRows;
    QMap<QString, QVariantMap> groupedRows;
    for (const QVariant &entry : metrics) {
        QVariantMap metric = annotateMetric(entry.toMap());
        annotatedRows.append(metric);
        double value = 0.0;
        if (!finiteAvailableValue(metric, &value)) {
            continue;
        }
        const QString key = scopedRowKey(metric);
        if (groupedRows.contains(key)) {
            QVariantMap existing = groupedRows.value(key);
            existing.insert(QStringLiteral("value"), existing.value(QStringLiteral("value")).toDouble() + value);
            existing.insert(QStringLiteral("contributingRowCount"),
                existing.value(QStringLiteral("contributingRowCount")).toInt() + 1);
            groupedRows.insert(key, existing);
        } else {
            metric.insert(QStringLiteral("contributingRowCount"), 1);
            groupedRows.insert(key, metric);
        }
    }

    QVariantList aggregateRows;
    QVariantList scopedRows;
    struct Reconciliation {
        double aggregateValue = 0.0;
        double scopedValue = 0.0;
        int aggregateCount = 0;
        int scopedCount = 0;
        QVariantMap identity;
    };
    QMap<QString, Reconciliation> reconciliations;
    for (const QVariantMap &row : std::as_const(groupedRows)) {
        const bool scoped = row.value(QStringLiteral("aggregationLevel")) == QLatin1String("scoped");
        if (scoped) {
            scopedRows.append(row);
        } else {
            aggregateRows.append(row);
        }
        const QString key = compatibilityKey(row);
        Reconciliation &reconciliation = reconciliations[key];
        reconciliation.identity = {
            { QStringLiteral("kind"), row.value(QStringLiteral("kind")) },
            { QStringLiteral("unit"), row.value(QStringLiteral("unit")) },
            { QStringLiteral("currency"), row.value(QStringLiteral("currency")) },
            { QStringLiteral("window"), row.value(QStringLiteral("window")) },
            { QStringLiteral("periodStart"), row.value(QStringLiteral("periodStart")) },
            { QStringLiteral("periodEnd"), row.value(QStringLiteral("periodEnd")) },
            { QStringLiteral("valueClass"), row.value(QStringLiteral("valueClass")) },
        };
        if (scoped) {
            reconciliation.scopedValue += row.value(QStringLiteral("value")).toDouble();
            ++reconciliation.scopedCount;
        } else {
            reconciliation.aggregateValue += row.value(QStringLiteral("value")).toDouble();
            ++reconciliation.aggregateCount;
        }
    }

    QVariantList reconciliationRows;
    for (const Reconciliation &item : std::as_const(reconciliations)) {
        if (item.aggregateCount == 0 || item.scopedCount == 0) {
            continue;
        }
        QVariantMap row = item.identity;
        row.insert(QStringLiteral("aggregateValue"), item.aggregateValue);
        row.insert(QStringLiteral("scopedValue"), item.scopedValue);
        row.insert(QStringLiteral("aggregateRowCount"), item.aggregateCount);
        row.insert(QStringLiteral("scopedRowCount"), item.scopedCount);
        const double scale = qMax(1.0, qMax(std::abs(item.aggregateValue), std::abs(item.scopedValue)));
        const bool exact = item.aggregateCount == 1 && std::abs(item.aggregateValue - item.scopedValue) <= scale * 1e-9;
        row.insert(QStringLiteral("reconciled"), exact);
        row.insert(QStringLiteral("status"),
            item.aggregateCount != 1 ? QStringLiteral("ambiguous_aggregate")
                : exact              ? QStringLiteral("exact")
                                     : QStringLiteral("mismatch"));
        reconciliationRows.append(row);
    }

    return {
        { QStringLiteral("rows"), annotatedRows },
        { QStringLiteral("aggregateRows"), aggregateRows },
        { QStringLiteral("scopedRows"), scopedRows },
        { QStringLiteral("reconciliations"), reconciliationRows },
        { QStringLiteral("hasScopes"), !scopedRows.isEmpty() },
    };
}
