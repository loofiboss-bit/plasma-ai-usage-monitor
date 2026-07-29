#include "sourcedetailmodel.h"

#include "dailystatemodel.h"
#include "scopebreakdownquery.h"
#include "usagedatabase.h"

#include <KLocalizedString>
#include <QDateTime>
#include <QSet>
#include <QUuid>
#include <utility>

namespace {
const QStringList kRoleNames{
    QStringLiteral("kind"),     QStringLiteral("available"),
    QStringLiteral("value"),    QStringLiteral("unit"),
    QStringLiteral("currency"), QStringLiteral("source"),
    QStringLiteral("quality"),  QStringLiteral("semantic"),
    QStringLiteral("scope"),    QStringLiteral("window"),
    QStringLiteral("resetAt"),
    QStringLiteral("modelScope"),
    QStringLiteral("modelScopeAvailable"),
    QStringLiteral("projectScope"),
    QStringLiteral("projectScopeAvailable"),
    QStringLiteral("projectDisplayKind"),
    QStringLiteral("projectDisplaySuffix"),
    QStringLiteral("serviceTierScope"),
    QStringLiteral("serviceTierAvailable"),
    QStringLiteral("lineItemScope"),
    QStringLiteral("lineItemAvailable"),
    QStringLiteral("aggregationLevel"),
    QStringLiteral("valueClass")};

bool availableMetric(const QVariantMap &metric) {
  const QVariant value = metric.value(QStringLiteral("value"));
  return metric.value(QStringLiteral("available")).toBool() &&
         value.isValid() && !value.isNull();
}

bool actualMetric(const QString &source, const QString &quality) {
  static const QSet<QString> actualSources{
      QStringLiteral("billing_api"),
      QStringLiteral("usage_api"),
      QStringLiteral("actual_api"),
      QStringLiteral("metrics_api"),
      QStringLiteral("response_headers"),
      QStringLiteral("browser_sync"),
      QStringLiteral("antigravity_local"),
      QStringLiteral("local_daemon_actual")};
  return quality == QLatin1String("actual") || actualSources.contains(source);
}

bool estimatedMetric(const QString &source, const QString &quality) {
  static const QSet<QString> estimatedSources{
      QStringLiteral("estimated_pricing"),
      QStringLiteral("estimated_from_usage"),
      QStringLiteral("local_observation"), QStringLiteral("self_tracked")};
  return quality == QLatin1String("estimated") ||
         quality == QLatin1String("local_estimate") ||
         estimatedSources.contains(source);
}
} // namespace

SourceDetailModel::SourceDetailModel(QObject *parent)
    : QAbstractListModel(parent) {}

int SourceDetailModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_metrics.size();
}

QVariant SourceDetailModel::data(const QModelIndex &index, int role) const {
  const int offset = role - KindRole;
  if (!index.isValid() || index.row() < 0 || index.row() >= m_metrics.size() ||
      offset < 0 || offset >= kRoleNames.size())
    return {};
  return m_metrics.at(index.row()).toMap().value(kRoleNames.at(offset));
}

QHash<int, QByteArray> SourceDetailModel::roleNames() const {
  QHash<int, QByteArray> result;
  for (int i = 0; i < kRoleNames.size(); ++i)
    result.insert(KindRole + i, kRoleNames.at(i).toUtf8());
  return result;
}

QString SourceDetailModel::sourceId() const { return m_sourceId; }

void SourceDetailModel::setSourceId(const QString &sourceId) {
  if (m_sourceId == sourceId)
    return;
  m_sourceId = sourceId;
  rebuild();
}

QVariantMap SourceDetailModel::source() const { return m_source; }
QString SourceDetailModel::actionLabel() const {
  return actionLabelFor(m_source);
}
QVariantList SourceDetailModel::quotaWindows() const {
  return m_source.value(QStringLiteral("quotaWindows")).toList();
}
QVariantMap SourceDetailModel::coverage() const { return m_coverage; }
QVariantMap SourceDetailModel::scopeBreakdown() const {
  return m_scopeBreakdown;
}
QVariantMap SourceDetailModel::recentHistory() const { return m_recentHistory; }
bool SourceDetailModel::historyLoading() const { return m_historyLoading; }
QString SourceDetailModel::historyId() const {
  const QString kind =
      m_source.value(QStringLiteral("sourceKind")).toString() ==
              QLatin1String("local_tool")
          ? QStringLiteral("tool")
          : QStringLiteral("provider");
  const QString dbName =
      m_source.value(QStringLiteral("historyDbName"), m_sourceId).toString();
  return dbName.isEmpty() ? QString() : kind + QLatin1Char(':') + dbName;
}
QString SourceDetailModel::historyMetric() const {
  return chooseHistoryMetric();
}

void SourceDetailModel::registerDailyState(QObject *modelObject) {
  auto *model = qobject_cast<DailyStateModel *>(modelObject);
  if (m_dailyState == model)
    return;
  if (m_dailyState)
    disconnect(m_dailyState, nullptr, this, nullptr);
  m_dailyState = model;
  if (model) {
    connect(model, &DailyStateModel::sourceChanged, this,
            [this](const QString &sourceId) {
              if (sourceId == m_sourceId)
                rebuild();
            });
    connect(model, &QAbstractItemModel::modelReset, this,
            &SourceDetailModel::rebuild);
    connect(model, &QObject::destroyed, this, [this]() {
      m_dailyState = nullptr;
      rebuild();
    });
  }
  rebuild();
}

void SourceDetailModel::registerHistoryDatabase(QObject *databaseObject) {
  auto *database = qobject_cast<UsageDatabase *>(databaseObject);
  if (m_historyDatabase == database)
    return;
  if (m_historyDatabase)
    disconnect(m_historyDatabase, nullptr, this, nullptr);
  m_historyDatabase = database;
  if (database) {
    connect(database, &UsageDatabase::historySeriesReady, this,
            &SourceDetailModel::handleHistoryResult);
    connect(database, &QObject::destroyed, this, [this]() {
      m_historyDatabase = nullptr;
      m_historyLoading = false;
      Q_EMIT historyChanged();
    });
  }
  refreshHistory();
}

void SourceDetailModel::refreshHistory() {
  m_recentHistory.clear();
  m_historyRequestId.clear();
  m_historyLoading = false;
  const QString metric = chooseHistoryMetric();
  if (!m_historyDatabase || m_sourceId.isEmpty() || metric.isEmpty()) {
    Q_EMIT historyChanged();
    return;
  }

  const bool tool = m_source.value(QStringLiteral("sourceKind")).toString() ==
                    QLatin1String("local_tool");
  const QString dbName =
      m_source.value(QStringLiteral("historyDbName"), m_sourceId).toString();
  const QVariantMap descriptor{
      {QStringLiteral("historyId"), historyId()},
      {QStringLiteral("sourceKind"),
       tool ? QStringLiteral("tool") : QStringLiteral("provider")},
      {QStringLiteral("dbName"), dbName},
      {QStringLiteral("displayName"),
       m_source.value(QStringLiteral("displayName"))}};
  m_historyRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_historyLoading = true;
  Q_EMIT historyChanged();
  const QDateTime to = QDateTime::currentDateTimeUtc();
  m_historyDatabase->requestHistorySeries(m_historyRequestId,
                                          QVariantList{descriptor},
                                          to.addDays(-7), to, metric, 60);
}

void SourceDetailModel::rebuild() {
  beginResetModel();
  m_source = m_dailyState && !m_sourceId.isEmpty()
                 ? m_dailyState->source(m_sourceId)
                 : QVariantMap();
  m_scopeBreakdown = ScopeBreakdownQuery::run(
      m_source.value(QStringLiteral("detailMetrics")).toList());
  m_metrics = m_scopeBreakdown.value(QStringLiteral("rows")).toList();
  endResetModel();

  int available = 0;
  int actual = 0;
  int estimated = 0;
  for (const QVariant &value : std::as_const(m_metrics)) {
    const QVariantMap metric = value.toMap();
    if (!availableMetric(metric))
      continue;
    ++available;
    const QString source = metric.value(QStringLiteral("source")).toString();
    const QString quality = metric.value(QStringLiteral("quality")).toString();
    if (actualMetric(source, quality))
      ++actual;
    else if (estimatedMetric(source, quality))
      ++estimated;
  }
  m_coverage = {{QStringLiteral("availableMetricCount"), available},
                {QStringLiteral("totalMetricCount"), m_metrics.size()},
                {QStringLiteral("actualMetricCount"), actual},
                {QStringLiteral("estimatedMetricCount"), estimated},
                {QStringLiteral("freshnessState"),
                 m_source.value(QStringLiteral("freshnessState"))}};
  Q_EMIT sourceChanged();
  refreshHistory();
}

void SourceDetailModel::handleHistoryResult(const QString &requestId,
                                            const QVariantMap &payload) {
  if (requestId != m_historyRequestId)
    return;
  m_historyLoading = false;
  m_recentHistory = payload;
  Q_EMIT historyChanged();
}

QString SourceDetailModel::chooseHistoryMetric() const {
  const bool tool = m_source.value(QStringLiteral("sourceKind")).toString() ==
                    QLatin1String("local_tool");
  if (tool)
    return m_source.value(QStringLiteral("percentUsedAvailable")).toBool()
               ? QStringLiteral("percentUsed")
               : QStringLiteral("usageCount");

  const QStringList order{QStringLiteral("cost"), QStringLiteral("tokens"),
                          QStringLiteral("requests"),
                          QStringLiteral("rateLimitUsed")};
  for (const QString &candidate : order) {
    for (const QVariant &value : m_metrics) {
      const QVariantMap metric = value.toMap();
      const QString kind = metric.value(QStringLiteral("kind")).toString();
      const bool matches =
          kind == candidate ||
          (candidate == QLatin1String("tokens") && kind.contains("tokens")) ||
          (candidate == QLatin1String("rateLimitUsed") &&
           (kind.contains("limit") || kind.contains("remaining")));
      if (matches && availableMetric(metric))
        return candidate;
    }
  }
  return {};
}

QString SourceDetailModel::actionLabelFor(const QVariantMap &source) {
  const QString action =
      source.value(QStringLiteral("nextActionKey")).toString();
  const QString reason =
      source.value(QStringLiteral("attentionReasonKey")).toString();
  if (action == QLatin1String("add_credentials") ||
      action == QLatin1String("replace_credentials"))
    return i18n("Add credential");
  if (action == QLatin1String("refresh_stale_data") ||
      action == QLatin1String("verify_source") ||
      action == QLatin1String("check_network") ||
      action == QLatin1String("retry_later"))
    return i18n("Refresh");
  if (action == QLatin1String("review_quota") ||
      reason.startsWith(QLatin1String("quota_")))
    return i18n("Review quota");
  if (action.isEmpty() || action == QLatin1String("none"))
    return QString();
  return i18n("Open source settings");
}
