#include "dailystatemodel.h"

#include "providerbackend.h"
#include "sourcereadinessmodel.h"
#include "subscriptionplancatalog.h"
#include "subscriptiontoolbackend.h"

#include <QDateTime>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
const QStringList kRoleNames{QStringLiteral("stableId"),
                             QStringLiteral("displayName"),
                             QStringLiteral("sourceKind"),
                             QStringLiteral("monitoringLevel"),
                             QStringLiteral("readinessState"),
                             QStringLiteral("qualityClass"),
                             QStringLiteral("freshnessState"),
                             QStringLiteral("lastSuccess"),
                             QStringLiteral("lastAttempt"),
                             QStringLiteral("lastErrorKind"),
                             QStringLiteral("nextActionKey"),
                             QStringLiteral("hasUsefulData"),
                             QStringLiteral("hasActualData"),
                             QStringLiteral("hasEstimatedData"),
                             QStringLiteral("hasBalance"),
                             QStringLiteral("connectivityOnly"),
                             QStringLiteral("attentionSeverity"),
                             QStringLiteral("attentionReasonKey"),
                             QStringLiteral("primaryMetricKind"),
                             QStringLiteral("primaryMetricAvailable"),
                             QStringLiteral("primaryMetricValue"),
                             QStringLiteral("primaryMetricUnit"),
                             QStringLiteral("percentUsedAvailable"),
                             QStringLiteral("percentUsed"),
                             QStringLiteral("percentRemainingAvailable"),
                             QStringLiteral("percentRemaining"),
                             QStringLiteral("resetAtAvailable"),
                             QStringLiteral("resetAt"),
                             QStringLiteral("currency"),
                             QStringLiteral("costAvailable"),
                             QStringLiteral("costValue"),
                             QStringLiteral("costSource"),
                             QStringLiteral("budgetAvailable"),
                             QStringLiteral("budgetPercentUsed")};

bool metricAvailable(const QVariantMap &metric) {
  const QVariant value = metric.value(QStringLiteral("value"));
  return metric.value(QStringLiteral("available")).toBool() &&
         value.isValid() && !value.isNull();
}

bool finiteNumber(const QVariant &value) {
  bool ok = false;
  const double number = value.toDouble(&ok);
  return ok && std::isfinite(number);
}

QDateTime asDateTime(const QVariant &value) {
  QDateTime result = value.toDateTime();
  if (!result.isValid())
    result = QDateTime::fromString(value.toString(), Qt::ISODate);
  return result.isValid() ? result.toUTC() : QDateTime();
}

bool actualSource(const QString &source) {
  static const QSet<QString> sources{QStringLiteral("billing_api"),
                                     QStringLiteral("usage_api"),
                                     QStringLiteral("actual_api"),
                                     QStringLiteral("metrics_api"),
                                     QStringLiteral("response_headers"),
                                     QStringLiteral("browser_sync"),
                                     QStringLiteral("antigravity_local"),
                                     QStringLiteral("local_daemon_actual")};
  return sources.contains(source);
}

bool estimatedSource(const QString &source) {
  static const QSet<QString> sources{QStringLiteral("estimated_pricing"),
                                     QStringLiteral("estimated_from_usage"),
                                     QStringLiteral("local_observation"),
                                     QStringLiteral("self_tracked")};
  return sources.contains(source);
}

QString freshnessKey(ProviderBackend::Freshness freshness) {
  switch (freshness) {
  case ProviderBackend::Freshness::Fresh:
    return QStringLiteral("fresh");
  case ProviderBackend::Freshness::Aging:
    return QStringLiteral("aging");
  case ProviderBackend::Freshness::Stale:
    return QStringLiteral("stale");
  case ProviderBackend::Freshness::Never:
    return QStringLiteral("never");
  }
  return QStringLiteral("never");
}

void addCurrency(QVariantMap &totals, const QString &currency, double value) {
  const QString key = currency.trimmed().toUpper();
  if (key.isEmpty() || !std::isfinite(value))
    return;
  totals.insert(key, totals.value(key).toDouble() + value);
}

void mergeCurrencies(QVariantMap &target, const QVariantMap &source) {
  for (auto it = source.cbegin(); it != source.cend(); ++it)
    addCurrency(target, it.key(), it.value().toDouble());
}

QVariantMap publicRow(QVariantMap row) {
  for (auto it = row.begin(); it != row.end();) {
    if (it.key().startsWith(QLatin1Char('_')))
      it = row.erase(it);
    else
      ++it;
  }
  return row;
}

QVariantMap quota(const QString &kind, const QString &window, double used,
                  double remaining, const QDateTime &resetAt) {
  QVariantMap result{
      {QStringLiteral("kind"), kind},
      {QStringLiteral("window"), window},
      {QStringLiteral("percentUsed"), qBound(0.0, used, 100.0)},
      {QStringLiteral("percentRemaining"), qBound(0.0, remaining, 100.0)}};
  if (resetAt.isValid())
    result.insert(QStringLiteral("resetAt"), resetAt);
  return result;
}

bool betterQuota(const QVariantMap &candidate, const QVariantMap &current) {
  if (current.isEmpty())
    return true;
  const double left =
      candidate.value(QStringLiteral("percentRemaining"), 101.0).toDouble();
  const double right =
      current.value(QStringLiteral("percentRemaining"), 101.0).toDouble();
  if (!qFuzzyCompare(left + 1.0, right + 1.0))
    return left < right;
  const QDateTime leftReset =
      asDateTime(candidate.value(QStringLiteral("resetAt")));
  const QDateTime rightReset =
      asDateTime(current.value(QStringLiteral("resetAt")));
  if (leftReset.isValid() != rightReset.isValid())
    return leftReset.isValid();
  return leftReset.isValid() && leftReset < rightReset;
}

QVariantMap providerQuota(const QVariantList &metrics) {
  QVariantMap best;
  for (const QVariant &entry : metrics) {
    const QVariantMap remainingMetric = entry.toMap();
    const QString kind =
        remainingMetric.value(QStringLiteral("kind")).toString();
    if (!metricAvailable(remainingMetric) ||
        (kind != QLatin1String("request_remaining") &&
         kind != QLatin1String("token_remaining")))
      continue;
    const QString limitKind = kind == QLatin1String("request_remaining")
                                  ? QStringLiteral("request_limit")
                                  : QStringLiteral("token_limit");
    QVariantMap limitMetric;
    for (const QVariant &possibleEntry : metrics) {
      const QVariantMap possible = possibleEntry.toMap();
      if (possible.value(QStringLiteral("kind")) == limitKind &&
          possible.value(QStringLiteral("scope")) ==
              remainingMetric.value(QStringLiteral("scope")) &&
          possible.value(QStringLiteral("window")) ==
              remainingMetric.value(QStringLiteral("window")) &&
          metricAvailable(possible)) {
        limitMetric = possible;
        break;
      }
    }
    const double limit = limitMetric.value(QStringLiteral("value")).toDouble();
    if (limit <= 0.0)
      continue;
    const double remaining =
        remainingMetric.value(QStringLiteral("value")).toDouble() * 100.0 /
        limit;
    const QVariantMap candidate =
        quota(kind, remainingMetric.value(QStringLiteral("window")).toString(),
              100.0 - remaining, remaining,
              asDateTime(remainingMetric.value(QStringLiteral("resetAt"))));
    if (betterQuota(candidate, best))
      best = candidate;
  }
  return best;
}

QVariantMap toolQuota(SubscriptionToolBackend *tool) {
  QVariantMap best;
  for (const QVariant &entry : tool->quotaWindows()) {
    const QVariantMap window = entry.toMap();
    if (window.value(QStringLiteral("precision")) ==
        QLatin1String("availability_only"))
      continue;
    const bool hasUsed =
        finiteNumber(window.value(QStringLiteral("percentUsed")));
    const bool hasRemaining =
        finiteNumber(window.value(QStringLiteral("percentRemaining")));
    if (!hasUsed && !hasRemaining)
      continue;
    const double used =
        hasUsed
            ? window.value(QStringLiteral("percentUsed")).toDouble()
            : 100.0 -
                  window.value(QStringLiteral("percentRemaining")).toDouble();
    const double remaining =
        hasRemaining
            ? window.value(QStringLiteral("percentRemaining")).toDouble()
            : 100.0 - used;
    const QVariantMap candidate = quota(
        window.value(QStringLiteral("kind"), QStringLiteral("quota"))
            .toString(),
        window
            .value(QStringLiteral("window"),
                   window.value(QStringLiteral("label")))
            .toString(),
        used, remaining, asDateTime(window.value(QStringLiteral("resetAt"))));
    if (betterQuota(candidate, best))
      best = candidate;
  }
  const auto consider = [&best](const QString &kind, const QString &window,
                                int used, int limit, const QDateTime &resetAt) {
    if (limit <= 0)
      return;
    const double percentUsed = static_cast<double>(used) * 100.0 / limit;
    const QVariantMap candidate =
        quota(kind, window, percentUsed, 100.0 - percentUsed, resetAt);
    if (betterQuota(candidate, best))
      best = candidate;
  };
  consider(QStringLiteral("primary_quota"), tool->periodLabel(),
           tool->usageCount(), tool->usageLimit(), tool->periodEnd());
  if (tool->hasSecondaryLimit())
    consider(QStringLiteral("secondary_quota"), tool->secondaryPeriodLabel(),
             tool->secondaryUsageCount(), tool->secondaryUsageLimit(),
             tool->secondaryPeriodEnd());
  return best;
}

QVariantList preferredCosts(const QVariantList &metrics) {
  QVariantList available;
  for (const QVariant &entry : metrics) {
    const QVariantMap metric = entry.toMap();
    if (metric.value(QStringLiteral("kind")) == QLatin1String("cost") &&
        metricAvailable(metric))
      available.append(metric);
  }
  if (available.isEmpty())
    return {};
  for (const QString &window :
       {QStringLiteral("current"), QStringLiteral("month"),
        QStringLiteral("day")}) {
    QVariantList selected;
    for (const QVariant &entry : available) {
      if (entry.toMap().value(QStringLiteral("window")) == window)
        selected.append(entry);
    }
    if (!selected.isEmpty())
      return selected;
  }
  const QString firstWindow =
      available.first().toMap().value(QStringLiteral("window")).toString();
  QVariantList selected;
  for (const QVariant &entry : available) {
    if (entry.toMap().value(QStringLiteral("window")).toString() == firstWindow)
      selected.append(entry);
  }
  return selected;
}

int severityRank(const QString &severity) {
  return severity == QLatin1String("critical")  ? 3
         : severity == QLatin1String("warning") ? 2
         : severity == QLatin1String("info")    ? 1
                                                : 0;
}
} // namespace

DailyStateModel::DailyStateModel(QObject *parent)
    : QAbstractListModel(parent), m_summary(buildSummary({})) {}

int DailyStateModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_rows.size();
}

QVariant DailyStateModel::data(const QModelIndex &index, int role) const {
  const int offset = role - StableIdRole;
  if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size() ||
      offset < 0 || offset >= kRoleNames.size())
    return {};
  return m_rows.at(index.row()).value(kRoleNames.at(offset));
}

QHash<int, QByteArray> DailyStateModel::roleNames() const {
  QHash<int, QByteArray> result;
  for (int i = 0; i < kRoleNames.size(); ++i)
    result.insert(StableIdRole + i, kRoleNames.at(i).toUtf8());
  return result;
}

QVariantMap DailyStateModel::summary() const { return m_summary; }
int DailyStateModel::warningThreshold() const { return m_warningThreshold; }
int DailyStateModel::criticalThreshold() const { return m_criticalThreshold; }

void DailyStateModel::setWarningThreshold(int threshold) {
  threshold = qBound(0, threshold, 100);
  if (m_warningThreshold == threshold)
    return;
  m_warningThreshold = threshold;
  if (m_criticalThreshold < threshold)
    m_criticalThreshold = threshold;
  Q_EMIT thresholdsChanged();
  rebuild();
}

void DailyStateModel::setCriticalThreshold(int threshold) {
  threshold = qBound(0, threshold, 100);
  if (m_criticalThreshold == threshold)
    return;
  m_criticalThreshold = threshold;
  if (m_warningThreshold > threshold)
    m_warningThreshold = threshold;
  Q_EMIT thresholdsChanged();
  rebuild();
}

void DailyStateModel::registerReadinessModel(QObject *modelObject) {
  auto *model = qobject_cast<SourceReadinessModel *>(modelObject);
  if (m_readinessModel == model)
    return;
  if (m_readinessModel)
    disconnect(m_readinessModel, nullptr, this, nullptr);
  m_readinessModel = model;
  if (model != nullptr) {
    connect(model, &SourceReadinessModel::sourceChanged, this,
            [this](const QString &) { rebuild(); });
    connect(model, &QAbstractItemModel::modelReset, this,
            &DailyStateModel::rebuild);
    connect(model, &QObject::destroyed, this, [this]() {
      m_readinessModel = nullptr;
      rebuild();
    });
  }
  rebuild();
}

void DailyStateModel::registerProviderBackend(const QString &stableId,
                                              QObject *backendObject) {
  auto *backend = qobject_cast<ProviderBackend *>(backendObject);
  if (stableId.isEmpty() || backend == nullptr ||
      m_providerBackends.value(stableId) == backend)
    return;
  if (m_providerBackends.value(stableId))
    disconnect(m_providerBackends.value(stableId), nullptr, this, nullptr);
  m_providerBackends.insert(stableId, backend);
  connectProvider(stableId, backend);
  rebuild();
}

void DailyStateModel::registerLocalTool(const QString &stableId,
                                        QObject *backendObject) {
  auto *backend = qobject_cast<SubscriptionToolBackend *>(backendObject);
  if (stableId.isEmpty() || backend == nullptr ||
      m_toolBackends.value(stableId) == backend)
    return;
  if (m_toolBackends.value(stableId))
    disconnect(m_toolBackends.value(stableId), nullptr, this, nullptr);
  m_toolBackends.insert(stableId, backend);
  connectTool(stableId, backend);
  rebuild();
}

QVariantMap DailyStateModel::source(const QString &stableId) const {
  for (const QVariantMap &row : m_rows) {
    if (row.value(QStringLiteral("stableId")) == stableId)
      return publicRow(row);
  }
  return {};
}

QStringList DailyStateModel::prioritizedSourceIds() const {
  QStringList ids;
  for (const QVariantMap &row : m_rows)
    ids.append(row.value(QStringLiteral("stableId")).toString());
  return ids;
}

void DailyStateModel::refresh() { rebuild(); }

QVariantMap DailyStateModel::buildRow(const QVariantMap &readiness,
                                      int sourceOrder) const {
  QVariantMap row{
      {QStringLiteral("stableId"), readiness.value(QStringLiteral("stableId"))},
      {QStringLiteral("displayName"),
       readiness.value(QStringLiteral("displayName"))},
      {QStringLiteral("sourceKind"),
       readiness.value(QStringLiteral("sourceKindKey"))},
      {QStringLiteral("monitoringLevel"),
       readiness.value(QStringLiteral("monitoringLevel"))},
      {QStringLiteral("readinessState"),
       readiness.value(QStringLiteral("readinessStateKey"))},
      {QStringLiteral("qualityClass"), QStringLiteral("unavailable")},
      {QStringLiteral("freshnessState"), QStringLiteral("never")},
      {QStringLiteral("lastSuccess"),
       readiness.value(QStringLiteral("lastVerified"))},
      {QStringLiteral("lastAttempt"),
       readiness.value(QStringLiteral("lastVerified"))},
      {QStringLiteral("lastErrorKind"),
       readiness.value(QStringLiteral("errorCode"))},
      {QStringLiteral("nextActionKey"),
       readiness.value(QStringLiteral("nextActionKey"))},
      {QStringLiteral("hasUsefulData"), false},
      {QStringLiteral("hasActualData"), false},
      {QStringLiteral("hasEstimatedData"), false},
      {QStringLiteral("hasBalance"), false},
      {QStringLiteral("connectivityOnly"), false},
      {QStringLiteral("primaryMetricKind"), QString()},
      {QStringLiteral("primaryMetricAvailable"), false},
      {QStringLiteral("primaryMetricValue"), QVariant()},
      {QStringLiteral("primaryMetricUnit"), QString()},
      {QStringLiteral("percentUsedAvailable"), false},
      {QStringLiteral("percentUsed"), QVariant()},
      {QStringLiteral("percentRemainingAvailable"), false},
      {QStringLiteral("percentRemaining"), QVariant()},
      {QStringLiteral("resetAtAvailable"), false},
      {QStringLiteral("resetAt"), QVariant()},
      {QStringLiteral("currency"), QString()},
      {QStringLiteral("costAvailable"), false},
      {QStringLiteral("costValue"), QVariant()},
      {QStringLiteral("costSource"), QStringLiteral("unknown")},
      {QStringLiteral("budgetAvailable"), false},
      {QStringLiteral("budgetPercentUsed"), QVariant()},
      {QStringLiteral("_actualCosts"), QVariantMap()},
      {QStringLiteral("_estimatedCosts"), QVariantMap()},
      {QStringLiteral("_fixedFees"), QVariantMap()}};
  const QString id = row.value(QStringLiteral("stableId")).toString();
  row = row.value(QStringLiteral("sourceKind")) == QLatin1String("provider")
            ? buildProviderRow(row, m_providerBackends.value(id))
            : buildToolRow(row, m_toolBackends.value(id));
  return finalizeRow(row, sourceOrder);
}

QVariantMap DailyStateModel::buildProviderRow(QVariantMap row,
                                              ProviderBackend *backend) const {
  if (backend == nullptr)
    return row;
  row.insert(QStringLiteral("lastSuccess"), backend->lastSuccess());
  row.insert(QStringLiteral("lastAttempt"), backend->lastAttempt());
  row.insert(QStringLiteral("freshnessState"),
             freshnessKey(backend->freshness()));

  bool actual = false;
  bool estimated = false;
  QVariantMap balance;
  QVariantMap fallback;
  for (const QVariant &entry : backend->metrics()) {
    const QVariantMap metric = entry.toMap();
    if (!metricAvailable(metric))
      continue;
    actual = actual ||
             actualSource(metric.value(QStringLiteral("source")).toString());
    estimated =
        estimated ||
        estimatedSource(metric.value(QStringLiteral("source")).toString());
    if (metric.value(QStringLiteral("kind")) == QLatin1String("credit_balance"))
      balance = metric;
    if (fallback.isEmpty())
      fallback = metric;
  }
  actual = actual || row.value(QStringLiteral("readinessState")) ==
                         QLatin1String("reporting_actual");
  estimated = estimated || row.value(QStringLiteral("readinessState")) ==
                               QLatin1String("reporting_estimate");
  row.insert(QStringLiteral("hasActualData"), actual);
  row.insert(QStringLiteral("hasEstimatedData"), estimated);
  row.insert(QStringLiteral("hasBalance"), !balance.isEmpty());
  row.insert(QStringLiteral("connectivityOnly"),
             row.value(QStringLiteral("readinessState")) ==
                 QLatin1String("connected_connectivity_only"));

  const QVariantMap quotaRow = providerQuota(backend->metrics());
  if (!quotaRow.isEmpty()) {
    row.insert(QStringLiteral("primaryMetricKind"),
               quotaRow.value(QStringLiteral("kind")));
    row.insert(QStringLiteral("primaryMetricAvailable"), true);
    row.insert(QStringLiteral("primaryMetricValue"),
               quotaRow.value(QStringLiteral("percentRemaining")));
    row.insert(QStringLiteral("primaryMetricUnit"),
               QStringLiteral("percent_remaining"));
    row.insert(QStringLiteral("percentUsedAvailable"), true);
    row.insert(QStringLiteral("percentUsed"),
               quotaRow.value(QStringLiteral("percentUsed")));
    row.insert(QStringLiteral("percentRemainingAvailable"), true);
    row.insert(QStringLiteral("percentRemaining"),
               quotaRow.value(QStringLiteral("percentRemaining")));
    const QDateTime resetAt =
        asDateTime(quotaRow.value(QStringLiteral("resetAt")));
    row.insert(QStringLiteral("resetAtAvailable"), resetAt.isValid());
    row.insert(QStringLiteral("resetAt"), resetAt);
    row.insert(QStringLiteral("_quotaWindow"),
               quotaRow.value(QStringLiteral("window")));
  } else {
    const QVariantMap primary = !balance.isEmpty() ? balance : fallback;
    if (!primary.isEmpty()) {
      row.insert(QStringLiteral("primaryMetricKind"),
                 primary.value(QStringLiteral("kind")));
      row.insert(QStringLiteral("primaryMetricAvailable"), true);
      row.insert(QStringLiteral("primaryMetricValue"),
                 primary.value(QStringLiteral("value")));
      row.insert(QStringLiteral("primaryMetricUnit"),
                 primary.value(QStringLiteral("unit")));
    }
  }

  QVariantMap actualCosts;
  QVariantMap estimatedCosts;
  QStringList costSources;
  for (const QVariant &entry : preferredCosts(backend->metrics())) {
    const QVariantMap metric = entry.toMap();
    const QString source = metric.value(QStringLiteral("source")).toString();
    const QString currency =
        metric.value(QStringLiteral("currency"), backend->currency())
            .toString();
    if (estimatedSource(source))
      addCurrency(estimatedCosts, currency,
                  metric.value(QStringLiteral("value")).toDouble());
    else if (actualSource(source))
      addCurrency(actualCosts, currency,
                  metric.value(QStringLiteral("value")).toDouble());
    if (!costSources.contains(source))
      costSources.append(source);
  }
  row.insert(QStringLiteral("_actualCosts"), actualCosts);
  row.insert(QStringLiteral("_estimatedCosts"), estimatedCosts);
  QVariantMap combined = actualCosts;
  mergeCurrencies(combined, estimatedCosts);
  if (combined.size() == 1) {
    row.insert(QStringLiteral("currency"), combined.firstKey());
    row.insert(QStringLiteral("costAvailable"), true);
    row.insert(QStringLiteral("costValue"), combined.constBegin().value());
  } else if (combined.size() > 1) {
    row.insert(QStringLiteral("currency"), QStringLiteral("MIXED"));
  }
  row.insert(QStringLiteral("costSource"),
             costSources.size() == 1 ? costSources.first()
             : costSources.isEmpty() ? QStringLiteral("unknown")
                                     : QStringLiteral("mixed"));

  bool hasDailyCost = false;
  bool hasMonthlyCost = false;
  for (const QVariant &entry : backend->metrics()) {
    const QVariantMap metric = entry.toMap();
    if (metric.value(QStringLiteral("kind")) != QLatin1String("cost") ||
        !metricAvailable(metric))
      continue;
    hasDailyCost = hasDailyCost || metric.value(QStringLiteral("window")) ==
                                       QLatin1String("day");
    hasMonthlyCost = hasMonthlyCost || metric.value(QStringLiteral("window")) ==
                                           QLatin1String("month");
  }
  double budgetPercent = -1.0;
  if (!backend->budgetCurrencyMismatch()) {
    if (hasDailyCost && backend->dailyBudget() > 0.0)
      budgetPercent = backend->dailyCost() * 100.0 / backend->dailyBudget();
    if (hasMonthlyCost && backend->monthlyBudget() > 0.0)
      budgetPercent = qMax(budgetPercent, backend->monthlyCost() * 100.0 /
                                              backend->monthlyBudget());
  }
  if (budgetPercent >= 0.0 && !combined.isEmpty()) {
    row.insert(QStringLiteral("budgetAvailable"), true);
    row.insert(QStringLiteral("budgetPercentUsed"), budgetPercent);
  }
  return row;
}

QVariantMap DailyStateModel::buildToolRow(QVariantMap row,
                                          SubscriptionToolBackend *tool) const {
  if (tool == nullptr)
    return row;
  QDateTime lastSuccess = tool->lastSyncTime();
  if (!lastSuccess.isValid() ||
      (tool->lastActivity().isValid() && tool->lastActivity() > lastSuccess))
    lastSuccess = tool->lastActivity();
  row.insert(QStringLiteral("lastSuccess"), lastSuccess);
  row.insert(QStringLiteral("lastAttempt"), lastSuccess);
  row.insert(QStringLiteral("freshnessState"),
             row.value(QStringLiteral("lastErrorKind")) ==
                     QLatin1String("stale")
                 ? QStringLiteral("stale")
             : lastSuccess.isValid() ? QStringLiteral("fresh")
                                     : QStringLiteral("never"));

  bool actual = false;
  for (const QVariant &entry : tool->quotaWindows())
    actual =
        actual ||
        actualSource(entry.toMap().value(QStringLiteral("source")).toString());
  actual = actual || (tool->lastSyncTime().isValid() &&
                      row.value(QStringLiteral("readinessState")) ==
                          QLatin1String("reporting_actual"));
  const bool estimated = tool->lastActivity().isValid() ||
                         tool->usageCount() > 0 ||
                         row.value(QStringLiteral("readinessState")) ==
                             QLatin1String("reporting_estimate");
  row.insert(QStringLiteral("hasActualData"), actual);
  row.insert(QStringLiteral("hasEstimatedData"), estimated);

  const QVariantMap quotaRow = toolQuota(tool);
  if (!quotaRow.isEmpty()) {
    row.insert(QStringLiteral("primaryMetricKind"),
               quotaRow.value(QStringLiteral("kind")));
    row.insert(QStringLiteral("primaryMetricAvailable"), true);
    row.insert(QStringLiteral("primaryMetricValue"),
               quotaRow.value(QStringLiteral("percentRemaining")));
    row.insert(QStringLiteral("primaryMetricUnit"),
               QStringLiteral("percent_remaining"));
    row.insert(QStringLiteral("percentUsedAvailable"), true);
    row.insert(QStringLiteral("percentUsed"),
               quotaRow.value(QStringLiteral("percentUsed")));
    row.insert(QStringLiteral("percentRemainingAvailable"), true);
    row.insert(QStringLiteral("percentRemaining"),
               quotaRow.value(QStringLiteral("percentRemaining")));
    const QDateTime resetAt =
        asDateTime(quotaRow.value(QStringLiteral("resetAt")));
    row.insert(QStringLiteral("resetAtAvailable"), resetAt.isValid());
    row.insert(QStringLiteral("resetAt"), resetAt);
    row.insert(QStringLiteral("_quotaWindow"),
               quotaRow.value(QStringLiteral("window")));
  } else if (tool->hasCredits()) {
    row.insert(QStringLiteral("primaryMetricKind"),
               QStringLiteral("credit_balance"));
    row.insert(QStringLiteral("primaryMetricAvailable"), true);
    row.insert(QStringLiteral("primaryMetricValue"), tool->remainingCredits());
    row.insert(QStringLiteral("primaryMetricUnit"), QStringLiteral("credits"));
    row.insert(QStringLiteral("hasBalance"), true);
  }

  const QVariantMap price = SubscriptionPlanCatalog::instance()->price(
      row.value(QStringLiteral("stableId")).toString(), tool->planTier());
  QVariantMap fixedFees;
  if (finiteNumber(price.value(QStringLiteral("amount"))))
    addCurrency(fixedFees, price.value(QStringLiteral("currency")).toString(),
                price.value(QStringLiteral("amount")).toDouble());
  row.insert(QStringLiteral("_fixedFees"), fixedFees);
  if (tool->hasExtraUsage()) {
    QString currency = price.value(QStringLiteral("currency")).toString();
    if (currency.isEmpty() && tool->currencySymbol() == QLatin1String("$"))
      currency = QStringLiteral("USD");
    QVariantMap actualCosts;
    addCurrency(actualCosts, currency, tool->extraUsageSpent());
    row.insert(QStringLiteral("_actualCosts"), actualCosts);
    if (!actualCosts.isEmpty()) {
      row.insert(QStringLiteral("currency"), actualCosts.firstKey());
      row.insert(QStringLiteral("costAvailable"), true);
      row.insert(QStringLiteral("costValue"), actualCosts.constBegin().value());
      row.insert(QStringLiteral("costSource"), QStringLiteral("browser_sync"));
    }
  }
  return row;
}

QVariantMap DailyStateModel::finalizeRow(QVariantMap row,
                                         int sourceOrder) const {
  const bool actual = row.value(QStringLiteral("hasActualData")).toBool();
  const bool estimated = row.value(QStringLiteral("hasEstimatedData")).toBool();
  const bool balance = row.value(QStringLiteral("hasBalance")).toBool();
  const bool connectivity =
      row.value(QStringLiteral("connectivityOnly")).toBool();
  row.insert(QStringLiteral("hasUsefulData"), actual || estimated || balance);
  row.insert(QStringLiteral("qualityClass"),
             balance        ? QStringLiteral("balance")
             : actual       ? QStringLiteral("actual")
             : estimated    ? QStringLiteral("estimated")
             : connectivity ? QStringLiteral("connectivity_only")
                            : QStringLiteral("unavailable"));

  int priority = 10;
  QString severity = QStringLiteral("none");
  QString reason = QStringLiteral("none");
  const QString readiness =
      row.value(QStringLiteral("readinessState")).toString();
  const QString error = row.value(QStringLiteral("lastErrorKind")).toString();
  const bool stale =
      row.value(QStringLiteral("freshnessState")) == QLatin1String("stale") ||
      error == QLatin1String("stale");
  const bool hasRemaining =
      row.value(QStringLiteral("percentRemainingAvailable")).toBool();
  const double remaining =
      row.value(QStringLiteral("percentRemaining")).toDouble();
  const double used = row.value(QStringLiteral("percentUsed")).toDouble();
  const bool hasBudget = row.value(QStringLiteral("budgetAvailable")).toBool();
  const double budget =
      row.value(QStringLiteral("budgetPercentUsed")).toDouble();
  if (!stale && (readiness == QLatin1String("failed") ||
                 readiness == QLatin1String("needs_configuration") ||
                 readiness == QLatin1String("unavailable_locally") ||
                 readiness == QLatin1String("degraded"))) {
    priority = 1;
    severity = QStringLiteral("critical");
    reason = error.isEmpty() ? readiness : error;
  } else if (hasRemaining && remaining <= 0.0) {
    priority = 2;
    severity = QStringLiteral("critical");
    reason = QStringLiteral("quota_exhausted");
  } else if (hasRemaining && remaining <= 100.0 - m_criticalThreshold) {
    priority = 3;
    severity = QStringLiteral("critical");
    reason = QStringLiteral("quota_critical");
  } else if (hasBudget && budget >= 100.0) {
    priority = 4;
    severity = QStringLiteral("critical");
    reason = QStringLiteral("budget_critical");
  } else if (stale) {
    priority = 6;
    severity = QStringLiteral("warning");
    reason = QStringLiteral("stale_data");
  } else if ((row.value(QStringLiteral("percentUsedAvailable")).toBool() &&
              used >= m_warningThreshold) ||
             (hasBudget && budget >= m_warningThreshold)) {
    priority = 7;
    severity = QStringLiteral("warning");
    reason = hasBudget && budget >= m_warningThreshold
                 ? QStringLiteral("budget_warning")
                 : QStringLiteral("quota_warning");
  } else if (readiness == QLatin1String("ready_to_verify")) {
    priority = 8;
    severity = QStringLiteral("info");
    reason = QStringLiteral("ready_to_verify");
  } else if (connectivity) {
    priority = 9;
  }
  row.insert(QStringLiteral("attentionSeverity"), severity);
  row.insert(QStringLiteral("attentionReasonKey"), reason);
  row.insert(QStringLiteral("_priority"), priority);
  row.insert(QStringLiteral("_sourceOrder"), sourceOrder);
  return row;
}

QVariantMap
DailyStateModel::buildSummary(const QList<QVariantMap> &rows) const {
  QVariantMap summary{
      {QStringLiteral("enabledSourceCount"), rows.size()},
      {QStringLiteral("reportingUsefulSourceCount"), 0},
      {QStringLiteral("actualSourceCount"), 0},
      {QStringLiteral("estimatedSourceCount"), 0},
      {QStringLiteral("balanceSourceCount"), 0},
      {QStringLiteral("connectivityOnlySourceCount"), 0},
      {QStringLiteral("attentionSourceCount"), 0},
      {QStringLiteral("staleSourceCount"), 0},
      {QStringLiteral("highestSeverity"), QStringLiteral("none")},
      {QStringLiteral("mostUrgentSourceId"), QString()},
      {QStringLiteral("mostUrgentSource"), QVariantMap()},
      {QStringLiteral("lowestRemainingQuota"), QVariantMap()},
      {QStringLiteral("nearestReset"), QVariantMap()},
      {QStringLiteral("actualSpendTotals"), QVariantMap()},
      {QStringLiteral("estimatedSpendTotals"), QVariantMap()},
      {QStringLiteral("fixedSubscriptionFees"), QVariantMap()},
      {QStringLiteral("lastAggregateRefreshCompletion"), QDateTime()}};
  QVariantMap actualCosts;
  QVariantMap estimatedCosts;
  QVariantMap fixedFees;
  QVariantMap lowestQuota;
  QVariantMap nearestReset;
  QDateTime lastCompletion;
  int highestSeverity = 0;
  for (const QVariantMap &row : rows) {
    const auto increment = [&summary](const QString &key) {
      summary.insert(key, summary.value(key).toInt() + 1);
    };
    if (row.value(QStringLiteral("hasUsefulData")).toBool())
      increment(QStringLiteral("reportingUsefulSourceCount"));
    if (row.value(QStringLiteral("hasActualData")).toBool())
      increment(QStringLiteral("actualSourceCount"));
    if (row.value(QStringLiteral("hasEstimatedData")).toBool())
      increment(QStringLiteral("estimatedSourceCount"));
    if (row.value(QStringLiteral("hasBalance")).toBool())
      increment(QStringLiteral("balanceSourceCount"));
    if (row.value(QStringLiteral("connectivityOnly")).toBool())
      increment(QStringLiteral("connectivityOnlySourceCount"));
    if (row.value(QStringLiteral("attentionSeverity")) != QLatin1String("none"))
      increment(QStringLiteral("attentionSourceCount"));
    if (row.value(QStringLiteral("freshnessState")) == QLatin1String("stale"))
      increment(QStringLiteral("staleSourceCount"));
    const int severity =
        severityRank(row.value(QStringLiteral("attentionSeverity")).toString());
    if (severity > highestSeverity) {
      highestSeverity = severity;
      summary.insert(QStringLiteral("highestSeverity"),
                     row.value(QStringLiteral("attentionSeverity")));
    }
    mergeCurrencies(actualCosts,
                    row.value(QStringLiteral("_actualCosts")).toMap());
    mergeCurrencies(estimatedCosts,
                    row.value(QStringLiteral("_estimatedCosts")).toMap());
    mergeCurrencies(fixedFees, row.value(QStringLiteral("_fixedFees")).toMap());
    if (row.value(QStringLiteral("percentRemainingAvailable")).toBool()) {
      QVariantMap candidate{
          {QStringLiteral("stableId"), row.value(QStringLiteral("stableId"))},
          {QStringLiteral("displayName"),
           row.value(QStringLiteral("displayName"))},
          {QStringLiteral("window"), row.value(QStringLiteral("_quotaWindow"))},
          {QStringLiteral("percentRemaining"),
           row.value(QStringLiteral("percentRemaining"))},
          {QStringLiteral("resetAt"), row.value(QStringLiteral("resetAt"))}};
      if (betterQuota(candidate, lowestQuota))
        lowestQuota = candidate;
    }
    const QDateTime resetAt = asDateTime(row.value(QStringLiteral("resetAt")));
    if (row.value(QStringLiteral("resetAtAvailable")).toBool() &&
        (!asDateTime(nearestReset.value(QStringLiteral("resetAt"))).isValid() ||
         resetAt < asDateTime(nearestReset.value(QStringLiteral("resetAt"))))) {
      nearestReset = {
          {QStringLiteral("stableId"), row.value(QStringLiteral("stableId"))},
          {QStringLiteral("displayName"),
           row.value(QStringLiteral("displayName"))},
          {QStringLiteral("window"), row.value(QStringLiteral("_quotaWindow"))},
          {QStringLiteral("resetAt"), resetAt}};
    }
    const QDateTime completion =
        asDateTime(row.value(QStringLiteral("lastSuccess")));
    if (completion.isValid() &&
        (!lastCompletion.isValid() || completion > lastCompletion))
      lastCompletion = completion;
  }
  if (!rows.isEmpty() && rows.first().value(QStringLiteral(
                             "attentionSeverity")) != QLatin1String("none")) {
    summary.insert(QStringLiteral("mostUrgentSourceId"),
                   rows.first().value(QStringLiteral("stableId")));
    summary.insert(QStringLiteral("mostUrgentSource"), publicRow(rows.first()));
  }
  summary.insert(QStringLiteral("lowestRemainingQuota"), lowestQuota);
  summary.insert(QStringLiteral("nearestReset"), nearestReset);
  summary.insert(QStringLiteral("actualSpendTotals"), actualCosts);
  summary.insert(QStringLiteral("estimatedSpendTotals"), estimatedCosts);
  summary.insert(QStringLiteral("fixedSubscriptionFees"), fixedFees);
  summary.insert(QStringLiteral("lastAggregateRefreshCompletion"),
                 lastCompletion);
  return summary;
}

void DailyStateModel::connectProvider(const QString &stableId,
                                      ProviderBackend *backend) {
  const auto update = [this, stableId]() {
    rebuild();
    Q_EMIT sourceChanged(stableId);
  };
  connect(backend, &ProviderBackend::connectedChanged, this, update);
  connect(backend, &ProviderBackend::loadingChanged, this, update);
  connect(backend, &ProviderBackend::errorChanged, this, update);
  connect(backend, &ProviderBackend::stateChanged, this, update);
  connect(backend, &ProviderBackend::dataUpdated, this, update);
  connect(backend, &ProviderBackend::metricsChanged, this, update);
  connect(backend, &ProviderBackend::budgetChanged, this, update);
  connect(backend, &QObject::destroyed, this, [this, stableId]() {
    m_providerBackends.remove(stableId);
    rebuild();
  });
}

void DailyStateModel::connectTool(const QString &stableId,
                                  SubscriptionToolBackend *backend) {
  const auto update = [this, stableId]() {
    rebuild();
    Q_EMIT sourceChanged(stableId);
  };
  connect(backend, &SubscriptionToolBackend::enabledChanged, this, update);
  connect(backend, &SubscriptionToolBackend::installedChanged, this, update);
  connect(backend, &SubscriptionToolBackend::usageUpdated, this, update);
  connect(backend, &SubscriptionToolBackend::usageLimitChanged, this, update);
  connect(backend, &SubscriptionToolBackend::syncStatusChanged, this, update);
  connect(backend, &SubscriptionToolBackend::quotaWindowsChanged, this, update);
  connect(backend, &SubscriptionToolBackend::planTierChanged, this, update);
  connect(backend, &QObject::destroyed, this, [this, stableId]() {
    m_toolBackends.remove(stableId);
    rebuild();
  });
}

void DailyStateModel::rebuild() {
  QList<QVariantMap> rows;
  if (m_readinessModel) {
    for (int sourceOrder = 0; sourceOrder < m_readinessModel->rowCount();
         ++sourceOrder) {
      const QModelIndex sourceIndex = m_readinessModel->index(sourceOrder);
      if (!m_readinessModel
               ->data(sourceIndex, SourceReadinessModel::EnabledRole)
               .toBool())
        continue;
      const QString stableId =
          m_readinessModel
              ->data(sourceIndex, SourceReadinessModel::StableIdRole)
              .toString();
      const QVariantMap readiness = m_readinessModel->source(stableId);
      rows.append(buildRow(readiness, sourceOrder));
    }
  }
  std::stable_sort(
      rows.begin(), rows.end(),
      [](const QVariantMap &left, const QVariantMap &right) {
        const int leftPriority =
            left.value(QStringLiteral("_priority")).toInt();
        const int rightPriority =
            right.value(QStringLiteral("_priority")).toInt();
        if (leftPriority != rightPriority)
          return leftPriority < rightPriority;
        const double leftRemaining =
            left.value(QStringLiteral("percentRemainingAvailable")).toBool()
                ? left.value(QStringLiteral("percentRemaining")).toDouble()
                : 101.0;
        const double rightRemaining =
            right.value(QStringLiteral("percentRemainingAvailable")).toBool()
                ? right.value(QStringLiteral("percentRemaining")).toDouble()
                : 101.0;
        if (!qFuzzyCompare(leftRemaining + 1.0, rightRemaining + 1.0))
          return leftRemaining < rightRemaining;
        const QDateTime leftReset =
            asDateTime(left.value(QStringLiteral("resetAt")));
        const QDateTime rightReset =
            asDateTime(right.value(QStringLiteral("resetAt")));
        if (leftReset.isValid() != rightReset.isValid())
          return leftReset.isValid();
        if (leftReset.isValid() && leftReset != rightReset)
          return leftReset < rightReset;
        return left.value(QStringLiteral("_sourceOrder")).toInt() <
               right.value(QStringLiteral("_sourceOrder")).toInt();
      });
  const bool changedCount = rows.size() != m_rows.size();
  const QVariantMap nextSummary = buildSummary(rows);
  beginResetModel();
  m_rows = rows;
  m_summary = nextSummary;
  endResetModel();
  if (changedCount)
    Q_EMIT countChanged();
  Q_EMIT summaryChanged();
}
