#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVersionNumber>
#include <QtQmlIntegration/qqmlintegration.h>

/**
 * Periodically checks GitHub releases for newer versions and emits
 * updateAvailable() so QML can fire a KDE notification.
 *
 * Usage from QML:
 *   UpdateChecker {
 *       currentVersion: "2.1.0"
 *       checkIntervalHours: 12
 *       onUpdateAvailable: (latestVersion, releaseUrl) => { ... }
 *   }
 */
class UpdateChecker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString currentVersion READ currentVersion WRITE setCurrentVersion NOTIFY currentVersionChanged)
    Q_PROPERTY(int checkIntervalHours READ checkIntervalHours WRITE setCheckIntervalHours NOTIFY checkIntervalHoursChanged)
    Q_PROPERTY(QString releaseApiUrl READ releaseApiUrl WRITE setReleaseApiUrl NOTIFY releaseApiUrlChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // ── Properties ──

    QString currentVersion() const;
    void setCurrentVersion(const QString &v);

    int checkIntervalHours() const;
    void setCheckIntervalHours(int h);

    QString releaseApiUrl() const;
    void setReleaseApiUrl(const QString &url);

    bool checking() const;
    QString latestVersion() const;

    /// Trigger a manual check (callable from QML)
    Q_INVOKABLE void checkForUpdate();

Q_SIGNALS:
    void currentVersionChanged();
    void checkIntervalHoursChanged();
    void releaseApiUrlChanged();
    void checkingChanged();
    void latestVersionChanged();
    void updateAvailable(const QString &latestVersion, const QString &releaseUrl);

private:
    void startTimerIfReady();

    QNetworkAccessManager *m_nam = nullptr;
    QTimer *m_timer = nullptr;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseApiUrl = QStringLiteral("https://api.github.com/repos/loofiboss-bit/plasma-ai-usage-monitor/releases/latest");
    int m_intervalHours = 12;
    bool m_checking = false;
};

#endif // UPDATECHECKER_H
