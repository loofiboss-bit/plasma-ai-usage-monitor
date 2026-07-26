#include "anthropicadminparser.h"

#include <KLocalizedString>
#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
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
} // namespace

bool AnthropicAdminParser::parseUsagePage(const QJsonObject &root,
                                          QList<UsageRow> *rows,
                                          QString *diagnostic) {
  if (rows == nullptr || diagnostic == nullptr ||
      !root.value(QStringLiteral("data")).isArray() ||
      !root.value(QStringLiteral("has_more")).isBool()) {
    if (diagnostic != nullptr)
      *diagnostic =
          i18n("The Anthropic usage report is missing required fields");
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

bool AnthropicAdminParser::parseCostPage(const QJsonObject &root,
                                         QList<CostRow> *rows,
                                         QString *diagnostic) {
  if (rows == nullptr || diagnostic == nullptr ||
      !root.value(QStringLiteral("data")).isArray() ||
      !root.value(QStringLiteral("has_more")).isBool()) {
    if (diagnostic != nullptr)
      *diagnostic =
          i18n("The Anthropic cost report is missing required fields");
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

bool AnthropicAdminParser::parseMicroUsd(const QString &fractionalCents,
                                         qint64 *microUsd) {
  if (microUsd == nullptr)
    return false;
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

AnthropicAdminParser::Pagination
AnthropicAdminParser::pagination(const QJsonObject &root,
                                 const QUrl &currentUrl, int pages,
                                 int maximumPages) {
  Pagination result;
  if (!root.value(QStringLiteral("has_more")).isBool()) {
    result.diagnostic =
        i18n("The Anthropic Admin response is missing pagination state");
    return result;
  }
  result.valid = true;
  result.complete = !root.value(QStringLiteral("has_more")).toBool();
  if (result.complete)
    return result;

  const QString nextPage = root.value(QStringLiteral("next_page")).toString();
  if (nextPage.isEmpty()) {
    result.valid = false;
    result.diagnostic =
        i18n("Anthropic pagination did not provide a next-page token");
    return result;
  }
  if (pages >= maximumPages) {
    result.valid = false;
    result.diagnostic = i18n("Anthropic pagination exceeded the safety limit");
    return result;
  }
  result.nextUrl = currentUrl;
  QUrlQuery query(result.nextUrl);
  query.removeAllQueryItems(QStringLiteral("page"));
  query.addQueryItem(QStringLiteral("page"), nextPage);
  result.nextUrl.setQuery(query);
  return result;
}
