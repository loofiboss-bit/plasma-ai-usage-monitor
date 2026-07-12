#include "browsersyncservice.h"

#include <QMetaObject>

BrowserSyncService::BrowserSyncService(QObject *parent)
    : QObject(parent)
    , m_extractor(this)
{
    connect(&m_extractor, &BrowserCookieExtractor::profilesChanged,
            this, &BrowserSyncService::readinessChanged);
    connect(&m_extractor, &BrowserCookieExtractor::browserTypeChanged,
            this, &BrowserSyncService::configurationChanged);
    connect(&m_extractor, &BrowserCookieExtractor::selectedFirefoxProfileChanged,
            this, &BrowserSyncService::configurationChanged);
}

int BrowserSyncService::browserType() const { return m_extractor.browserType(); }
void BrowserSyncService::setBrowserType(int type) { m_extractor.setBrowserType(type); }
QString BrowserSyncService::selectedFirefoxProfile() const { return m_extractor.selectedFirefoxProfile(); }
void BrowserSyncService::setSelectedFirefoxProfile(const QString &profile) { m_extractor.setSelectedFirefoxProfile(profile); }
bool BrowserSyncService::hasFirefoxProfile() const { return m_extractor.hasFirefoxProfile(); }
bool BrowserSyncService::hasCurrentBrowserProfile() const { return m_extractor.hasCurrentBrowserProfile(); }
bool BrowserSyncService::hasSafeStorageAccess() const { return m_extractor.hasSafeStorageAccess(); }
QString BrowserSyncService::readinessSummary() const { return m_extractor.readinessSummary(); }
QString BrowserSyncService::state() const { return m_state; }
QString BrowserSyncService::lastDiagnostic() const { return m_lastDiagnostic; }
bool BrowserSyncService::circuitOpen() const { return m_circuitOpen; }
QStringList BrowserSyncService::firefoxProfiles() const { return m_extractor.firefoxProfiles(); }
QStringList BrowserSyncService::browserProfiles() const { return m_extractor.browserProfiles(); }
QVariantMap BrowserSyncService::readinessReport(const QString &service) const { return m_extractor.readinessReport(service); }
QString BrowserSyncService::testConnection(const QString &service) const { return m_extractor.testConnection(service); }
QString BrowserSyncService::connectionMessage(const QString &service, const QString &code) const
{ return m_extractor.connectionMessage(service, code); }

QString BrowserSyncService::domainForService(const QString &service) const
{
    if (service == QLatin1String("claude")) return QStringLiteral("claude.ai");
    if (service == QLatin1String("codex")) return QStringLiteral("chatgpt.com");
    return {};
}

void BrowserSyncService::recordFailure(const QString &diagnostic)
{
    ++m_consecutiveFailures;
    m_lastDiagnostic = diagnostic;
    m_state = QStringLiteral("failed");
    if (m_consecutiveFailures >= 3) {
        m_circuitOpen = true;
        m_state = QStringLiteral("disabled_by_circuit_breaker");
    }
    Q_EMIT stateChanged();
}

bool BrowserSyncService::sync(const QString &service, QObject *monitor)
{
    if (m_circuitOpen || !monitor) {
        return false;
    }
    const QString domain = domainForService(service);
    if (domain.isEmpty()) {
        recordFailure(QStringLiteral("unknown_service"));
        return false;
    }
    const QString cookieHeader = m_extractor.getCookieHeader(domain);
    if (cookieHeader.isEmpty()) {
        recordFailure(QStringLiteral("session_missing_or_expired"));
        return false;
    }

    m_state = QStringLiteral("syncing");
    m_lastDiagnostic.clear();
    Q_EMIT stateChanged();
    const bool invoked = QMetaObject::invokeMethod(
        monitor, "syncFromBrowser", Qt::DirectConnection,
        Q_ARG(QString, cookieHeader), Q_ARG(int, browserType()));
    if (!invoked) {
        recordFailure(QStringLiteral("monitor_contract_error"));
        return false;
    }
    m_consecutiveFailures = 0;
    m_state = QStringLiteral("requested");
    Q_EMIT stateChanged();
    return true;
}

void BrowserSyncService::resetCircuit()
{
    m_consecutiveFailures = 0;
    m_circuitOpen = false;
    m_lastDiagnostic.clear();
    m_state = QStringLiteral("idle");
    Q_EMIT stateChanged();
}
