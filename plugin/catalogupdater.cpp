#include "catalogupdater.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

#include <utility>

#include <openssl/evp.h>

namespace {

void fail(QString *diagnostic, const QString &message)
{
    if (diagnostic) *diagnostic = message;
}

bool isFutureTimestamp(const QString &value)
{
    const QDateTime timestamp = QDateTime::fromString(value, Qt::ISODateWithMs).toUTC();
    const QDateTime fallback = timestamp.isValid()
        ? timestamp
        : QDateTime::fromString(value, Qt::ISODate).toUTC();
    return fallback.isValid() && fallback > QDateTime::currentDateTimeUtc();
}

bool versionAtLeast(const QString &required, const QString &current)
{
    if (required.trimmed().isEmpty()) return true;
    if (current.trimmed().isEmpty()) return false;
    const QVersionNumber requiredVersion = QVersionNumber::fromString(required);
    const QVersionNumber currentVersion = QVersionNumber::fromString(current);
    if (requiredVersion.isNull() || currentVersion.isNull()) return false;
    return currentVersion >= requiredVersion;
}

struct ParsedEnvelope {
    QJsonObject envelopeObject;
    QJsonObject payload;
    QByteArray canonicalPayload;
    QString actualSha;
};

struct CachedState {
    bool hadPreviousCatalog = false;
    bool hadPreviousMetadata = false;
    QByteArray previousCatalog;
    QByteArray previousMetadata;
    qint64 currentSequence = 0;
};

bool parseAndVerifyEnvelope(const QByteArray &envelope, ParsedEnvelope &out, QString *diagnostic)
{
    if (envelope.size() <= 0 || envelope.size() > 8 * 1024 * 1024) {
        fail(diagnostic, QStringLiteral("signed catalog envelope exceeds the size limit"));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(envelope, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(diagnostic, QStringLiteral("signed catalog envelope is invalid JSON"));
        return false;
    }
    out.envelopeObject = document.object();
    out.payload = out.envelopeObject.value(QStringLiteral("payload")).toObject();
    if (out.payload.isEmpty()) {
        fail(diagnostic, QStringLiteral("signed catalog envelope has no payload"));
        return false;
    }
    if (out.envelopeObject.value(QStringLiteral("keyId")).toString()
            != QLatin1String("catalog-ed25519-v1")) {
        fail(diagnostic, QStringLiteral("signed catalog key id is not trusted"));
        return false;
    }

    out.canonicalPayload = QJsonDocument(out.payload).toJson(QJsonDocument::Compact);
    const QByteArray signature = QByteArray::fromBase64(
        out.envelopeObject.value(QStringLiteral("signature")).toString().toUtf8());
    const QString expectedSha = out.envelopeObject.value(QStringLiteral("sha256")).toString().toLower();
    out.actualSha = QString::fromLatin1(QCryptographicHash::hash(
        out.canonicalPayload, QCryptographicHash::Sha256).toHex());
    if (expectedSha.isEmpty() || expectedSha != out.actualSha
        || !CatalogUpdateManager::verifyEd25519(out.canonicalPayload, signature)) {
        fail(diagnostic, QStringLiteral("signed catalog signature or digest verification failed"));
        return false;
    }

    return true;
}

bool validateFeedConstraints(const ParsedEnvelope &parsed, QString *diagnostic)
{
    if (parsed.payload.value(QStringLiteral("schemaVersion")).toInt() != 7
        || parsed.payload.value(QStringLiteral("runtimeScraping")).toBool(true) != false) {
        fail(diagnostic, QStringLiteral("signed catalog schema or runtime policy is invalid"));
        return false;
    }
    const qint64 sequence = parsed.payload.value(QStringLiteral("sequence")).toVariant().toLongLong();
    if (sequence < 1) {
        fail(diagnostic, QStringLiteral("signed catalog sequence is invalid"));
        return false;
    }
    if (parsed.envelopeObject.value(QStringLiteral("sequence")).toVariant().toLongLong() != sequence) {
        fail(diagnostic, QStringLiteral("signed catalog envelope sequence does not match payload"));
        return false;
    }
    if (parsed.payload.value(QStringLiteral("catalogVersion")).toString().trimmed().isEmpty()
        || !parsed.payload.value(QStringLiteral("providers")).isArray()) {
        fail(diagnostic, QStringLiteral("signed catalog required fields are invalid"));
        return false;
    }
    if (!isFutureTimestamp(parsed.envelopeObject.value(QStringLiteral("expiresAt")).toString())
        || !isFutureTimestamp(parsed.payload.value(QStringLiteral("hardExpiresAt")).toString())) {
        fail(diagnostic, QStringLiteral("signed catalog expiry is missing or expired"));
        return false;
    }
    QString appVersion = QString::fromLatin1(qgetenv("AIUSAGE_MONITOR_VERSION"));
#ifdef AIUSAGE_MONITOR_VERSION
    if (appVersion.isEmpty()) appVersion = QStringLiteral(AIUSAGE_MONITOR_VERSION);
#endif
    if (!versionAtLeast(parsed.envelopeObject.value(QStringLiteral("minAppVersion")).toString(), appVersion)) {
        fail(diagnostic, QStringLiteral("signed catalog requires a newer application version"));
        return false;
    }

    return true;
}

bool verifyCacheAndRollback(const QString &target, const QString &metaPath,
                             qint64 sequence, CachedState &outState, QString *diagnostic)
{
    outState.hadPreviousCatalog = QFile::exists(target);
    outState.hadPreviousMetadata = QFile::exists(metaPath);
    outState.previousCatalog = [&]() {
        QFile file(target);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    outState.previousMetadata = [&]() {
        QFile file(metaPath);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    if (outState.hadPreviousCatalog != outState.hadPreviousMetadata) {
        fail(diagnostic, QStringLiteral("cached catalog metadata is missing"));
        return false;
    }
    QFile metaFile(metaPath);
    outState.currentSequence = 0;
    if (outState.hadPreviousMetadata && metaFile.open(QIODevice::ReadOnly)) {
        const QByteArray metadataBytes = metaFile.readAll();
        QJsonParseError metadataError;
        const QJsonDocument meta = QJsonDocument::fromJson(metadataBytes, &metadataError);
        if (metadataError.error != QJsonParseError::NoError || !meta.isObject()) {
            fail(diagnostic, QStringLiteral("cached catalog metadata is invalid"));
            return false;
        }
        outState.currentSequence = meta.object().value(QStringLiteral("sequence")).toVariant().toLongLong();
        const QString cachedSha = meta.object().value(QStringLiteral("sha256")).toString().toLower();
        if (outState.currentSequence < 1 || cachedSha.isEmpty()
            || cachedSha != QString::fromLatin1(QCryptographicHash::hash(
                outState.previousCatalog, QCryptographicHash::Sha256).toHex())) {
            fail(diagnostic, QStringLiteral("cached catalog integrity verification failed"));
            return false;
        }
    } else if (outState.hadPreviousMetadata) {
        fail(diagnostic, QStringLiteral("cached catalog metadata is unreadable"));
        return false;
    }
    if (sequence <= outState.currentSequence) {
        fail(diagnostic, QStringLiteral("signed catalog sequence is a rollback or duplicate"));
        return false;
    }

    return true;
}

void restoreOrCleanupCache(const QString &target, const QString &metaPath,
                           const CachedState &cached)
{
    if (cached.hadPreviousCatalog && cached.hadPreviousMetadata) {
        QSaveFile restoreCatalog(target);
        if (restoreCatalog.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            restoreCatalog.write(cached.previousCatalog);
            restoreCatalog.commit();
        }
        QSaveFile restoreMetadata(metaPath);
        if (restoreMetadata.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            restoreMetadata.write(cached.previousMetadata);
            restoreMetadata.commit();
        }
    } else {
        QFile::remove(target);
        QFile::remove(metaPath);
    }
}

bool activateCatalog(const QString &target, const QString &metaPath,
                     const ParsedEnvelope &parsed, qint64 sequence,
                     const CachedState &cached, QString *diagnostic)
{
    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile catalogFile(target);
    if (!catalogFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || catalogFile.write(parsed.canonicalPayload) != parsed.canonicalPayload.size()
        || !catalogFile.commit()) {
        fail(diagnostic, QStringLiteral("signed catalog could not be activated atomically"));
        return false;
    }

    QSaveFile metadataFile(metaPath);
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        restoreOrCleanupCache(target, metaPath, cached);
        fail(diagnostic, QStringLiteral("signed catalog metadata could not be written"));
        return false;
    }
    const QJsonObject metadata{
        {QStringLiteral("sequence"), sequence},
        {QStringLiteral("sha256"), parsed.actualSha},
        {QStringLiteral("state"), QStringLiteral("remote_verified")},
        {QStringLiteral("installedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    };
    const QByteArray metadataBytes = QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if (metadataFile.write(metadataBytes) != metadataBytes.size() || !metadataFile.commit()) {
        restoreOrCleanupCache(target, metaPath, cached);
        fail(diagnostic, QStringLiteral("signed catalog metadata could not be activated atomically"));
        return false;
    }

    return true;
}

} // namespace

CatalogUpdateManager::CatalogUpdateManager(QString fileName, QObject *parent)
    : QObject(parent)
    , m_fileName(std::move(fileName))
{
    m_networkManager = new QNetworkAccessManager(this);
    m_feedUrl = QString::fromLocal8Bit(qgetenv("AIUSAGE_MONITOR_CATALOG_FEED_URL")).trimmed();
    m_networkUpdatesEnabled = qgetenv("AIUSAGE_MONITOR_CATALOG_UPDATES") != "0";
}

bool CatalogUpdateManager::networkUpdatesEnabled() const
{
    return m_networkUpdatesEnabled;
}

void CatalogUpdateManager::setNetworkUpdatesEnabled(bool enabled)
{
    if (m_networkUpdatesEnabled == enabled) return;
    m_networkUpdatesEnabled = enabled;
    Q_EMIT configurationChanged();
}

QString CatalogUpdateManager::feedUrl() const
{
    return m_feedUrl;
}

void CatalogUpdateManager::setFeedUrl(const QString &url)
{
    const QString normalized = url.trimmed();
    if (m_feedUrl == normalized) return;
    m_feedUrl = normalized;
    Q_EMIT configurationChanged();
}

QString CatalogUpdateManager::status() const
{
    return m_status;
}

QString CatalogUpdateManager::diagnostic() const
{
    return m_diagnostic;
}

QDateTime CatalogUpdateManager::lastChecked() const
{
    return m_lastChecked;
}

bool CatalogUpdateManager::checking() const
{
    return m_checking;
}

QByteArray CatalogUpdateManager::pinnedPublicKey()
{
#ifdef CATALOG_UPDATER_TEST_PUBLIC_KEY_HEX
    return QByteArray::fromHex(CATALOG_UPDATER_TEST_PUBLIC_KEY_HEX);
#else
    // The production feed signer is rotated by changing this single pinned
    // value in a reviewed release. The corresponding private key never ships.
    return QByteArray::fromHex("c240ca47ad004e842ad92bdf474cf05f93f33ff99f95d940eefc330fed6a342b");
#endif
}

bool CatalogUpdateManager::verifyEd25519(const QByteArray &message,
                                         const QByteArray &signature,
                                         const QByteArray &publicKey)
{
    if (publicKey.size() != 32 || signature.size() != 64) return false;
    EVP_PKEY *key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                                reinterpret_cast<const unsigned char *>(publicKey.constData()),
                                                static_cast<size_t>(publicKey.size()));
    if (!key) return false;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    const bool initialized = context
        && EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key) == 1;
    const bool verified = initialized
        && EVP_DigestVerify(context,
                            reinterpret_cast<const unsigned char *>(signature.constData()),
                            static_cast<size_t>(signature.size()),
                            reinterpret_cast<const unsigned char *>(message.constData()),
                            static_cast<size_t>(message.size())) == 1;
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return verified;
}

QString CatalogUpdateManager::cachePath() const
{
    QString cacheDir = QString::fromLocal8Bit(qgetenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR")).trimmed();
    if (cacheDir.isEmpty()) {
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/catalog-cache");
    }
    return QDir(cacheDir).filePath(m_fileName);
}

bool CatalogUpdateManager::installSignedFeed(const QByteArray &envelope, QString *diagnostic)
{
    ParsedEnvelope parsed;
    if (!parseAndVerifyEnvelope(envelope, parsed, diagnostic)) {
        return false;
    }

    if (!validateFeedConstraints(parsed, diagnostic)) {
        return false;
    }

    const QString targetPath = cachePath();
    const QString metaPath = targetPath + QStringLiteral(".meta.json");

    CachedState cachedState;
    const qint64 sequence = parsed.payload.value(QStringLiteral("sequence")).toVariant().toLongLong();
    if (!verifyCacheAndRollback(targetPath, metaPath, sequence, cachedState, diagnostic)) {
        return false;
    }

    if (!activateCatalog(targetPath, metaPath, parsed, sequence, cachedState, diagnostic)) {
        return false;
    }

    if (diagnostic) diagnostic->clear();
    m_status = QStringLiteral("remote-verified");
    m_diagnostic.clear();
    Q_EMIT statusChanged();
    Q_EMIT catalogInstalled(sequence);
    return true;
}

bool CatalogUpdateManager::allowedFeedUrl(const QUrl &url) const
{
    if (!url.isValid() || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
        || url.host().isEmpty()) {
        return false;
    }
    const QString configuredHost = QString::fromLocal8Bit(
        qgetenv("AIUSAGE_MONITOR_CATALOG_FEED_HOST")).trimmed().toLower();
    const QString host = url.host().toLower();
    return host == QLatin1String("raw.githubusercontent.com")
        || (!configuredHost.isEmpty() && host == configuredHost);
}

void CatalogUpdateManager::persistCheckState(const QString &etag,
                                             const QString &lastModified)
{
    const QString statePath = cachePath() + QStringLiteral(".check.json");
    QDir().mkpath(QFileInfo(statePath).absolutePath());
    QSaveFile file(statePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    const QJsonObject state{
        {QStringLiteral("lastChecked"), m_lastChecked.toString(Qt::ISODateWithMs)},
        {QStringLiteral("etag"), etag},
        {QStringLiteral("lastModified"), lastModified},
    };
    const QByteArray bytes = QJsonDocument(state).toJson(QJsonDocument::Indented);
    if (file.write(bytes) == bytes.size()) file.commit();
}

void CatalogUpdateManager::finishNetworkCheck(const QByteArray &body,
                                              const QString &etag,
                                              const QString &lastModified,
                                              int httpStatus)
{
    persistCheckState(etag, lastModified);
    if (httpStatus == 304) {
        m_status = QStringLiteral("up-to-date");
        m_diagnostic.clear();
    } else if (httpStatus < 200 || httpStatus >= 300) {
        m_status = QStringLiteral("failed");
        m_diagnostic = QStringLiteral("catalog feed returned HTTP %1").arg(httpStatus);
    } else {
        QString diagnostic;
        if (!installSignedFeed(body, &diagnostic)) {
            m_status = QStringLiteral("failed");
            m_diagnostic = diagnostic;
        }
    }
    m_checking = false;
    Q_EMIT statusChanged();
}

void CatalogUpdateManager::checkForUpdate()
{
    if (m_checking) return;
    if (!m_networkUpdatesEnabled) {
        m_status = QStringLiteral("disabled");
        m_diagnostic = QStringLiteral("network catalog updates are disabled");
        Q_EMIT statusChanged();
        return;
    }
    const QUrl url(m_feedUrl);
    if (!allowedFeedUrl(url)) {
        m_status = QStringLiteral("not-configured");
        m_diagnostic = m_feedUrl.isEmpty()
            ? QStringLiteral("no HTTPS catalog feed is configured")
            : QStringLiteral("catalog feed URL is outside the HTTPS allowlist");
        Q_EMIT statusChanged();
        return;
    }

    const QString statePath = cachePath() + QStringLiteral(".check.json");
    QString etag;
    QString lastModified;
    QFile stateFile(statePath);
    if (stateFile.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        const QJsonDocument state = QJsonDocument::fromJson(stateFile.readAll(), &error);
        if (error.error == QJsonParseError::NoError && state.isObject()) {
            const QJsonObject object = state.object();
            m_lastChecked = QDateTime::fromString(
                object.value(QStringLiteral("lastChecked")).toString(), Qt::ISODateWithMs).toUTC();
            etag = object.value(QStringLiteral("etag")).toString();
            lastModified = object.value(QStringLiteral("lastModified")).toString();
        }
    }
    if (m_lastChecked.isValid()
        && m_lastChecked.secsTo(QDateTime::currentDateTimeUtc()) < 24 * 60 * 60) {
        m_status = QStringLiteral("not-due");
        m_diagnostic.clear();
        Q_EMIT statusChanged();
        return;
    }

    m_lastChecked = QDateTime::currentDateTimeUtc();
    m_checking = true;
    m_status = QStringLiteral("checking");
    m_diagnostic.clear();
    Q_EMIT statusChanged();

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Cache-Control", "no-cache");
    if (!etag.isEmpty()) request.setRawHeader("If-None-Match", etag.toUtf8());
    if (!lastModified.isEmpty()) request.setRawHeader("If-Modified-Since", lastModified.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_networkManager->get(request);
    QPointer<QNetworkReply> reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        if (!reply) return;
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl finalUrl = reply->url();
        const QString etag = QString::fromUtf8(reply->rawHeader("ETag"));
        const QString lastModified = QString::fromUtf8(reply->rawHeader("Last-Modified"));
        if (finalUrl.host().compare(url.host(), Qt::CaseInsensitive) != 0
            || !allowedFeedUrl(finalUrl)) {
            m_status = QStringLiteral("failed");
            m_diagnostic = QStringLiteral("catalog feed redirect left the HTTPS allowlist");
            m_checking = false;
            Q_EMIT statusChanged();
            reply->deleteLater();
            return;
        }
        const QByteArray body = reply->readAll();
        if (body.size() > 8 * 1024 * 1024) {
            m_status = QStringLiteral("failed");
            m_diagnostic = QStringLiteral("catalog feed exceeds the size limit");
            m_checking = false;
            Q_EMIT statusChanged();
            reply->deleteLater();
            return;
        }
        finishNetworkCheck(body, etag, lastModified, statusCode);
        reply->deleteLater();
    });
}
