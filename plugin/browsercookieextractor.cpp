#include "browsercookieextractor.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUuid>
#include <QProcess>
#include <QVariantMap>
#include <memory>

#include <KWallet>

#include <openssl/evp.h>
#include <openssl/sha.h>

BrowserCookieExtractor::BrowserCookieExtractor(QObject *parent)
    : QObject(parent)
{
}

int BrowserCookieExtractor::browserType() const { return m_browserType; }
void BrowserCookieExtractor::setBrowserType(int type)
{
    if (m_browserType != type) {
        m_browserType = type;
        Q_EMIT browserTypeChanged();
    }
}

QString BrowserCookieExtractor::selectedFirefoxProfile() const
{
    return m_selectedFirefoxProfile;
}

void BrowserCookieExtractor::setSelectedFirefoxProfile(const QString &profile)
{
    if (m_selectedFirefoxProfile != profile) {
        m_selectedFirefoxProfile = profile;
        // Invalidate cache when profile changes.
        m_cachedDomain.clear();
        m_cachedCookies.clear();
        m_cacheTimestamp = 0;
        Q_EMIT selectedFirefoxProfileChanged();
        Q_EMIT profilesChanged();
    }
}

bool BrowserCookieExtractor::hasFirefoxProfile() const
{
    return !firefoxProfilePath().isEmpty();
}

bool BrowserCookieExtractor::hasCurrentBrowserProfile() const
{
    switch (m_browserType) {
    case Firefox:
        return !firefoxProfilePath().isEmpty();
    case Chrome:
        return !chromeProfilePath().isEmpty();
    case Chromium:
        return !chromiumProfilePath().isEmpty();
    case Brave:
        return !chromiumSelectedProfilePath(braveProfileRoot()).isEmpty();
    }
    return false;
}

QString BrowserCookieExtractor::cookieDbPath() const
{
    switch (m_browserType) {
    case Firefox: {
        QString profile = firefoxProfilePath();
        if (!profile.isEmpty()) {
            return profile + QStringLiteral("/cookies.sqlite");
        }
        break;
    }
    case Chrome:
        return chromeProfilePath() + QStringLiteral("/Cookies");
    case Chromium:
        return chromiumProfilePath() + QStringLiteral("/Cookies");
    case Brave:
        return chromiumSelectedProfilePath(braveProfileRoot()) + QStringLiteral("/Cookies");
    }
    return QString();
}

// --- Firefox Profile Detection ---

QString BrowserCookieExtractor::firefoxRoot() const
{
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.mozilla/firefox"),
        QDir::homePath() + QStringLiteral("/.var/app/org.mozilla.firefox/.mozilla/firefox"),
        QDir::homePath() + QStringLiteral("/snap/firefox/common/.mozilla/firefox"),
    };

    for (const QString &path : candidates) {
        if (QDir(path).exists()) {
            return path;
        }
    }
    return candidates.first();
}

QString BrowserCookieExtractor::firefoxProfilePath() const
{
    const QString mozDir = firefoxRoot();
    if (!QDir(mozDir).exists()) return QString();

    QDir dir(mozDir);

    // If user selected a specific profile, try it first.
    if (!m_selectedFirefoxProfile.trimmed().isEmpty()) {
        const QString selectedPath = firefoxProfilePathByName(m_selectedFirefoxProfile.trimmed());
        if (!selectedPath.isEmpty()) {
            return selectedPath;
        }
    }

    // Read profiles.ini to find the default profile
    QString profilesIni = mozDir + QStringLiteral("/profiles.ini");
    if (QFileInfo::exists(profilesIni)) {
        QSettings ini(profilesIni, QSettings::IniFormat);
        QStringList groups = ini.childGroups();

        // Look for default-release profile first, then any default
        for (const QString &group : groups) {
            if (!group.startsWith(QStringLiteral("Profile"))) continue;
            ini.beginGroup(group);
            bool isDefault = ini.value(QStringLiteral("Default"), 0).toInt() == 1;
            QString name = ini.value(QStringLiteral("Name")).toString();
            QString path = ini.value(QStringLiteral("Path")).toString();
            bool isRelative = ini.value(QStringLiteral("IsRelative"), 1).toInt() == 1;
            ini.endGroup();

            if (isDefault || name.contains(QStringLiteral("default-release"))) {
                QString fullPath = isRelative ? (mozDir + QStringLiteral("/") + path) : path;
                if (QFileInfo::exists(fullPath + QStringLiteral("/cookies.sqlite"))) {
                    return fullPath;
                }
            }
        }

        // Fallback: find any profile with cookies.sqlite
        for (const QString &group : groups) {
            if (!group.startsWith(QStringLiteral("Profile"))) continue;
            ini.beginGroup(group);
            QString path = ini.value(QStringLiteral("Path")).toString();
            bool isRelative = ini.value(QStringLiteral("IsRelative"), 1).toInt() == 1;
            ini.endGroup();

            QString fullPath = isRelative ? (mozDir + QStringLiteral("/") + path) : path;
            if (QFileInfo::exists(fullPath + QStringLiteral("/cookies.sqlite"))) {
                return fullPath;
            }
        }
    }

    // Last resort: look for *.default-release directory
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (entry.fileName().endsWith(QStringLiteral(".default-release"))
            || entry.fileName().endsWith(QStringLiteral(".default"))) {
            if (QFileInfo::exists(entry.absoluteFilePath() + QStringLiteral("/cookies.sqlite"))) {
                return entry.absoluteFilePath();
            }
        }
    }

    return QString();
}

QString BrowserCookieExtractor::firefoxProfilePathByName(const QString &profileName) const
{
    if (profileName.trimmed().isEmpty()) {
        return QString();
    }

    const QString mozDir = firefoxRoot();
    const QString profilesIni = mozDir + QStringLiteral("/profiles.ini");
    if (!QFileInfo::exists(profilesIni)) {
        return QString();
    }

    QSettings ini(profilesIni, QSettings::IniFormat);
    const QStringList groups = ini.childGroups();
    for (const QString &group : groups) {
        if (!group.startsWith(QStringLiteral("Profile"))) continue;
        ini.beginGroup(group);
        const QString name = ini.value(QStringLiteral("Name")).toString().trimmed();
        const QString path = ini.value(QStringLiteral("Path")).toString();
        const bool isRelative = ini.value(QStringLiteral("IsRelative"), 1).toInt() == 1;
        ini.endGroup();

        if (name != profileName) continue;

        const QString fullPath = isRelative ? (mozDir + QStringLiteral("/") + path) : path;
        if (QFileInfo::exists(fullPath + QStringLiteral("/cookies.sqlite"))) {
            return fullPath;
        }
    }

    return QString();
}

QStringList BrowserCookieExtractor::firefoxProfiles() const
{
    QStringList profiles;
    const QString mozDir = firefoxRoot();
    QDir dir(mozDir);
    if (!dir.exists()) return profiles;

    QString profilesIni = mozDir + QStringLiteral("/profiles.ini");
    if (QFileInfo::exists(profilesIni)) {
        QSettings ini(profilesIni, QSettings::IniFormat);
        QStringList groups = ini.childGroups();
        for (const QString &group : groups) {
            if (!group.startsWith(QStringLiteral("Profile"))) continue;
            ini.beginGroup(group);
            QString name = ini.value(QStringLiteral("Name")).toString();
            ini.endGroup();
            if (!name.isEmpty() && !profiles.contains(name)) profiles.append(name);
        }
    }
    return profiles;
}

QStringList BrowserCookieExtractor::browserProfiles() const
{
    if (m_browserType == Firefox) {
        return firefoxProfiles();
    }

    QStringList profiles;
    const QString rootPath = (m_browserType == Chrome)
        ? chromeProfileRoot()
        : (m_browserType == Brave ? braveProfileRoot() : chromiumProfileRoot());
    QDir root(rootPath);
    if (!root.exists()) {
        return profiles;
    }

    if (QFileInfo::exists(root.filePath(QStringLiteral("Default/Cookies")))) {
        profiles.append(QStringLiteral("Default"));
    }

    const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if ((entry.fileName().startsWith(QStringLiteral("Profile"))
             || entry.fileName() == QStringLiteral("Guest Profile"))
            && QFileInfo::exists(entry.absoluteFilePath() + QStringLiteral("/Cookies"))
            && !profiles.contains(entry.fileName())) {
            profiles.append(entry.fileName());
        }
    }

    return profiles;
}

QString BrowserCookieExtractor::chromeProfilePath() const
{
    return chromiumSelectedProfilePath(chromeProfileRoot());
}

QString BrowserCookieExtractor::chromiumProfilePath() const
{
    return chromiumSelectedProfilePath(chromiumProfileRoot());
}

QString BrowserCookieExtractor::chromeProfileRoot() const
{
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.config/google-chrome"),
        QDir::homePath() + QStringLiteral("/.var/app/com.google.Chrome/config/google-chrome"),
    };
    for (const QString &path : candidates) {
        if (QDir(path).exists()) return path;
    }
    return candidates.first();
}

QString BrowserCookieExtractor::chromiumProfileRoot() const
{
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.config/chromium"),
        QDir::homePath() + QStringLiteral("/.var/app/org.chromium.Chromium/config/chromium"),
    };
    for (const QString &path : candidates) {
        if (QDir(path).exists()) return path;
    }
    return candidates.first();
}

QString BrowserCookieExtractor::braveProfileRoot() const
{
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.config/BraveSoftware/Brave-Browser"),
        QDir::homePath() + QStringLiteral("/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser"),
    };
    for (const QString &path : candidates) {
        if (QDir(path).exists()) return path;
    }
    return candidates.first();
}

QString BrowserCookieExtractor::chromiumSelectedProfilePath(const QString &rootPath) const
{
    QDir root(rootPath);
    if (!root.exists()) {
        return QString();
    }

    if (!m_selectedFirefoxProfile.trimmed().isEmpty()) {
        const QString selectedPath = root.filePath(m_selectedFirefoxProfile.trimmed());
        if (QFileInfo::exists(selectedPath + QStringLiteral("/Cookies"))) {
            return selectedPath;
        }
    }

    const QString defaultPath = root.filePath(QStringLiteral("Default"));
    if (QFileInfo::exists(defaultPath + QStringLiteral("/Cookies"))) {
        return defaultPath;
    }

    const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if ((entry.fileName().startsWith(QStringLiteral("Profile"))
             || entry.fileName() == QStringLiteral("Guest Profile"))
            && QFileInfo::exists(entry.absoluteFilePath() + QStringLiteral("/Cookies"))) {
            return entry.absoluteFilePath();
        }
    }

    return QString();
}

// --- Cookie Reading (Firefox) ---

QMap<QString, QString> BrowserCookieExtractor::readFirefoxCookies(const QString &domain) const
{
    // Return cached result if same domain and within TTL
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (domain == m_cachedDomain && (now - m_cacheTimestamp) < CACHE_TTL_MS) {
        return m_cachedCookies;
    }

    QMap<QString, QString> cookies;

    QString dbPath = cookieDbPath();
    if (dbPath.isEmpty() || !QFileInfo::exists(dbPath)) {
        return cookies;
    }

    // Use a unique connection name to avoid conflicts
    QString connName = QStringLiteral("firefox_cookies_") + QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Copy the database and its WAL sidecar to a private temporary directory.
    // Recent Firefox writes active login cookies to cookies.sqlite-wal while it
    // is running. Copying only cookies.sqlite therefore returns a stale view
    // and incorrectly reports a missing session.
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        qWarning() << "BrowserCookieExtractor: Cannot create temporary cookie snapshot directory";
        return cookies;
    }
    QFile::setPermissions(tmpDir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    const QString tmpPath = tmpDir.filePath(QStringLiteral("cookies.sqlite"));
    if (!QFile::copy(dbPath, tmpPath)) {
        qWarning() << "BrowserCookieExtractor: Cannot copy cookies.sqlite to temporary snapshot";
        return cookies;
    }
    QFile::setPermissions(tmpPath, QFile::ReadOwner | QFile::WriteOwner);

    for (const QString &suffix : {QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString sourceSidecar = dbPath + suffix;
        if (!QFileInfo::exists(sourceSidecar)) {
            continue;
        }
        const QString targetSidecar = tmpPath + suffix;
        if (!QFile::copy(sourceSidecar, targetSidecar)) {
            qWarning() << "BrowserCookieExtractor: Cannot copy Firefox cookie sidecar" << suffix;
            return cookies;
        }
        QFile::setPermissions(targetSidecar, QFile::ReadOwner | QFile::WriteOwner);
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(tmpPath);

        if (!db.open()) {
            qWarning() << "BrowserCookieExtractor: Cannot open Firefox cookies.sqlite:" << db.lastError().text();
            QSqlDatabase::removeDatabase(connName);
            return cookies;
        }

        QSqlQuery query(db);
        // Firefox stores expiry as Unix timestamp in seconds
        // Filter out expired cookies
        query.prepare(QStringLiteral(
            "SELECT name, value FROM moz_cookies "
            "WHERE (host = :domain1 OR host = :domain2) "
            "AND (expiry > :now OR expiry = 0)"
        ));
        query.bindValue(QStringLiteral(":domain1"), domain);
        // Also match with/without leading dot
        QString altDomain = domain.startsWith(QLatin1Char('.'))
            ? domain.mid(1) : (QStringLiteral(".") + domain);
        query.bindValue(QStringLiteral(":domain2"), altDomain);
        query.bindValue(QStringLiteral(":now"), QDateTime::currentSecsSinceEpoch());

        if (query.exec()) {
            while (query.next()) {
                cookies.insert(query.value(0).toString(), query.value(1).toString());
            }
        } else {
            qWarning() << "BrowserCookieExtractor: Cookie query failed:" << query.lastError().text();
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connName);
    // Update cache
    m_cachedDomain = domain;
    m_cachedCookies = cookies;
    m_cacheTimestamp = QDateTime::currentMSecsSinceEpoch();

    return cookies;
}

QMap<QString, QString> BrowserCookieExtractor::readChromiumCookies(const QString &domain) const
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (domain == m_cachedDomain && (now - m_cacheTimestamp) < CACHE_TTL_MS) {
        return m_cachedCookies;
    }

    QMap<QString, QString> cookies;
    const QString dbPath = cookieDbPath();
    if (dbPath.isEmpty() || !QFileInfo::exists(dbPath)) {
        return cookies;
    }

    const QString connName = QStringLiteral("chromium_cookies_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(true);
    if (!tmpFile.open()) {
        return cookies;
    }
    tmpFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    if (!QFile::copy(dbPath, tmpPath)) {
        QFile::remove(tmpPath);
        if (!QFile::copy(dbPath, tmpPath)) {
            return cookies;
        }
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(tmpPath);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(connName);
            return cookies;
        }

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT name, value, encrypted_value FROM cookies "
            "WHERE (host_key = :domain1 OR host_key = :domain2) "
            "AND (expires_utc = 0 OR expires_utc > :now)"
        ));
        query.bindValue(QStringLiteral(":domain1"), domain);
        const QString altDomain = domain.startsWith(QLatin1Char('.'))
            ? domain.mid(1) : (QStringLiteral(".") + domain);
        query.bindValue(QStringLiteral(":domain2"), altDomain);
        query.bindValue(QStringLiteral(":now"),
                        (QDateTime::currentMSecsSinceEpoch() + 11644473600000LL) * 10);

        if (query.exec()) {
            while (query.next()) {
                const QString name = query.value(0).toString();
                QString value = query.value(1).toString();
                if (value.isEmpty()) {
                    value = decryptChromiumCookieValue(query.value(2).toByteArray());
                }
                if (!value.isEmpty()) {
                    cookies.insert(name, value);
                }
            }
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connName);
    QFile::remove(tmpPath);

    m_cachedDomain = domain;
    m_cachedCookies = cookies;
    m_cacheTimestamp = QDateTime::currentMSecsSinceEpoch();

    return cookies;
}

QString BrowserCookieExtractor::chromiumSafeStoragePassword() const
{
    const QString envName = (m_browserType == Chrome)
        ? QStringLiteral("PLASMA_AI_MONITOR_CHROME_SAFE_STORAGE")
        : (m_browserType == Brave
           ? QStringLiteral("PLASMA_AI_MONITOR_BRAVE_SAFE_STORAGE")
           : QStringLiteral("PLASMA_AI_MONITOR_CHROMIUM_SAFE_STORAGE"));
    const QString envValue = QString::fromLocal8Bit(qgetenv(envName.toUtf8().constData())).trimmed();
    if (!envValue.isEmpty()) {
        return envValue;
    }

    QProcess process;
    process.start(QStringLiteral("secret-tool"),
                  {QStringLiteral("lookup"),
                   QStringLiteral("application"),
                   currentChromiumSafeStorageName()});
    if (process.waitForFinished(1500) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0) {
        const QString password = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!password.isEmpty()) {
            return password;
        }
    }

    return chromiumSafeStoragePasswordFromKWallet();
}

QString BrowserCookieExtractor::chromiumSafeStoragePasswordFromKWallet() const
{
    std::unique_ptr<KWallet::Wallet> wallet(
        KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous));
    if (!wallet) {
        return QString();
    }

    const QStringList folders = {
        QStringLiteral("Chrome Keys"),
        QStringLiteral("Chromium Keys"),
        QStringLiteral("Passwords"),
        QStringLiteral("Network Wallet")
    };
    const QStringList entries = {
        currentChromiumSafeStorageName(),
        currentChromiumSafeStorageName().toLower().replace(QLatin1Char(' '), QLatin1Char('_'))
    };

    for (const QString &folder : folders) {
        if (!wallet->hasFolder(folder)) {
            continue;
        }
        wallet->setFolder(folder);
        for (const QString &entry : entries) {
            QString password;
            if (wallet->readPassword(entry, password) == 0 && !password.isEmpty()) {
                return password;
            }
        }
    }

    return QString();
}

QString BrowserCookieExtractor::decryptChromiumCookieValue(const QByteArray &encryptedValue) const
{
    if (encryptedValue.isEmpty()) {
        return QString();
    }

    if (!encryptedValue.startsWith("v10") && !encryptedValue.startsWith("v11")) {
        return QString::fromUtf8(encryptedValue);
    }

    const QString safeStoragePassword = chromiumSafeStoragePassword();
    if (safeStoragePassword.isEmpty()) {
        return QString();
    }

    unsigned char key[16] = {};
    if (PKCS5_PBKDF2_HMAC_SHA1(safeStoragePassword.toUtf8().constData(),
                               safeStoragePassword.toUtf8().size(),
                               reinterpret_cast<const unsigned char *>("saltysalt"),
                               9,
                               1,
                               sizeof(key),
                               key) != 1) {
        return QString();
    }

    const QByteArray cipherText = encryptedValue.mid(3);
    QByteArray plainText(cipherText.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int outLen1 = 0;
    int outLen2 = 0;
    const unsigned char iv[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                                  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return QString();
    }

    const bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key, iv) == 1
        && EVP_DecryptUpdate(ctx,
                             reinterpret_cast<unsigned char *>(plainText.data()), &outLen1,
                             reinterpret_cast<const unsigned char *>(cipherText.constData()),
                             cipherText.size()) == 1
        && EVP_DecryptFinal_ex(ctx,
                               reinterpret_cast<unsigned char *>(plainText.data()) + outLen1,
                               &outLen2) == 1;
    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        return QString();
    }

    plainText.truncate(outLen1 + outLen2);
    return QString::fromUtf8(plainText);
}

QString BrowserCookieExtractor::getCookie(const QString &domain, const QString &name) const
{
    const QMap<QString, QString> cookies = (m_browserType == Firefox)
        ? readFirefoxCookies(domain)
        : readChromiumCookies(domain);
    return cookies.value(name);
}

QString BrowserCookieExtractor::currentBrowserName() const
{
    switch (m_browserType) {
    case Firefox: return QStringLiteral("Firefox");
    case Chrome: return QStringLiteral("Chrome");
    case Chromium: return QStringLiteral("Chromium");
    case Brave: return QStringLiteral("Brave");
    }
    return QStringLiteral("Unknown");
}

QString BrowserCookieExtractor::currentChromiumSafeStorageName() const
{
    if (m_browserType == Chrome) return QStringLiteral("Chrome Safe Storage");
    if (m_browserType == Brave) return QStringLiteral("Brave Safe Storage");
    return QStringLiteral("Chromium Safe Storage");
}

bool BrowserCookieExtractor::hasSafeStorageAccess() const
{
    if (m_browserType == Firefox) {
        return true;
    }
    return !chromiumSafeStoragePassword().isEmpty();
}

QString BrowserCookieExtractor::readinessSummary() const
{
    const QVariantMap report = readinessReport();
    if (report.value(QStringLiteral("ready")).toBool()) {
        return QStringLiteral("Ready");
    }
    return report.value(QStringLiteral("nextStep")).toString();
}

QVariantMap BrowserCookieExtractor::readinessReport(const QString &service) const
{
    QVariantMap report;
    const QString dbPath = cookieDbPath();
    const bool profileReady = hasCurrentBrowserProfile();
    const bool dbReady = !dbPath.isEmpty() && QFileInfo::exists(dbPath) && QFileInfo(dbPath).isReadable();
    const bool safeStorageReady = hasSafeStorageAccess();

    report.insert(QStringLiteral("browser"), currentBrowserName());
    report.insert(QStringLiteral("profileReady"), profileReady);
    report.insert(QStringLiteral("cookieDatabaseReadable"), dbReady);
    report.insert(QStringLiteral("safeStorageReady"), safeStorageReady);
    report.insert(QStringLiteral("cookieDatabasePath"), dbPath);

    QString serviceCode;
    if (!service.isEmpty()) {
        serviceCode = testConnection(service);
        report.insert(QStringLiteral("serviceProbe"), serviceCode);
    }

    const bool ready = profileReady && dbReady && safeStorageReady
        && (service.isEmpty() || serviceCode == QStringLiteral("connected"));
    report.insert(QStringLiteral("ready"), ready);

    QString nextStep;
    if (!profileReady) {
        nextStep = QStringLiteral("Choose a supported browser profile or open the browser once.");
    } else if (!dbReady) {
        nextStep = QStringLiteral("Open the browser, sign in to the service, then retry so the cookie database exists and is readable.");
    } else if (!safeStorageReady) {
        nextStep = QStringLiteral("Unlock KWallet/libsecret safe storage, or use local estimation instead of Browser Sync Labs.");
    } else if (!service.isEmpty() && serviceCode != QStringLiteral("connected")) {
        nextStep = connectionMessage(service, serviceCode) + QStringLiteral(" Local estimation remains available.");
    } else {
        nextStep = QStringLiteral("Browser Sync Labs can probe this profile. Local estimation remains available as fallback.");
    }
    report.insert(QStringLiteral("nextStep"), nextStep);

    return report;
}

bool BrowserCookieExtractor::hasCookiesFor(const QString &domain) const
{
    const QMap<QString, QString> cookies = (m_browserType == Firefox)
        ? readFirefoxCookies(domain)
        : readChromiumCookies(domain);
    return !cookies.isEmpty();
}

QString BrowserCookieExtractor::getCookieHeader(const QString &domain) const
{
    const QMap<QString, QString> cookies = (m_browserType == Firefox)
        ? readFirefoxCookies(domain)
        : readChromiumCookies(domain);
    QStringList parts;
    for (auto it = cookies.constBegin(); it != cookies.constEnd(); ++it) {
        parts.append(it.key() + QStringLiteral("=") + it.value());
    }
    return parts.join(QStringLiteral("; "));
}

QString BrowserCookieExtractor::testConnection(const QString &service) const
{
    QString domain;
    QStringList sessionCookieNames;

    if (service == QStringLiteral("claude")) {
        domain = QStringLiteral("claude.ai");
        // Claude.ai primary session cookies
        sessionCookieNames = {
            QStringLiteral("sessionKey"),
            QStringLiteral("__Secure-next-auth.session-token"),
        };
    } else if (service == QStringLiteral("chatgpt") || service == QStringLiteral("codex")) {
        domain = QStringLiteral("chatgpt.com");
        // ChatGPT primary session cookies
        sessionCookieNames = {
            QStringLiteral("__Secure-next-auth.session-token"),
            QStringLiteral("__Secure-next-auth.callback-url"),
        };
    } else if (service == QStringLiteral("github")) {
        domain = QStringLiteral("github.com");
        sessionCookieNames = {
            QStringLiteral("user_session"),
            QStringLiteral("dotcom_user"),
        };
    } else {
        return QStringLiteral("unknown_service");
    }

    if (!hasCurrentBrowserProfile()) {
        return QStringLiteral("profile_missing");
    }

    const QString dbPath = cookieDbPath();
    if (dbPath.isEmpty() || !QFileInfo::exists(dbPath)) {
        return QStringLiteral("cookie_db_missing");
    }

    if (!hasCookiesFor(domain)) {
        return QStringLiteral("cookies_not_found");
    }

    // Has cookies — check for actual session cookies (not just CF bot management etc.)
    QMap<QString, QString> cookies = (m_browserType == Firefox)
        ? readFirefoxCookies(domain)
        : readChromiumCookies(domain);

    for (const QString &name : sessionCookieNames) {
        for (auto it = cookies.constBegin(); it != cookies.constEnd(); ++it) {
            // Auth.js/NextAuth splits large secure cookies into numbered
            // chunks (for example session-token.0 and session-token.1).
            if ((it.key() == name || it.key().startsWith(name + QLatin1Char('.')))
                && !it.value().isEmpty()) {
                return QStringLiteral("connected");
            }
        }
    }

    // Has cookies for the domain but none of the expected session cookies.
    return QStringLiteral("session_missing_or_expired");
}

QString BrowserCookieExtractor::connectionMessage(const QString &service, const QString &code) const
{
    Q_UNUSED(service);

    QString normalizedCode = code;
    if (normalizedCode == QStringLiteral("not_found")) {
        normalizedCode = QStringLiteral("cookies_not_found");
    } else if (normalizedCode == QStringLiteral("expired")) {
        normalizedCode = QStringLiteral("session_missing_or_expired");
    }

    if (normalizedCode == QStringLiteral("connected")) {
        return QStringLiteral("Connected");
    }
    if (normalizedCode == QStringLiteral("profile_missing")) {
        return QStringLiteral("No browser profile detected on this system.");
    }
    if (normalizedCode == QStringLiteral("cookie_db_missing")) {
        return QStringLiteral("Browser profile found, but the cookie database is missing or unreadable.");
    }
    if (normalizedCode == QStringLiteral("cookies_not_found")) {
        return QStringLiteral("No cookies found for this service.");
    }
    if (normalizedCode == QStringLiteral("session_missing_or_expired")) {
        return QStringLiteral("Session cookies missing or expired. Sign in again.");
    }
    if (normalizedCode == QStringLiteral("unsupported_browser")) {
        return QStringLiteral("The selected browser is not supported for Browser Sync.");
    }
    if (normalizedCode == QStringLiteral("unknown_service")) {
        return QStringLiteral("Unknown service.");
    }

    return QStringLiteral("Connection check failed.");
}
