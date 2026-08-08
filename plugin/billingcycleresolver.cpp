#include "billingcycleresolver.h"

#include <QTimeZone>

namespace {
QDateTime localBoundary(const QDate &date, const QTimeZone &zone) {
  return QDateTime(date, QTime(0, 0), zone).toUTC();
}

BillingCycleResolver::Cycle invalid(const QString &reason) {
  return {{}, {}, reason};
}
} // namespace

bool BillingCycleResolver::Cycle::isValid() const {
  return reasonKey.isEmpty() && startUtc.isValid() && endUtc.isValid() &&
         startUtc < endUtc && startUtc.timeSpec() == Qt::UTC &&
         endUtc.timeSpec() == Qt::UTC;
}

QVariantMap BillingCycleResolver::Cycle::toVariantMap() const {
  return {{QStringLiteral("ok"), isValid()},
          {QStringLiteral("startUtc"), startUtc},
          {QStringLiteral("endUtc"), endUtc},
          {QStringLiteral("reasonKey"), reasonKey}};
}

BillingCycleResolver::Cycle
BillingCycleResolver::resolve(const Request &request) {
  const QDateTime now = request.generatedAt.isValid()
                            ? request.generatedAt.toUTC()
                            : QDateTime::currentDateTimeUtc();
  if (request.periodType == QLatin1String("provider_reset")) {
    if (!request.catalogSupportsProviderReset ||
        !request.providerResetAuthenticated || !request.providerResetStable ||
        !request.providerPeriodStart.isValid() ||
        !request.providerResetAt.isValid()) {
      return invalid(QStringLiteral("unstable-reset"));
    }
    const QDateTime start = request.providerPeriodStart.toUTC();
    const QDateTime end = request.providerResetAt.toUTC();
    if (start >= end || now < start || now >= end) {
      return invalid(QStringLiteral("unstable-reset"));
    }
    return {start, end, {}};
  }

  const QTimeZone zone(request.timeZoneId.toUtf8());
  if (!zone.isValid()) {
    return invalid(QStringLiteral("invalid-policy"));
  }
  const QDate localDate = now.toTimeZone(zone).date();
  QDate startDate;
  QDate endDate;
  if (request.periodType == QLatin1String("calendar_day")) {
    startDate = localDate;
    endDate = localDate.addDays(1);
  } else if (request.periodType == QLatin1String("iso_week")) {
    startDate = localDate.addDays(1 - localDate.dayOfWeek());
    endDate = startDate.addDays(7);
  } else if (request.periodType == QLatin1String("calendar_month")) {
    startDate = QDate(localDate.year(), localDate.month(), 1);
    endDate = startDate.addMonths(1);
  } else if (request.periodType == QLatin1String("anchored_month")) {
    if (request.anchorDay < 1 || request.anchorDay > 28) {
      return invalid(QStringLiteral("invalid-policy"));
    }
    const QDate currentAnchor(localDate.year(), localDate.month(),
                              request.anchorDay);
    startDate = localDate >= currentAnchor ? currentAnchor
                                           : currentAnchor.addMonths(-1);
    endDate = startDate.addMonths(1);
  } else {
    return invalid(QStringLiteral("invalid-policy"));
  }
  const QDateTime start = localBoundary(startDate, zone);
  const QDateTime end = localBoundary(endDate, zone);
  if (!start.isValid() || !end.isValid() || now < start || now >= end) {
    return invalid(QStringLiteral("invalid-policy"));
  }
  return {start, end, {}};
}

BillingCycleResolver::Cycle
BillingCycleResolver::previous(const Request &request, const Cycle &current) {
  if (!current.isValid()) {
    return invalid(current.reasonKey.isEmpty()
                       ? QStringLiteral("invalid-policy")
                       : current.reasonKey);
  }
  if (request.periodType == QLatin1String("provider_reset")) {
    return invalid(QStringLiteral("unstable-reset"));
  }
  Request previousRequest = request;
  previousRequest.generatedAt = current.startUtc.addMSecs(-1);
  return resolve(previousRequest);
}
