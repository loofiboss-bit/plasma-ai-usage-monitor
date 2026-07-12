#include "refreshschedulermodel.h"

#include <QtGlobal>
#include <cmath>

RefreshSchedulerModel::RefreshSchedulerModel(QObject *parent)
    : QObject(parent)
{
}

int RefreshSchedulerModel::deterministicJitterMs(const QString &providerKey) const
{
    int hash = 0;
    for (qsizetype i = 0; i < providerKey.size(); ++i) {
        hash = (hash + providerKey.at(i).unicode() * static_cast<int>(i + 1)) % 997;
    }
    return hash * 37;
}

int RefreshSchedulerModel::effectiveIntervalMs(int providerSeconds,
                                               int globalSeconds,
                                               bool popupOpen) const
{
    int seconds = providerSeconds > 0 ? providerSeconds : qMax(1, globalSeconds);
    if (!popupOpen) {
        seconds = qMax(seconds * 4, 900);
    }
    return seconds * 1000;
}

double RefreshSchedulerModel::backoffMultiplier(int consecutiveErrors, bool retryable) const
{
    if (consecutiveErrors <= 0) return 1.0;
    return retryable
        ? qMin(8.0, std::pow(2.0, qMin(consecutiveErrors, 3)))
        : qMin(4.0, static_cast<double>(consecutiveErrors + 1));
}

int RefreshSchedulerModel::scheduledIntervalMs(const QString &providerKey,
                                               int providerSeconds,
                                               int globalSeconds,
                                               bool popupOpen,
                                               int consecutiveErrors,
                                               bool retryable) const
{
    return static_cast<int>(effectiveIntervalMs(providerSeconds, globalSeconds, popupOpen)
                            * backoffMultiplier(consecutiveErrors, retryable))
        + deterministicJitterMs(providerKey);
}

bool RefreshSchedulerModel::isFresh(const QDateTime &lastSuccess,
                                    int providerSeconds,
                                    int globalSeconds,
                                    bool popupOpen,
                                    const QDateTime &now) const
{
    if (!lastSuccess.isValid() || !now.isValid()) return false;
    return lastSuccess.toUTC().msecsTo(now.toUTC())
        < effectiveIntervalMs(providerSeconds, globalSeconds, popupOpen);
}

QDateTime RefreshSchedulerModel::nextScheduledRefresh(const QDateTime &lastSuccess,
                                                      const QString &providerKey,
                                                      int providerSeconds,
                                                      int globalSeconds,
                                                      bool popupOpen,
                                                      int consecutiveErrors,
                                                      bool retryable) const
{
    if (!lastSuccess.isValid()) return {};
    return lastSuccess.toUTC().addMSecs(scheduledIntervalMs(
        providerKey, providerSeconds, globalSeconds, popupOpen, consecutiveErrors, retryable));
}
