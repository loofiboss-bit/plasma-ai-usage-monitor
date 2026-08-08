#include "budgetobservationquery.h"

#include "currencyminorunits.h"

#include <QSqlQuery>
#include <QTimeZone>

namespace {
QDateTime utcDateTime(const QVariant &value) {
  if (value.metaType().id() == QMetaType::QString) {
    QDateTime parsed = QDateTime::fromString(
        value.toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (parsed.isValid())
      parsed.setTimeZone(QTimeZone::UTC);
    if (!parsed.isValid())
      parsed = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!parsed.isValid())
      parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    return parsed.toUTC();
  }
  return value.toDateTime().toUTC();
}

ForecastContract::ValueClass valueClassFor(const QString &semantic,
                                           const QString &source,
                                           const QString &quality) {
  const QString normalizedSource = source.trimmed().toLower();
  const QString normalizedQuality = quality.trimmed().toLower();
  if (semantic == QLatin1String("local_estimate") ||
      normalizedSource.contains(QStringLiteral("estimated")) ||
      normalizedSource == QLatin1String("self_tracked") ||
      normalizedSource == QLatin1String("browser_sync") ||
      normalizedQuality.contains(QStringLiteral("estimated")))
    return ForecastContract::ValueClass::Estimated;
  return ForecastContract::ValueClass::Actual;
}

QString dimensionFromScope(const QString &scope, const QString &kind) {
  const QString prefix =
      QStringLiteral("organization_scoped:") + kind + QLatin1Char(':');
  return scope.startsWith(prefix) ? scope.mid(prefix.size()) : QString();
}
} // namespace

BudgetPacingQuery::Request
BudgetObservationQuery::requestFromPolicy(const QVariantMap &policy,
                                          const QDateTime &generatedAt,
                                          const QSqlDatabase &database) {
  BudgetPacingQuery::Request request;
  request.policyId = policy.value(QStringLiteral("policyId")).toString();
  request.sourceId = policy.value(QStringLiteral("sourceId")).toString();
  request.sourceKind = policy.value(QStringLiteral("sourceKind")).toString();
  request.scopeMode =
      policy.value(QStringLiteral("scopeMode"), QStringLiteral("aggregate"))
          .toString();
  request.scopeKind = policy.value(QStringLiteral("scopeKind")).toString();
  request.scopeIdentity =
      policy.value(QStringLiteral("scopeIdentity")).toString();
  request.scopeLabel = policy.value(QStringLiteral("scopeLabel")).toString();
  request.valueClass = policy.value(QStringLiteral("valueClass")).toString() ==
                               QLatin1String("estimated")
                           ? ForecastContract::ValueClass::Estimated
                           : ForecastContract::ValueClass::Actual;
  request.limitMinor = policy.value(QStringLiteral("limitMinor")).toLongLong();
  request.currency = policy.value(QStringLiteral("currency")).toString();
  request.warningPercent =
      policy.value(QStringLiteral("warningPercent"), 80).toInt();
  request.criticalPercent =
      policy.value(QStringLiteral("criticalPercent"), 90).toInt();
  request.timeZoneId = policy.value(QStringLiteral("timeZoneId")).toString();
  request.generatedAt = generatedAt.toUTC();

  const QString requestedDimension =
      request.scopeMode == QLatin1String("aggregate")
          ? QStringLiteral("aggregate")
          : request.scopeKind;
  QStringList supportedScopes =
      policy.value(QStringLiteral("catalogSupportedScopes")).toStringList();
  if (supportedScopes.isEmpty())
    supportedScopes.append(QStringLiteral("aggregate"));
  if (!supportedScopes.contains(requestedDimension)) {
    request.preflightReason = QStringLiteral("scope-unavailable");
    return request;
  }
  if (request.sourceId == QLatin1String("litellm") &&
      requestedDimension != QLatin1String("aggregate") &&
      !policy.value(QStringLiteral("validatedGatewayScopes"))
           .toStringList()
           .contains(requestedDimension)) {
    request.preflightReason = QStringLiteral("scope-unavailable");
    return request;
  }

  BillingCycleResolver::Request cycleRequest;
  cycleRequest.periodType =
      policy.value(QStringLiteral("periodType")).toString();
  cycleRequest.anchorDay = policy.value(QStringLiteral("anchorDay")).toInt();
  cycleRequest.timeZoneId = request.timeZoneId;
  cycleRequest.generatedAt = request.generatedAt;
  cycleRequest.providerPeriodStart =
      policy.value(QStringLiteral("providerPeriodStartUtc")).toDateTime();
  cycleRequest.providerResetAt =
      policy.value(QStringLiteral("providerResetUtc")).toDateTime();
  cycleRequest.providerResetStable =
      policy.value(QStringLiteral("providerResetStable")).toBool();
  cycleRequest.providerResetAuthenticated =
      policy.value(QStringLiteral("providerResetAuthenticated")).toBool();
  cycleRequest.catalogSupportsProviderReset =
      policy.value(QStringLiteral("catalogSupportsProviderReset")).toBool();
  request.cycle = BillingCycleResolver::resolve(cycleRequest);
  request.previousCycle =
      BillingCycleResolver::previous(cycleRequest, request.cycle);
  if (!request.cycle.isValid()) {
    request.preflightReason = request.cycle.reasonKey;
    return request;
  }
  if (!database.isOpen()) {
    request.preflightReason = QStringLiteral("query-failed");
    return request;
  }

  const QDateTime earliest = request.previousCycle.isValid()
                                 ? qMin(request.previousCycle.startUtc,
                                        request.cycle.startUtc.addDays(-45))
                                 : request.cycle.startUtc.addDays(-45);
  QSqlQuery query(database);
  query.setForwardOnly(true);
  query.prepare(QStringLiteral(
      "SELECT "
      "observed_at_utc,interval_start_utc,interval_end_utc,value,currency,"
      "semantic,source,data_quality,"
      "COALESCE(model_scope,''),COALESCE(project_scope,''),scope,"
      "COALESCE(service_tier_scope,''),COALESCE(line_item_scope,'') FROM "
      "observations "
      "WHERE provider=? AND metric_kind='cost' AND window='day' "
      "AND interval_start_utc>=? AND interval_end_utc<=? ORDER BY "
      "observed_at_utc,id"));
  query.addBindValue(
      policy.value(QStringLiteral("provider"), request.sourceId));
  query.addBindValue(earliest.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
  query.addBindValue(
      request.cycle.endUtc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
  if (!query.exec()) {
    request.preflightReason = QStringLiteral("query-failed");
    return request;
  }
  while (query.next()) {
    BudgetPacingQuery::Observation observation;
    observation.observedAt = utcDateTime(query.value(0));
    observation.intervalStart = utcDateTime(query.value(1));
    observation.intervalEnd = utcDateTime(query.value(2));
    observation.currency = query.value(4).toString().trimmed().toUpper();
    if (!query.value(3).isNull()) {
      const std::optional<qint64> minor = CurrencyMinorUnits::fromMajor(
          query.value(3).toDouble(), observation.currency);
      if (!minor) {
        request.preflightReason = QStringLiteral("unknown-currency");
        return request;
      }
      observation.valueMinor = *minor;
    }
    observation.valueClass =
        valueClassFor(query.value(5).toString(), query.value(6).toString(),
                      query.value(7).toString());
    const QString modelScope = query.value(8).toString();
    const QString projectScope = query.value(9).toString();
    const QString observationScope = query.value(10).toString();
    const QString serviceTier =
        query.value(11).toString().isEmpty()
            ? dimensionFromScope(observationScope,
                                 QStringLiteral("service_tier"))
            : query.value(11).toString();
    const QString lineItem =
        query.value(12).toString().isEmpty()
            ? dimensionFromScope(observationScope, QStringLiteral("line_item"))
            : query.value(12).toString();
    if (request.scopeMode == QLatin1String("scoped")) {
      observation.scopeKind = request.scopeKind;
      if (request.scopeKind == QLatin1String("project"))
        observation.scopeIdentity = projectScope;
      else if (request.scopeKind == QLatin1String("workspace"))
        observation.scopeIdentity = projectScope;
      else if (request.scopeKind == QLatin1String("model"))
        observation.scopeIdentity = modelScope;
      else if (request.scopeKind == QLatin1String("service_tier"))
        observation.scopeIdentity = serviceTier;
      else if (request.scopeKind == QLatin1String("line_item"))
        observation.scopeIdentity = lineItem;
    } else if (!serviceTier.isEmpty()) {
      observation.scopeKind = QStringLiteral("service_tier");
      observation.scopeIdentity = serviceTier;
    } else if (!lineItem.isEmpty()) {
      observation.scopeKind = QStringLiteral("line_item");
      observation.scopeIdentity = lineItem;
    } else if (!projectScope.isEmpty()) {
      observation.scopeKind = QStringLiteral("project");
      observation.scopeIdentity = projectScope;
    } else if (!modelScope.isEmpty()) {
      observation.scopeKind = QStringLiteral("model");
      observation.scopeIdentity = modelScope;
    } else if (observationScope !=
               policy.value(QStringLiteral("observationScope"),
                            QStringLiteral("organization"))) {
      observation.scopeKind = QStringLiteral("provider_scope");
      observation.scopeIdentity = observationScope;
    }
    request.observations.append(observation);
  }
  return request;
}
