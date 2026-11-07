#include "sessionquery.h"
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

namespace SessionQuery {
QString eventWhere() {
  return QStringLiteral(" (?='' OR s.tool_id=?) AND e.observed_at_utc>=? AND "
                        "e.observed_at_utc<? AND (?='' OR s.project_id=?) "
                        "AND (?=0 OR e.model_id=?) ");
}
void bindEventFilter(QSqlQuery &query, const QString &tool,
                     const QDateTime &from, const QDateTime &to,
                     const QString &project, const QVariantMap &filter) {
  const QString toolFilter =
      (tool.isEmpty() || tool == QLatin1String("all")) ? QStringLiteral("")
                                                       : tool;
  query.addBindValue(toolFilter);
  query.addBindValue(toolFilter);
  query.addBindValue(from.toUTC().toString(Qt::ISODateWithMs));
  query.addBindValue(to.toUTC().toString(Qt::ISODateWithMs));
  query.addBindValue(project.isEmpty() ? QStringLiteral("") : project);
  query.addBindValue(project.isEmpty() ? QStringLiteral("") : project);
  query.addBindValue(filter.contains("modelId") ? 1 : 0);
  query.addBindValue(filter.value("modelId", QStringLiteral("")).toString());
}
// Aggregate recorded observations only. SQL NULL is deliberately preserved:
// missing usage is not evidence of zero usage.
const QStringList sessionTokenMetrics{"inputTokens", "outputTokens",
                                      "cacheReadTokens", "cacheWriteTokens",
                                      "totalTokens"};
const QStringList sessionTokenExpressions{
    "e.input_tokens", "e.output_tokens", "e.cache_read_tokens",
    "e.cache_write_tokens", "(e.input_tokens+e.output_tokens)"};
QString aggregateProjection() {
  QString projection =
      QStringLiteral("COUNT(DISTINCT s.session_id),COUNT(e.event_id),"
                     "COUNT(DISTINCT s.project_id),MAX(e.observed_at_utc),"
                     "SUM(CASE WHEN s.coverage!='reported' THEN 1 ELSE 0 END)");
  for (int i = 0; i < sessionTokenExpressions.size(); ++i) {
    const auto &expression = sessionTokenExpressions[i];
    // Keep the aggregate identity total = input + output, including known
    // observations from events with a missing counterpart. Paired-event
    // coverage below prevents treating that partial aggregate as complete.
    if (sessionTokenMetrics[i] == QLatin1String("totalTokens"))
      projection +=
          QStringLiteral(",SUM(e.input_tokens)+SUM(e.output_tokens),COUNT(%1)")
              .arg(expression);
    else
      projection += QStringLiteral(",SUM(%1),COUNT(%1)").arg(expression);
  }
  return projection;
}
QVariantMap aggregateRow(const QSqlQuery &query, int offset,
                         const QDateTime &from, const QDateTime &to,
                         bool partial) {
  const qlonglong events = query.value(offset + 1).toLongLong();
  const QString coverage = !events ? QStringLiteral("unavailable")
                           : partial || query.value(offset + 4).toLongLong() > 0
                               ? QStringLiteral("partial")
                               : QStringLiteral("reported");
  QVariantMap result{{"sessionCount", query.value(offset)},
                     {"eventCount", query.value(offset + 1)},
                     {"projectCount", query.value(offset + 2)},
                     {"lastActivityAtUtc", query.value(offset + 3)},
                     {"coverage", coverage},
                     {"fromUtc", from.toUTC().toString(Qt::ISODateWithMs)},
                     {"toUtc", to.toUTC().toString(Qt::ISODateWithMs)}};
  QVariantMap categories;
  for (int i = 0; i < sessionTokenMetrics.size(); ++i) {
    const auto reported = query.value(offset + 6 + 2 * i).toLongLong();
    result.insert(sessionTokenMetrics[i], query.value(offset + 5 + 2 * i));
    categories.insert(
        sessionTokenMetrics[i],
        QVariantMap{{"reportedEventCount", reported},
                    {"missingEventCount", events - reported},
                    {"coverage", !reported ? QStringLiteral("unavailable")
                                 : reported == events
                                     ? QStringLiteral("reported")
                                     : QStringLiteral("partial")}});
  }
  result.insert("categoryCoverage", categories);
  return result;
}
bool aggregateSessions(QSqlDatabase &db, const QString &tool,
                       const QDateTime &from, const QDateTime &to,
                       const QString &project, const QVariantMap &filter,
                       bool enabled, const QDateTime &retentionCutoff,
                       QVariantMap &summary, QMap<QString, QVariantMap> *models,
                       QMap<QString, QVariantMap> *projects,
                       QMap<QString, QVariantMap> *tools) {
  QSqlQuery checkpoint(db);
  const bool allTools = tool.isEmpty() || tool == QLatin1String("all");
  if (allTools) {
    checkpoint.prepare(QStringLiteral(
        "SELECT coverage,cutoff_utc FROM local_import_checkpoints"));
  } else {
    checkpoint.prepare(QStringLiteral(
        "SELECT coverage,cutoff_utc FROM local_import_checkpoints "
        "WHERE tool_id=?"));
    checkpoint.addBindValue(tool);
  }
  if (!checkpoint.exec())
    return false;
  QString toolCoverage = QStringLiteral("unknown");
  QDateTime clearCutoff;
  while (checkpoint.next()) {
    const QString cov = checkpoint.value(0).toString();
    if (cov == QLatin1String("partial") ||
        toolCoverage == QLatin1String("unknown"))
      toolCoverage = cov;
    const QDateTime cut = QDateTime::fromString(checkpoint.value(1).toString(),
                                                Qt::ISODateWithMs);
    if (cut.isValid() && (!clearCutoff.isValid() || cut > clearCutoff))
      clearCutoff = cut;
  }
  const bool partial = toolCoverage == QLatin1String("partial") ||
                       from < retentionCutoff ||
                       (clearCutoff.isValid() && from < clearCutoff);
  const QString base =
      QStringLiteral(" FROM local_sessions s JOIN local_usage_events e ON "
                     "e.session_id=s.session_id WHERE ") +
      eventWhere();
  QSqlQuery query(db);
  query.prepare(QStringLiteral("SELECT ") + aggregateProjection() + base);
  bindEventFilter(query, tool, from, to, project, filter);
  if (!query.exec() || !query.next())
    return false;
  summary = aggregateRow(query, 0, from, to, partial);
  summary.insert("toolId", allTools ? QStringLiteral("all") : tool);
  summary.insert("toolCoverage", toolCoverage);
  summary.insert("toolEnabled", enabled);
  summary.insert("importStatus",
                 enabled ? toolCoverage : QStringLiteral("disabled"));
  const auto grouped = [&](const QString &column,
                           QMap<QString, QVariantMap> *out,
                           bool normalizeToolKey = false) {
    if (!out)
      return true;
    QSqlQuery rows(db);
    rows.prepare(QStringLiteral("SELECT ") + column + QLatin1Char(',') +
                 aggregateProjection() + base + QStringLiteral(" GROUP BY ") +
                 column + QStringLiteral(" ORDER BY ") + column);
    bindEventFilter(rows, tool, from, to, project, filter);
    if (!rows.exec())
      return false;
    while (rows.next()) {
      QString key = rows.value(0).toString();
      if (normalizeToolKey && key == QLatin1String("codex"))
        key = QStringLiteral("codex-cli");
      out->insert(key, aggregateRow(rows, 1, from, to, partial));
    }
    return !rows.lastError().isValid();
  };
  return grouped(QStringLiteral("e.model_id"), models) &&
         grouped(QStringLiteral("s.project_id"), projects) &&
         grouped(QStringLiteral("s.tool_id"), tools, true);
}
QVariantMap sessionDeltas(const QVariantMap &current,
                          const QVariantMap &previous) {
  QVariantMap result;
  QStringList metrics{"sessionCount", "eventCount"};
  metrics.append(sessionTokenMetrics);
  for (const auto &metric : metrics) {
    QString reason;
    if (current.value("eventCount").toLongLong() == 0 ||
        previous.value("eventCount").toLongLong() == 0)
      reason = QStringLiteral("no-observations");
    else if (current.value("coverage") != QStringLiteral("reported") ||
             previous.value("coverage") != QStringLiteral("reported"))
      reason = QStringLiteral("partial-coverage");
    else if (sessionTokenMetrics.contains(metric) &&
             (current.value("categoryCoverage")
                      .toMap()
                      .value(metric)
                      .toMap()
                      .value("coverage") != QStringLiteral("reported") ||
              previous.value("categoryCoverage")
                      .toMap()
                      .value(metric)
                      .toMap()
                      .value("coverage") != QStringLiteral("reported")))
      reason = QStringLiteral("missing-category");
    const bool available = reason.isEmpty();
    const qlonglong baseline = previous.value(metric).toLongLong();
    const qlonglong absolute = current.value(metric).toLongLong() - baseline;
    const bool percentAvailable = available && baseline != 0;
    result.insert(
        metric,
        QVariantMap{{"available", available},
                    {"absolute", available ? QVariant(absolute) : QVariant()},
                    {"percentAvailable", percentAvailable},
                    {"percent",
                     percentAvailable
                         ? QVariant(100.0 * double(absolute) / double(baseline))
                         : QVariant()},
                    {"reason", available && !percentAvailable
                                   ? QStringLiteral("zero-baseline")
                                   : reason}});
  }
  return result;
}
QVariantList comparisonRows(const QMap<QString, QVariantMap> &current,
                            const QMap<QString, QVariantMap> &previous,
                            const QString &idKey,
                            const QVariantMap &currentTotals,
                            const QVariantMap &previousTotals,
                            int *totalCount) {
  QMap<QString, bool> ids;
  for (auto it = current.cbegin(); it != current.cend(); ++it)
    ids.insert(it.key(), true);
  for (auto it = previous.cbegin(); it != previous.cend(); ++it)
    ids.insert(it.key(), true);
  if (totalCount)
    *totalCount = int(ids.size());
  const auto absent = [](const QVariantMap &totals) {
    QVariantMap row{{"sessionCount", 0},
                    {"eventCount", 0},
                    {"coverage", QStringLiteral("unavailable")},
                    {"fromUtc", totals.value("fromUtc")},
                    {"toUtc", totals.value("toUtc")}};
    QVariantMap categories;
    for (const auto &metric : sessionTokenMetrics) {
      row.insert(metric, QVariant());
      categories.insert(
          metric, QVariantMap{{"reportedEventCount", 0},
                              {"missingEventCount", 0},
                              {"coverage", QStringLiteral("unavailable")}});
    }
    row.insert("categoryCoverage", categories);
    return row;
  };
  QVariantList rows;
  const qlonglong grandTotal = currentTotals.value("totalTokens").toLongLong();
  const bool grandTotalComplete =
      currentTotals.value("coverage") == QStringLiteral("reported") &&
      grandTotal > 0;
  for (auto it = ids.cbegin(); it != ids.cend(); ++it) {
    if (rows.size() >= 100)
      break;
    const auto now = current.value(it.key(), absent(currentTotals));
    const auto before = previous.value(it.key(), absent(previousTotals));
    QVariantMap row{{idKey, it.key()},
                    {"current", now},
                    {"previous", before},
                    {"deltas", sessionDeltas(now, before)}};
    if (grandTotalComplete &&
        now.value("coverage") == QStringLiteral("reported") &&
        now.value("categoryCoverage")
                .toMap()
                .value("totalTokens")
                .toMap()
                .value("coverage") == QStringLiteral("reported")) {
      row.insert("sharePercent",
                 100.0 * now.value("totalTokens").toDouble() /
                     double(grandTotal));
    }
    const QVariantMap nowCats = now.value("categoryCoverage").toMap();
    if (nowCats.value("cacheReadTokens").toMap().value("coverage") ==
            QStringLiteral("reported") &&
        nowCats.value("inputTokens").toMap().value("coverage") ==
            QStringLiteral("reported") &&
        now.value("inputTokens").toLongLong() > 0) {
      row.insert("cacheHitPercent",
                 100.0 * now.value("cacheReadTokens").toDouble() /
                     now.value("inputTokens").toDouble());
    }
    rows.append(row);
  }
  return rows;
}

QVariantList insights(const QVariantMap &current, const QVariantMap &previous,
                      const QMap<QString, QVariantMap> &models,
                      const QMap<QString, QVariantMap> &projects,
                      const QMap<QString, QVariantMap> &tools) {
  QVariantList result;
  const auto complete = [](const QVariantMap &totals, const QString &metric) {
    return totals.value("coverage") == QStringLiteral("reported") &&
           totals.value("categoryCoverage")
                   .toMap()
                   .value(metric)
                   .toMap()
                   .value("coverage") == QStringLiteral("reported");
  };
  const auto append = [&](const QString &key, const QString &metric,
                          const QVariant &value, const QString &reason,
                          QVariantMap extra = {}) {
    extra.insert("key", key);
    extra.insert("metric", metric);
    extra.insert("value", value);
    extra.insert("reason", reason);
    extra.insert("period", QStringLiteral("current"));
    extra.insert("coverage", current.value("coverage"));
    result.append(extra);
  };
  append(QStringLiteral("observation-coverage"), QStringLiteral("eventCount"),
         current.value("eventCount"), current.value("coverage").toString());
  const auto totalDelta =
      sessionDeltas(current, previous).value("totalTokens").toMap();
  if (totalDelta.value("available").toBool())
    append(QStringLiteral("total-tokens-change"), QStringLiteral("totalTokens"),
           totalDelta.value("absolute"), totalDelta.value("reason").toString(),
           {{"previousValue", previous.value("totalTokens")},
            {"percentAvailable", totalDelta.value("percentAvailable")},
            {"percent", totalDelta.value("percent")}});
  const auto rank = [&](const QMap<QString, QVariantMap> &groups,
                        const QString &key, const QString &idKey) {
    if (!complete(current, QStringLiteral("totalTokens")))
      return;
    QStringList ids;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it)
      if (complete(it.value(), QStringLiteral("totalTokens")))
        ids.append(it.key());
    std::sort(ids.begin(), ids.end(), [&](const QString &a, const QString &b) {
      const auto left = groups.value(a).value("totalTokens").toLongLong();
      const auto right = groups.value(b).value("totalTokens").toLongLong();
      return left == right ? a < b : left > right;
    });
    const auto total = current.value("totalTokens").toLongLong();
    for (int i = 0; i < qMin(3, ids.size()); ++i) {
      const auto value = groups.value(ids[i]).value("totalTokens");
      QVariantMap extra{{idKey, ids[i]}, {"rank", i + 1}};
      if (total > 0)
        extra.insert("sharePercent", 100.0 * value.toDouble() / double(total));
      append(key, QStringLiteral("totalTokens"), value, {}, extra);
    }
  };
  if (!tools.isEmpty())
    rank(tools, QStringLiteral("top-tool"), QStringLiteral("toolId"));
  rank(models, QStringLiteral("top-model"), QStringLiteral("modelId"));
  rank(projects, QStringLiteral("top-project"), QStringLiteral("projectId"));
  const auto share = [&](const QString &key, const QString &numerator,
                         const QString &denominator) {
    if (!complete(current, numerator) || !complete(current, denominator) ||
        current.value(denominator).toLongLong() <= 0)
      return;
    append(key, numerator,
           100.0 * current.value(numerator).toDouble() /
               current.value(denominator).toDouble(),
           {},
           {{"numerator", current.value(numerator)},
            {"denominator", current.value(denominator)},
            {"denominatorMetric", denominator}});
  };
  share(QStringLiteral("input-share"), QStringLiteral("inputTokens"),
        QStringLiteral("totalTokens"));
  share(QStringLiteral("output-share"), QStringLiteral("outputTokens"),
        QStringLiteral("totalTokens"));
  share(QStringLiteral("cache-read-share"), QStringLiteral("cacheReadTokens"),
        QStringLiteral("inputTokens"));
  return result;
}

} // namespace SessionQuery
