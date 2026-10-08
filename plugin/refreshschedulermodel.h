#ifndef REFRESHSCHEDULERMODEL_H
#define REFRESHSCHEDULERMODEL_H

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QtQmlIntegration/qqmlintegration.h>

class RefreshSchedulerModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool networkStateKnown READ networkStateKnown NOTIFY reachabilityChanged)
    Q_PROPERTY(bool externalRefreshAllowed READ externalRefreshAllowed NOTIFY reachabilityChanged)
public:
    explicit RefreshSchedulerModel(QObject *parent = nullptr);

    Q_INVOKABLE void startMonitoring();
    void observeWakeClock(const QDateTime &now);
    Q_INVOKABLE void observeReachability(bool online);
    bool networkStateKnown() const;
    bool externalRefreshAllowed() const;

    Q_INVOKABLE int deterministicJitterMs(const QString &providerKey) const;
    Q_INVOKABLE int effectiveIntervalMs(int providerSeconds,
                                        int globalSeconds,
                                        bool popupOpen) const;
    Q_INVOKABLE double backoffMultiplier(int consecutiveErrors, bool retryable) const;
    Q_INVOKABLE int scheduledIntervalMs(const QString &providerKey,
                                        int providerSeconds,
                                        int globalSeconds,
                                        bool popupOpen,
                                        int consecutiveErrors,
                                        bool retryable) const;
    Q_INVOKABLE bool isFresh(const QDateTime &lastSuccess,
                             int providerSeconds,
                             int globalSeconds,
                             bool popupOpen,
                             const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    Q_INVOKABLE QDateTime nextScheduledRefresh(const QDateTime &lastSuccess,
                                               const QString &providerKey,
                                               int providerSeconds,
                                               int globalSeconds,
                                               bool popupOpen,
                                               int consecutiveErrors,
                                               bool retryable) const;
  Q_SIGNALS:
    void recoveryRequested();
    void reachabilityChanged();

  private:
    QTimer m_wakeTimer;
    QTimer m_recoveryTimer;
    QDateTime m_lastWakeCheck;
    bool m_seenOffline = false;
    bool m_networkStateKnown = false;
    bool m_externalRefreshAllowed = true;
    bool m_monitoring = false;
};

#endif
