#ifndef BROWSERCOOKIEEXTRACTOR_H
#define BROWSERCOOKIEEXTRACTOR_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QDateTime>
#include <QVariantMap>

/**
 * Utility class to extract session cookies from the user's web browser.
 *
 * This is used by the experimental browser-sync feature to authenticate
 * with internal APIs of Claude.ai, ChatGPT, and GitHub.
 *
 * Supported browsers:
 * - Firefox: reads cookies from cookies.sqlite snapshots.
 * - Chrome, Chromium, and Brave on Linux when safe-storage secrets are available.
 *
 * WARNING: This feature uses internal/undocumented APIs of third-party services.
 * It may violate Terms of Service and can break without notice. Users must
 * opt-in and acknowledge the experimental nature.
 */
class BrowserCookieExtractor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int browserType READ browserType WRITE setBrowserType NOTIFY browserTypeChanged)
    Q_PROPERTY(bool hasFirefoxProfile READ hasFirefoxProfile NOTIFY profilesChanged)
    Q_PROPERTY(bool hasCurrentBrowserProfile READ hasCurrentBrowserProfile NOTIFY profilesChanged)
    Q_PROPERTY(bool hasSafeStorageAccess READ hasSafeStorageAccess NOTIFY profilesChanged)
    Q_PROPERTY(QString readinessSummary READ readinessSummary NOTIFY profilesChanged)
    Q_PROPERTY(QString selectedFirefoxProfile READ selectedFirefoxProfile WRITE setSelectedFirefoxProfile NOTIFY selectedFirefoxProfileChanged)

public:
    enum BrowserType {
        Firefox = 0,
        Chrome = 1,
        Chromium = 2,
        Brave = 3
    };
    Q_ENUM(BrowserType)

    explicit BrowserCookieExtractor(QObject *parent = nullptr);

    int browserType() const;
    void setBrowserType(int type);
    bool hasFirefoxProfile() const;
    bool hasCurrentBrowserProfile() const;
    QString selectedFirefoxProfile() const;
    void setSelectedFirefoxProfile(const QString &profile);

    /**
     * Get the path to the active browser's cookie database.
     * For Firefox, returns the cookies.sqlite path of the default profile.
     */
    Q_INVOKABLE QString cookieDbPath() const;

    /**
     * Extract a specific cookie value from the browser's cookie store.
     * @param domain Cookie domain (e.g., ".claude.ai", ".chatgpt.com")
     * @param name Cookie name (e.g., "sessionKey", "__Secure-next-auth.session-token")
     * @return Cookie value, or empty string if not found or expired
     */
    Q_INVOKABLE QString getCookie(const QString &domain, const QString &name) const;

    /**
     * Check if valid (non-expired) cookies exist for a given domain.
     */
    Q_INVOKABLE bool hasCookiesFor(const QString &domain) const;

    /**
     * Get all cookies for a domain as a formatted cookie header string.
     * e.g., "name1=value1; name2=value2"
     */
    Q_INVOKABLE QString getCookieHeader(const QString &domain) const;

    /**
     * Test connection to a service by checking if we have valid session cookies.
     * @param service One of "claude", "codex", "github"
     * @return Status string:
     *   "connected"
     *   "profile_missing"
     *   "cookie_db_missing"
     *   "cookies_not_found"
     *   "session_missing_or_expired"
     *   "unsupported_browser"
     *   "unknown_service"
     */
    Q_INVOKABLE QString testConnection(const QString &service) const;
    Q_INVOKABLE QString connectionMessage(const QString &service, const QString &code) const;

    /**
     * Detect available Firefox profiles.
     */
    Q_INVOKABLE QStringList firefoxProfiles() const;
    Q_INVOKABLE QStringList browserProfiles() const;
    Q_INVOKABLE QVariantMap readinessReport(const QString &service = QString()) const;
    bool hasSafeStorageAccess() const;
    QString readinessSummary() const;

Q_SIGNALS:
    void browserTypeChanged();
    void profilesChanged();
    void selectedFirefoxProfileChanged();

private:
    QString firefoxRoot() const;
    QString firefoxProfilePath() const;
    QString firefoxProfilePathByName(const QString &profileName) const;
    QString chromeProfilePath() const;
    QString chromiumProfilePath() const;
    QString chromeProfileRoot() const;
    QString chromiumProfileRoot() const;
    QString braveProfileRoot() const;
    QString currentBrowserName() const;
    QString currentChromiumSafeStorageName() const;

    // Read cookies from Firefox SQLite database (unencrypted)
    QMap<QString, QString> readFirefoxCookies(const QString &domain) const;
    QMap<QString, QString> readChromiumCookies(const QString &domain) const;
    QString chromiumSelectedProfilePath(const QString &rootPath) const;
    QString chromiumSafeStoragePassword() const;
    QString chromiumSafeStoragePasswordFromKWallet() const;
    QString decryptChromiumCookieValue(const QByteArray &encryptedValue) const;

    int m_browserType = Firefox;

    // Short-lived cache to avoid redundant DB reads within a single operation
    mutable QString m_cachedDomain;
    mutable QMap<QString, QString> m_cachedCookies;
    mutable qint64 m_cacheTimestamp = 0;
    static constexpr int CACHE_TTL_MS = 3000; // 3 seconds
    QString m_selectedFirefoxProfile;
};

#endif // BROWSERCOOKIEEXTRACTOR_H
