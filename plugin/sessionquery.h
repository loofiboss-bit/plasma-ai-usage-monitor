#ifndef SESSIONQUERY_H
#define SESSIONQUERY_H

#include <QDateTime>
#include <QMap>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>
class QSqlQuery;

// Read-only projections. The caller owns a read transaction so period totals,
// category coverage and breakdowns all observe the same SQLite snapshot.
namespace SessionQuery {
QString eventWhere();
void bindEventFilter(QSqlQuery &query, const QString &tool,
                     const QDateTime &from, const QDateTime &to,
                     const QString &project, const QVariantMap &filter);
bool aggregateSessions(QSqlDatabase &db, const QString &tool,
                       const QDateTime &from, const QDateTime &to,
                       const QString &project, const QVariantMap &filter,
                       bool enabled, const QDateTime &retentionCutoff,
                       QVariantMap &summary,
                       QMap<QString, QVariantMap> *models = nullptr,
                       QMap<QString, QVariantMap> *projects = nullptr,
                       QMap<QString, QVariantMap> *tools = nullptr);
QVariantMap sessionDeltas(const QVariantMap &current,
                          const QVariantMap &previous);
QVariantList comparisonRows(const QMap<QString, QVariantMap> &current,
                            const QMap<QString, QVariantMap> &previous,
                            const QString &idKey,
                            const QVariantMap &currentTotals,
                            const QVariantMap &previousTotals,
                            int *totalCount = nullptr);
QVariantList insights(const QVariantMap &current, const QVariantMap &previous,
                      const QMap<QString, QVariantMap> &models,
                      const QMap<QString, QVariantMap> &projects,
                      const QMap<QString, QVariantMap> &tools = {});
} // namespace SessionQuery
#endif
