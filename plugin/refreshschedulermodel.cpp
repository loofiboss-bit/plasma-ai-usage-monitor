#include "refreshschedulermodel.h"

#include <QNetworkInformation>
#include <QtGlobal>
#include <cmath>

RefreshSchedulerModel::RefreshSchedulerModel(QObject *parent)
    : QObject(parent)
{
  m_recoveryTimer.setSingleShot(true);
  m_recoveryTimer.setInterval(1000);
  connect(&m_recoveryTimer, &QTimer::timeout, this,
          &RefreshSchedulerModel::recoveryRequested);
  m_wakeTimer.setInterval(30000);
  connect(&m_wakeTimer, &QTimer::timeout, this,
          [this]() { observeWakeClock(QDateTime::currentDateTimeUtc()); });
}

void RefreshSchedulerModel::startMonitoring() {
  if (m_monitoring)
    return;
  m_monitoring = true;
  observeWakeClock(QDateTime::currentDateTimeUtc());
  m_wakeTimer.start();
  QNetworkInformation::loadDefaultBackend();
  if (auto *network = QNetworkInformation::instance()) {
    const auto initialReachability = network->reachability();
    m_networkStateKnown = initialReachability !=
                          QNetworkInformation::Reachability::Unknown;
    m_externalRefreshAllowed = !m_networkStateKnown ||
        initialReachability != QNetworkInformation::Reachability::Disconnected;
    m_seenOffline = m_networkStateKnown && !m_externalRefreshAllowed;
    Q_EMIT reachabilityChanged();
    connect(network, &QNetworkInformation::reachabilityChanged, this,
            [this](QNetworkInformation::Reachability reachability) {
              const bool known = reachability !=
                  QNetworkInformation::Reachability::Unknown;
              const bool externalAllowed = !known || reachability !=
                  QNetworkInformation::Reachability::Disconnected;
              const bool changed = m_networkStateKnown != known ||
                                   m_externalRefreshAllowed != externalAllowed;
              m_networkStateKnown = known;
              m_externalRefreshAllowed = externalAllowed;
              if (changed)
                Q_EMIT reachabilityChanged();
              if (!known)
                return;
              observeReachability(externalAllowed);
            });
  }
}

void RefreshSchedulerModel::observeWakeClock(const QDateTime &now) {
  if (!now.isValid())
    return;
  // A missed timer interval covers suspend/resume without touching login
  // services.
  if (m_lastWakeCheck.isValid() && m_lastWakeCheck.secsTo(now) > 90 &&
      !m_recoveryTimer.isActive())
    m_recoveryTimer.start();
  m_lastWakeCheck = now;
}

void RefreshSchedulerModel::observeReachability(bool online) {
  const bool changed = !m_networkStateKnown ||
                       m_externalRefreshAllowed != online;
  m_networkStateKnown = true;
  m_externalRefreshAllowed = online;
  if (changed)
    Q_EMIT reachabilityChanged();
  if (!online) {
    m_seenOffline = true;
    return;
  }
  if (m_seenOffline && !m_recoveryTimer.isActive())
    m_recoveryTimer.start();
  m_seenOffline = false;
}

bool RefreshSchedulerModel::networkStateKnown() const {
  return m_networkStateKnown;
}

bool RefreshSchedulerModel::externalRefreshAllowed() const {
  return m_externalRefreshAllowed;
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
    return lastSuccess <= now &&
           lastSuccess.toUTC().msecsTo(now.toUTC()) <
               effectiveIntervalMs(providerSeconds, globalSeconds, popupOpen);
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
