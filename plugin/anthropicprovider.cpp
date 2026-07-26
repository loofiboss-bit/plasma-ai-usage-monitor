#include "anthropicprovider.h"

#include <KLocalizedString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrlQuery>
#include <limits>

namespace {
QDateTime utcDateTime(const QJsonValue &value) {
  const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
  return parsed.isValid() ? parsed.toUTC() : QDateTime();
}

QString rowKey(const QDateTime &start, const QDateTime &end,
               const QString &model, const QString &project,
               const QString &tier) {
  return start.toString(Qt::ISODateWithMs) + QLatin1Char('|') +
         end.toString(Qt::ISODateWithMs) + QLatin1Char('|') + model +
         QLatin1Char('|') + project + QLatin1Char('|') + tier;
}

QString capabilityName(AnthropicProvider::AdminCapability capability) {
  return capability == AnthropicProvider::AdminCapability::Usage
             ? QStringLiteral("usage")
             : QStringLiteral("cost");
}
} // namespace

AnthropicProvider::AnthropicProvider(QObject *parent)
    : ProviderBackend(parent) {
  registerCatalogPricing(QStringLiteral("anthropic"));
}

QString AnthropicProvider::model() const { return m_model; }

void AnthropicProvider::setModel(const QString &model) {
  if (m_model == model)
    return;
  m_model = model;
  Q_EMIT modelChanged();
}

void AnthropicProvider::setAdminApiKey(const QString &key) {
  if (m_adminApiKey == key)
    return;
  cancelRefresh();
  m_adminApiKey = key;
  Q_EMIT credentialsChanged();
}

bool AnthropicProvider::hasAdminApiKey() const {
  if (qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO"))
    return true;
  return !m_adminApiKey.isEmpty();
}

void AnthropicProvider::refreshImpl() {
  if (!hasApiKey() && !hasAdminApiKey()) {
    setErrorDetails(i18n("No Anthropic API key configured"),
                    ProviderErrorKind::Configuration);
    setConnected(false);
    return;
  }

  beginRefresh();
  setLoading(true);
  clearError();
  const int generation = currentGeneration();

  m_pendingUsageRows.clear();
  m_pendingCostRows.clear();
  m_connectivityPending = hasApiKey();
  m_usagePending = hasAdminApiKey();
  m_costPending = hasAdminApiKey();
  m_connectivitySucceeded = false;
  m_usageSucceeded = false;
  m_costSucceeded = false;
  m_usagePartial = false;
  m_costPartial = false;
  m_usagePages = 0;
  m_costPages = 0;

  if (m_connectivityPending)
    fetchModels(generation);
  else
    setCapabilityStatus(QStringLiteral("connectivity"),
                        QStringLiteral("unavailable"),
                        i18n("A standard Anthropic API key is not configured"));

  if (hasAdminApiKey()) {
    const bool backfill =
        lastRefreshReason() == RefreshReason::Startup ||
        lastRefreshReason() == RefreshReason::Manual ||
        lastRefreshReason() == RefreshReason::CredentialChanged;
    const int days = backfill ? 31 : 2;
    fetchAdminPage(AdminCapability::Usage,
                   adminUrl(AdminCapability::Usage, days), generation);
    fetchAdminPage(AdminCapability::Cost, adminUrl(AdminCapability::Cost, days),
                   generation);
  } else {
    setCapabilityStatus(QStringLiteral("usage"), QStringLiteral("unavailable"),
                        i18n("An Anthropic Admin API key is not configured"));
    setCapabilityStatus(QStringLiteral("cost"), QStringLiteral("unavailable"),
                        i18n("An Anthropic Admin API key is not configured"));
  }
}

QUrl AnthropicProvider::adminUrl(AdminCapability capability, int days) const {
  const QString path =
      capability == AdminCapability::Usage
          ? QStringLiteral("/organizations/usage_report/messages")
          : QStringLiteral("/organizations/cost_report");
  QUrl url(effectiveBaseUrl(BASE_URL) + path);
  QUrlQuery query;
  const QDate today = QDateTime::currentDateTimeUtc().date();
  const QDateTime start(today.addDays(-qBound(1, days, 31) + 1), QTime(0, 0),
                        QTimeZone::UTC);
  const QDateTime end(today.addDays(1), QTime(0, 0), QTimeZone::UTC);
  query.addQueryItem(QStringLiteral("starting_at"),
                     start.toString(Qt::ISODate));
  query.addQueryItem(QStringLiteral("ending_at"), end.toString(Qt::ISODate));
  query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));
  query.addQueryItem(QStringLiteral("limit"),
                     QString::number(qBound(1, days, 31)));
  query.addQueryItem(QStringLiteral("group_by[]"),
                     QStringLiteral("workspace_id"));
  if (capability == AdminCapability::Usage) {
    query.addQueryItem(QStringLiteral("group_by[]"), QStringLiteral("model"));
    query.addQueryItem(QStringLiteral("group_by[]"),
                       QStringLiteral("service_tier"));
  } else {
    query.addQueryItem(QStringLiteral("group_by[]"),
                       QStringLiteral("description"));
  }
  url.setQuery(query);
  return url;
}

void AnthropicProvider::fetchModels(int generation) {
  QNetworkRequest request = createRequest(
      QUrl(effectiveBaseUrl(BASE_URL) + QStringLiteral("/models")));
  request.setRawHeader("Authorization", QByteArray());
  request.setRawHeader("x-api-key", apiKey().toUtf8());
  request.setRawHeader("anthropic-version", API_VERSION);
  QNetworkReply *reply = networkManager()->get(request);
  trackReply(reply);
  connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
    handleModelsReply(reply, generation);
  });
}

void AnthropicProvider::fetchAdminPage(AdminCapability capability,
                                       const QUrl &url, int generation) {
  if (capability == AdminCapability::Usage)
    ++m_usagePages;
  else
    ++m_costPages;

  QNetworkRequest request = createRequest(url);
  request.setRawHeader("Authorization", QByteArray());
  request.setRawHeader("x-api-key", m_adminApiKey.toUtf8());
  request.setRawHeader("anthropic-version", API_VERSION);
  QNetworkReply *reply = networkManager()->get(request);
  trackReply(reply);
  connect(reply, &QNetworkReply::finished, this,
          [this, capability, reply, generation]() {
            handleAdminReply(capability, reply, generation);
          });
}

void AnthropicProvider::handleModelsReply(QNetworkReply *reply,
                                          int generation) {
  if (!isCurrentGeneration(generation)) {
    reply->deleteLater();
    return;
  }
  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  reply->readAll();
  if (reply->error() == QNetworkReply::NoError) {
    m_connectivitySucceeded = true;
    setCapabilityStatus(QStringLiteral("connectivity"),
                        QStringLiteral("available"));
  } else {
    const ProviderErrorKind kind = errorKindForNetworkReply(reply);
    setCapabilityStatus(QStringLiteral("connectivity"),
                        QStringLiteral("failed"),
                        i18n("The Anthropic Models API is unavailable"));
    if (!hasAdminApiKey())
      setErrorDetails(i18n("Anthropic connection check failed"), kind, status,
                      retryAfterForReply(reply));
  }
  m_connectivityPending = false;
  reply->deleteLater();
  finalizeRefresh(generation);
}

void AnthropicProvider::handleAdminReply(AdminCapability capability,
                                         QNetworkReply *reply, int generation) {
  if (!isCurrentGeneration(generation)) {
    reply->deleteLater();
    return;
  }
  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QByteArray body = reply->readAll();
  if (reply->error() != QNetworkReply::NoError) {
    const ProviderErrorKind kind = errorKindForNetworkReply(reply);
    const QString diagnostic =
        status == 401 ? i18n("The Anthropic Admin API key is invalid")
        : status == 403
            ? i18n("The Anthropic Admin API key lacks the required permission")
        : status == 429
            ? i18n("The Anthropic Admin API rate limit was reached")
            : i18n("The Anthropic Admin reporting endpoint is unavailable");
    reply->deleteLater();
    finishCapability(capability, false, diagnostic, kind, status,
                     retryAfterForReply(reply));
    finalizeRefresh(generation);
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
  QString diagnostic;
  const bool valid =
      parseError.error == QJsonParseError::NoError && document.isObject() &&
      (capability == AdminCapability::Usage
           ? parseUsagePage(document.object(), &m_pendingUsageRows, &diagnostic)
           : parseCostPage(document.object(), &m_pendingCostRows, &diagnostic));
  if (!valid) {
    reply->deleteLater();
    finishCapability(
        capability, false,
        diagnostic.isEmpty()
            ? i18n("The Anthropic Admin response has an unexpected schema")
            : diagnostic,
        ProviderErrorKind::Schema, status);
    finalizeRefresh(generation);
    return;
  }

  const QJsonObject root = document.object();
  const bool hasMore = root.value(QStringLiteral("has_more")).toBool();
  const QString nextPage = root.value(QStringLiteral("next_page")).toString();
  const int pages =
      capability == AdminCapability::Usage ? m_usagePages : m_costPages;
  if (hasMore) {
    if (nextPage.isEmpty() || pages >= MAX_ADMIN_PAGES) {
      reply->deleteLater();
      finishCapability(
          capability, false,
          pages >= MAX_ADMIN_PAGES
              ? i18n("Anthropic pagination exceeded the safety limit")
              : i18n("Anthropic pagination did not provide a next-page token"),
          ProviderErrorKind::Schema, status);
      finalizeRefresh(generation);
      return;
    }
    QUrl nextUrl = reply->url();
    QUrlQuery query(nextUrl);
    query.removeAllQueryItems(QStringLiteral("page"));
    query.addQueryItem(QStringLiteral("page"), nextPage);
    nextUrl.setQuery(query);
    reply->deleteLater();
    fetchAdminPage(capability, nextUrl, generation);
    return;
  }

  reply->deleteLater();
  finishCapability(capability, true);
  finalizeRefresh(generation);
}

void AnthropicProvider::finishCapability(
    AdminCapability capability, bool complete, const QString &diagnostic,
    ProviderErrorKind errorKind, int httpStatus, const QDateTime &retryAfter) {
  if (capability == AdminCapability::Usage) {
    m_usagePending = false;
    m_usageSucceeded = complete;
    m_usagePartial = !complete && !m_pendingUsageRows.isEmpty();
    if (complete) {
      m_lastUsageRows = m_pendingUsageRows;
      publishUsage(false);
    } else if (!m_lastUsageRows.isEmpty()) {
      markProviderMetricsStale(MetricSource::UsageApi, diagnostic);
    } else {
      clearProviderMetric(MetricKind::InputTokens,
                          QStringLiteral("organization"),
                          QStringLiteral("current"));
      clearProviderMetric(MetricKind::OutputTokens,
                          QStringLiteral("organization"),
                          QStringLiteral("current"));
    }
  } else {
    m_costPending = false;
    m_costSucceeded = complete;
    m_costPartial = !complete && !m_pendingCostRows.isEmpty();
    if (complete) {
      m_lastCostRows = m_pendingCostRows;
      publishCost(false);
    } else if (!m_lastCostRows.isEmpty()) {
      markProviderMetricsStale(MetricSource::BillingApi, diagnostic);
    } else {
      clearProviderMetric(MetricKind::Cost, QStringLiteral("organization"),
                          QStringLiteral("current"));
    }
  }

  const QString status =
      complete ? QStringLiteral("available")
               : ((capability == AdminCapability::Usage ? m_usagePartial
                                                        : m_costPartial)
                      ? QStringLiteral("partial")
                      : QStringLiteral("failed"));
  setCapabilityStatus(capabilityName(capability), status, diagnostic);
  if (!complete && errorKind != ProviderErrorKind::None && !m_usageSucceeded &&
      !m_costSucceeded) {
    setErrorDetails(diagnostic, errorKind, httpStatus, retryAfter);
  }
}

void AnthropicProvider::finalizeRefresh(int generation) {
  if (!isCurrentGeneration(generation) || m_connectivityPending ||
      m_usagePending || m_costPending)
    return;

  const bool useful = m_connectivitySucceeded || m_usageSucceeded ||
                      m_costSucceeded || !m_lastUsageRows.isEmpty() ||
                      !m_lastCostRows.isEmpty();
  setConnected(useful);
  if (m_usageSucceeded) {
    setUsageSource(QStringLiteral("actual_api"));
  } else if (m_connectivitySucceeded) {
    setUsageSource(QStringLiteral("model_discovery_api"));
  }
  if (m_costSucceeded)
    setCostSource(QStringLiteral("billing_api"));
  else if (m_connectivitySucceeded && m_lastCostRows.isEmpty())
    setCostSource(QStringLiteral("unknown"));

  if (m_usageSucceeded && m_costSucceeded)
    setDataQuality(QStringLiteral("actual"));
  else if (m_usageSucceeded || m_costSucceeded)
    setDataQuality(QStringLiteral("actual_partial"));
  else if (m_connectivitySucceeded)
    setDataQuality(QStringLiteral("connectivity_only"));
  else if (useful)
    setDataQuality(QStringLiteral("stale"));

  if (useful) {
    updateLastRefreshed();
    if ((m_usageSucceeded || m_costSucceeded) &&
        errorKind() != ProviderErrorKind::None)
      clearError();
  }
  setLoading(false);
  Q_EMIT dataUpdated();
}

void AnthropicProvider::publishUsage(bool stale) {
  Q_UNUSED(stale)
  qint64 input = 0;
  qint64 cacheRead = 0;
  qint64 cacheCreation = 0;
  qint64 output = 0;
  bool prioritySeen = false;
  for (const UsageRow &row : std::as_const(m_lastUsageRows)) {
    input += row.input;
    cacheRead += row.cacheRead;
    cacheCreation += row.cacheCreation;
    output += row.output;
    prioritySeen = prioritySeen || row.serviceTier == QLatin1String("priority");
    const QString scope =
        row.serviceTier.isEmpty()
            ? QStringLiteral("organization")
            : QStringLiteral("organization:") + row.serviceTier;
    setProviderMetric(MetricKind::InputTokens, row.input,
                      QStringLiteral("token"), QString(), scope,
                      QStringLiteral("day"), MetricSource::UsageApi,
                      QStringLiteral("actual"), {}, row.periodStart,
                      row.periodEnd, row.model, row.project);
    setProviderMetric(MetricKind::CacheReadInputTokens, row.cacheRead,
                      QStringLiteral("token"), QString(), scope,
                      QStringLiteral("day"), MetricSource::UsageApi,
                      QStringLiteral("actual"), {}, row.periodStart,
                      row.periodEnd, row.model, row.project);
    setProviderMetric(MetricKind::CacheCreationInputTokens, row.cacheCreation,
                      QStringLiteral("token"), QString(), scope,
                      QStringLiteral("day"), MetricSource::UsageApi,
                      QStringLiteral("actual"), {}, row.periodStart,
                      row.periodEnd, row.model, row.project);
    setProviderMetric(MetricKind::OutputTokens, row.output,
                      QStringLiteral("token"), QString(), scope,
                      QStringLiteral("day"), MetricSource::UsageApi,
                      QStringLiteral("actual"), {}, row.periodStart,
                      row.periodEnd, row.model, row.project);
  }
  setInputTokens(input + cacheRead + cacheCreation);
  setOutputTokens(output);
  setProviderMetric(MetricKind::InputTokens, input + cacheRead + cacheCreation,
                    QStringLiteral("token"), QString(),
                    QStringLiteral("organization"), QStringLiteral("current"),
                    MetricSource::UsageApi, QStringLiteral("actual"));
  setProviderMetric(MetricKind::CacheReadInputTokens, cacheRead,
                    QStringLiteral("token"), QString(),
                    QStringLiteral("organization"), QStringLiteral("current"),
                    MetricSource::UsageApi, QStringLiteral("actual"));
  setProviderMetric(MetricKind::CacheCreationInputTokens, cacheCreation,
                    QStringLiteral("token"), QString(),
                    QStringLiteral("organization"), QStringLiteral("current"),
                    MetricSource::UsageApi, QStringLiteral("actual"));
  setProviderMetric(MetricKind::OutputTokens, output, QStringLiteral("token"),
                    QString(), QStringLiteral("organization"),
                    QStringLiteral("current"), MetricSource::UsageApi,
                    QStringLiteral("actual"));
  if (prioritySeen) {
    clearProviderMetric(MetricKind::Cost,
                        QStringLiteral("organization:priority"),
                        QStringLiteral("day"));
  }
}

void AnthropicProvider::publishCost(bool stale) {
  Q_UNUSED(stale)
  qint64 totalMicroUsd = 0;
  qint64 todayMicroUsd = 0;
  qint64 monthMicroUsd = 0;
  const QDate today = QDateTime::currentDateTimeUtc().date();
  for (const CostRow &row : std::as_const(m_lastCostRows)) {
    totalMicroUsd += row.microUsd;
    if (row.periodStart.date() == today)
      todayMicroUsd += row.microUsd;
    if (row.periodStart.date().year() == today.year() &&
        row.periodStart.date().month() == today.month())
      monthMicroUsd += row.microUsd;
    const QString scope =
        row.serviceTier.isEmpty()
            ? QStringLiteral("organization")
            : QStringLiteral("organization:") + row.serviceTier;
    setProviderMetric(MetricKind::Cost,
                      static_cast<double>(row.microUsd) / 1'000'000.0,
                      QStringLiteral("USD"), QStringLiteral("USD"), scope,
                      QStringLiteral("day"), MetricSource::BillingApi,
                      QStringLiteral("actual"), {}, row.periodStart,
                      row.periodEnd, row.model, row.project);
  }
  const double total = static_cast<double>(totalMicroUsd) / 1'000'000.0;
  const double daily = static_cast<double>(todayMicroUsd) / 1'000'000.0;
  const double monthly = static_cast<double>(monthMicroUsd) / 1'000'000.0;
  setCurrency(QStringLiteral("USD"));
  setCost(total);
  setDailyCost(daily);
  setMonthlyCost(monthly);
  setProviderMetric(MetricKind::Cost, daily, QStringLiteral("USD"),
                    QStringLiteral("USD"), QStringLiteral("organization"),
                    QStringLiteral("day"), MetricSource::BillingApi,
                    QStringLiteral("actual"));
  setProviderMetric(MetricKind::Cost, monthly, QStringLiteral("USD"),
                    QStringLiteral("USD"), QStringLiteral("organization"),
                    QStringLiteral("month"), MetricSource::BillingApi,
                    QStringLiteral("actual"));
}

bool AnthropicProvider::parseUsagePage(const QJsonObject &root,
                                       QList<UsageRow> *rows,
                                       QString *diagnostic) {
  if (!root.value(QStringLiteral("data")).isArray() ||
      !root.value(QStringLiteral("has_more")).isBool()) {
    *diagnostic = i18n("The Anthropic usage report is missing required fields");
    return false;
  }
  QHash<QString, qsizetype> indexes;
  for (qsizetype index = 0; index < rows->size(); ++index) {
    const UsageRow &row = rows->at(index);
    indexes.insert(rowKey(row.periodStart, row.periodEnd, row.model,
                          row.project, row.serviceTier),
                   index);
  }
  for (const QJsonValue &bucketValue :
       root.value(QStringLiteral("data")).toArray()) {
    if (!bucketValue.isObject()) {
      *diagnostic =
          i18n("The Anthropic usage report contains an invalid bucket");
      return false;
    }
    const QJsonObject bucket = bucketValue.toObject();
    const QDateTime start =
        utcDateTime(bucket.value(QStringLiteral("starting_at")));
    const QDateTime end =
        utcDateTime(bucket.value(QStringLiteral("ending_at")));
    if (!start.isValid() || !end.isValid() || end <= start ||
        !bucket.value(QStringLiteral("results")).isArray()) {
      *diagnostic = i18n("The Anthropic usage bucket has an invalid period");
      return false;
    }
    for (const QJsonValue &resultValue :
         bucket.value(QStringLiteral("results")).toArray()) {
      if (!resultValue.isObject()) {
        *diagnostic =
            i18n("The Anthropic usage report contains an invalid result");
        return false;
      }
      const QJsonObject result = resultValue.toObject();
      const auto nonNegativeInteger = [&result](const QString &key,
                                                qint64 *value) {
        if (!result.value(key).isDouble())
          return false;
        *value = result.value(key).toInteger(-1);
        return *value >= 0;
      };
      qint64 uncached = 0;
      qint64 cacheRead = 0;
      qint64 output = 0;
      if (!nonNegativeInteger(QStringLiteral("uncached_input_tokens"),
                              &uncached) ||
          !nonNegativeInteger(QStringLiteral("cache_read_input_tokens"),
                              &cacheRead) ||
          !nonNegativeInteger(QStringLiteral("output_tokens"), &output)) {
        *diagnostic =
            i18n("The Anthropic usage report contains invalid token totals");
        return false;
      }
      qint64 cacheCreation = 0;
      const QJsonObject creation =
          result.value(QStringLiteral("cache_creation")).toObject();
      for (auto it = creation.constBegin(); it != creation.constEnd(); ++it) {
        if (!it.value().isDouble() || it.value().toInteger(-1) < 0) {
          *diagnostic =
              i18n("The Anthropic usage report contains invalid cache totals");
          return false;
        }
        cacheCreation += it.value().toInteger();
      }
      UsageRow row;
      row.periodStart = start;
      row.periodEnd = end;
      row.model = result.value(QStringLiteral("model")).toString();
      row.project = result.value(QStringLiteral("workspace_id")).toString();
      row.serviceTier = result.value(QStringLiteral("service_tier")).toString();
      row.input = uncached;
      row.cacheRead = cacheRead;
      row.cacheCreation = cacheCreation;
      row.output = output;
      const QString key =
          rowKey(start, end, row.model, row.project, row.serviceTier);
      if (indexes.contains(key)) {
        UsageRow &existing = (*rows)[indexes.value(key)];
        existing.input += row.input;
        existing.cacheRead += row.cacheRead;
        existing.cacheCreation += row.cacheCreation;
        existing.output += row.output;
      } else {
        indexes.insert(key, rows->size());
        rows->append(row);
      }
    }
  }
  return true;
}

bool AnthropicProvider::parseCostPage(const QJsonObject &root,
                                      QList<CostRow> *rows,
                                      QString *diagnostic) {
  if (!root.value(QStringLiteral("data")).isArray() ||
      !root.value(QStringLiteral("has_more")).isBool()) {
    *diagnostic = i18n("The Anthropic cost report is missing required fields");
    return false;
  }
  QHash<QString, qsizetype> indexes;
  for (qsizetype index = 0; index < rows->size(); ++index) {
    const CostRow &row = rows->at(index);
    indexes.insert(rowKey(row.periodStart, row.periodEnd, row.model,
                          row.project, row.serviceTier),
                   index);
  }
  for (const QJsonValue &bucketValue :
       root.value(QStringLiteral("data")).toArray()) {
    if (!bucketValue.isObject()) {
      *diagnostic =
          i18n("The Anthropic cost report contains an invalid bucket");
      return false;
    }
    const QJsonObject bucket = bucketValue.toObject();
    const QDateTime start =
        utcDateTime(bucket.value(QStringLiteral("starting_at")));
    const QDateTime end =
        utcDateTime(bucket.value(QStringLiteral("ending_at")));
    if (!start.isValid() || !end.isValid() || end <= start ||
        !bucket.value(QStringLiteral("results")).isArray()) {
      *diagnostic = i18n("The Anthropic cost bucket has an invalid period");
      return false;
    }
    for (const QJsonValue &resultValue :
         bucket.value(QStringLiteral("results")).toArray()) {
      const QJsonObject result = resultValue.toObject();
      qint64 microUsd = 0;
      if (!resultValue.isObject() ||
          result.value(QStringLiteral("currency")).toString().toUpper() !=
              QLatin1String("USD") ||
          !result.value(QStringLiteral("amount")).isString() ||
          !parseMicroUsd(result.value(QStringLiteral("amount")).toString(),
                         &microUsd)) {
        *diagnostic =
            i18n("The Anthropic cost report contains an invalid amount");
        return false;
      }
      CostRow row;
      row.periodStart = start;
      row.periodEnd = end;
      row.model = result.value(QStringLiteral("model")).toString();
      row.project = result.value(QStringLiteral("workspace_id")).toString();
      row.serviceTier = result.value(QStringLiteral("service_tier")).toString();
      row.microUsd = microUsd;
      const QString key =
          rowKey(start, end, row.model, row.project, row.serviceTier);
      if (indexes.contains(key))
        (*rows)[indexes.value(key)].microUsd += microUsd;
      else {
        indexes.insert(key, rows->size());
        rows->append(row);
      }
    }
  }
  return true;
}

bool AnthropicProvider::parseMicroUsd(const QString &fractionalCents,
                                      qint64 *microUsd) {
  static const QRegularExpression pattern(
      QStringLiteral("^([+-]?)([0-9]+)(?:\\.([0-9]+))?$"));
  const QRegularExpressionMatch match =
      pattern.match(fractionalCents.trimmed());
  if (!match.hasMatch())
    return false;
  bool ok = false;
  const quint64 whole = match.captured(2).toULongLong(&ok);
  if (!ok ||
      whole > static_cast<quint64>(std::numeric_limits<qint64>::max() / 10'000))
    return false;
  QString fraction = match.captured(3);
  const bool roundUp =
      fraction.size() > 4 && fraction.at(4) >= QLatin1Char('5');
  fraction = fraction.left(4).leftJustified(4, QLatin1Char('0'));
  quint64 value = whole * 10'000;
  value += fraction.isEmpty() ? 0 : fraction.toULongLong(&ok);
  if (!ok || (roundUp && value == static_cast<quint64>(
                                      std::numeric_limits<qint64>::max())))
    return false;
  if (roundUp)
    ++value;
  if (value > static_cast<quint64>(std::numeric_limits<qint64>::max()))
    return false;
  *microUsd = match.captured(1) == QLatin1String("-")
                  ? -static_cast<qint64>(value)
                  : static_cast<qint64>(value);
  return true;
}

void AnthropicProvider::countTokensDiagnostic() {
  if (!hasApiKey()) {
    setErrorDetails(i18n("No standard Anthropic API key configured"),
                    ProviderErrorKind::Configuration);
    return;
  }
  beginRefresh();
  setLoading(true);
  clearError();
  fetchRateLimits(currentGeneration());
}

void AnthropicProvider::fetchRateLimits(int generation) {
  QNetworkRequest request = createRequest(QUrl(
      effectiveBaseUrl(BASE_URL) + QStringLiteral("/messages/count_tokens")));
  request.setRawHeader("Authorization", QByteArray());
  request.setRawHeader("x-api-key", apiKey().toUtf8());
  request.setRawHeader("anthropic-version", API_VERSION);
  const QJsonObject payload{
      {QStringLiteral("model"), m_model},
      {QStringLiteral("messages"),
       QJsonArray{
           QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                       {QStringLiteral("content"), QStringLiteral("hi")}}}}};
  QNetworkReply *reply = networkManager()->post(
      request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
  trackReply(reply);
  connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
    handleCountTokensReply(reply, generation);
  });
}

void AnthropicProvider::handleCountTokensReply(QNetworkReply *reply,
                                               int generation) {
  if (!isCurrentGeneration(generation)) {
    reply->deleteLater();
    return;
  }
  if (reply->error() != QNetworkReply::NoError) {
    setNetworkError(reply, i18n("Anthropic rate-limit diagnostic failed"));
    reply->deleteLater();
    setLoading(false);
    return;
  }
  const auto integerHeader = [reply](const char *name,
                                     bool *present = nullptr) {
    const QByteArray value = reply->rawHeader(name);
    if (present)
      *present = !value.isEmpty();
    return value.toInt();
  };
  bool hasRequestsRemaining = false;
  const int requestLimit = integerHeader("anthropic-ratelimit-requests-limit");
  const int requestsRemaining = integerHeader(
      "anthropic-ratelimit-requests-remaining", &hasRequestsRemaining);
  if (requestLimit > 0 && hasRequestsRemaining) {
    setRateLimitRequests(requestLimit);
    setRateLimitRequestsRemaining(requestsRemaining);
  }
  bool hasInputRemaining = false;
  bool hasOutputRemaining = false;
  const int inputLimit =
      integerHeader("anthropic-ratelimit-input-tokens-limit");
  const int inputRemaining = integerHeader(
      "anthropic-ratelimit-input-tokens-remaining", &hasInputRemaining);
  const int outputLimit =
      integerHeader("anthropic-ratelimit-output-tokens-limit");
  const int outputRemaining = integerHeader(
      "anthropic-ratelimit-output-tokens-remaining", &hasOutputRemaining);
  if (inputLimit + outputLimit > 0 && hasInputRemaining && hasOutputRemaining) {
    setRateLimitTokens(inputLimit + outputLimit);
    setRateLimitTokensRemaining(inputRemaining + outputRemaining);
  }
  const QString reset =
      QString::fromUtf8(reply->rawHeader("anthropic-ratelimit-requests-reset"));
  if (!reset.isEmpty())
    setRateLimitResetTime(reset);
  setConnected(true);
  setUsageSource(QStringLiteral("connectivity_probe"));
  setDataQuality(QStringLiteral("rate_limit_only"));
  setCapabilityStatus(QStringLiteral("manual_rate_limits"),
                      QStringLiteral("available"));
  reply->deleteLater();
  updateLastRefreshed();
  setLoading(false);
  Q_EMIT dataUpdated();
}
