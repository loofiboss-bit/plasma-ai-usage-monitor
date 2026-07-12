#ifndef BROWSERSYNCSERVICE_H
#define BROWSERSYNCSERVICE_H

#include "browsercookieextractor.h"

#include <QObject>
#include <QPointer>

class BrowserSyncService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int browserType READ browserType WRITE setBrowserType NOTIFY configurationChanged)
    Q_PROPERTY(QString selectedFirefoxProfile READ selectedFirefoxProfile WRITE setSelectedFirefoxProfile NOTIFY configurationChanged)
    Q_PROPERTY(bool hasFirefoxProfile READ hasFirefoxProfile NOTIFY readinessChanged)
    Q_PROPERTY(bool hasCurrentBrowserProfile READ hasCurrentBrowserProfile NOTIFY readinessChanged)
    Q_PROPERTY(bool hasSafeStorageAccess READ hasSafeStorageAccess NOTIFY readinessChanged)
    Q_PROPERTY(QString readinessSummary READ readinessSummary NOTIFY readinessChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastDiagnostic READ lastDiagnostic NOTIFY stateChanged)
    Q_PROPERTY(bool circuitOpen READ circuitOpen NOTIFY stateChanged)

public:
    explicit BrowserSyncService(QObject *parent = nullptr);

    int browserType() const;
    void setBrowserType(int type);
    QString selectedFirefoxProfile() const;
    void setSelectedFirefoxProfile(const QString &profile);
    bool hasFirefoxProfile() const;
    bool hasCurrentBrowserProfile() const;
    bool hasSafeStorageAccess() const;
    QString readinessSummary() const;
    QString state() const;
    QString lastDiagnostic() const;
    bool circuitOpen() const;

    Q_INVOKABLE QStringList firefoxProfiles() const;
    Q_INVOKABLE QStringList browserProfiles() const;
    Q_INVOKABLE QVariantMap readinessReport(const QString &service = QString()) const;
    Q_INVOKABLE QString testConnection(const QString &service) const;
    Q_INVOKABLE QString connectionMessage(const QString &service, const QString &code) const;
    Q_INVOKABLE bool sync(const QString &service, QObject *monitor);
    Q_INVOKABLE void resetCircuit();

Q_SIGNALS:
    void configurationChanged();
    void readinessChanged();
    void stateChanged();

private:
    QString domainForService(const QString &service) const;
    void recordFailure(const QString &diagnostic);

    BrowserCookieExtractor m_extractor;
    QString m_state = QStringLiteral("idle");
    QString m_lastDiagnostic;
    int m_consecutiveFailures = 0;
    bool m_circuitOpen = false;
};

#endif
