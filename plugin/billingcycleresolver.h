#ifndef BILLINGCYCLERESOLVER_H
#define BILLINGCYCLERESOLVER_H

#include <QDateTime>
#include <QString>
#include <QVariantMap>

class BillingCycleResolver final {
public:
  struct Request {
    QString periodType;
    int anchorDay = 0;
    QString timeZoneId;
    QDateTime generatedAt;
    QDateTime providerPeriodStart;
    QDateTime providerResetAt;
    bool providerResetStable = false;
    bool providerResetAuthenticated = false;
    bool catalogSupportsProviderReset = false;
  };

  struct Cycle {
    QDateTime startUtc;
    QDateTime endUtc;
    QString reasonKey;

    bool isValid() const;
    QVariantMap toVariantMap() const;
  };

  static Cycle resolve(const Request &request);
  static Cycle previous(const Request &request, const Cycle &current);
};

#endif
