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
    const QJsonObject envelopeObject = document.object();
    const QJsonObject payload = envelopeObject.value(QStringLiteral("payload")).toObject();
    if (payload.isEmpty()) {
        fail(diagnostic, QStringLiteral("signed catalog envelope has no payload"));
        return false;
    }
    if (envelopeObject.value(QStringLiteral("keyId")).toString()
            != QLatin1String("catalog-ed25519-v1")) {
        fail(diagnostic, QStringLiteral("signed catalog key id is not trusted"));
        return false;
    }

    const QByteArray canonicalPayload = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QByteArray signature = QByteArray::fromBase64(
        envelopeObject.value(QStringLiteral("signature")).toString().toUtf8());
    const QString expectedSha = envelopeObject.value(QStringLiteral("sha256")).toString().toLower();
    const QString actualSha = QString::fromLatin1(QCryptographicHash::hash(
        canonicalPayload, QCryptographicHash::Sha256).toHex());
    if (expectedSha.isEmpty() || expectedSha != actualSha
        || !verifyEd25519(canonicalPayload, signature)) {
        fail(diagnostic, QStringLiteral("signed catalog signature or digest verification failed"));
        return false;
    }

    if (payload.value(QStringLiteral("schemaVersion")).toInt() != 7
        || payload.value(QStringLiteral("runtimeScraping")).toBool(true) != false) {
        fail(diagnostic, QStringLiteral("signed catalog schema or runtime policy is invalid"));
        return false;
    }
    const qint64 sequence = payload.value(QStringLiteral("sequence")).toVariant().toLongLong();
    if (sequence < 1) {
        fail(diagnostic, QStringLiteral("signed catalog sequence is invalid"));
        return false;
    }
    if (envelopeObject.value(QStringLiteral("sequence")).toVariant().toLongLong() != sequence) {
        fail(diagnostic, QStringLiteral("signed catalog envelope sequence does not match payload"));
        return false;
    }
    if (payload.value(QStringLiteral("catalogVersion")).toString().trimmed().isEmpty()
        || !payload.value(QStringLiteral("providers")).isArray()) {
        fail(diagnostic, QStringLiteral("signed catalog required fields are invalid"));
        return false;
    }
    if (!isFutureTimestamp(envelopeObject.value(QStringLiteral("expiresAt")).toString())
        || !isFutureTimestamp(payload.value(QStringLiteral("hardExpiresAt")).toString())) {
        fail(diagnostic, QStringLiteral("signed catalog expiry is missing or expired"));
        return false;
    }
    QString appVersion = QString::fromLatin1(qgetenv("AIUSAGE_MONITOR_VERSION"));
#ifdef AIUSAGE_MONITOR_VERSION
    if (appVersion.isEmpty()) appVersion = QStringLiteral(AIUSAGE_MONITOR_VERSION);
#endif
    if (!versionAtLeast(envelopeObject.value(QStringLiteral("minAppVersion")).toString(), appVersion)) {
        fail(diagnostic, QStringLiteral("signed catalog requires a newer application version"));
        return false;
    }

    const QString target = cachePath();
    const QString metaPath = target + QStringLiteral(".meta.json");
    const bool hadPreviousCatalog = QFile::exists(target);
    const bool hadPreviousMetadata = QFile::exists(metaPath);
    const QByteArray previousCatalog = [&]() {
        QFile file(target);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    const QByteArray previousMetadata = [&]() {
        QFile file(metaPath);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    if (hadPreviousCatalog != hadPreviousMetadata) {
        fail(diagnostic, QStringLiteral("cached catalog metadata is missing"));
        return false;
    }
    QFile metaFile(metaPath);
    qint64 currentSequence = 0;
    if (hadPreviousMetadata && metaFile.open(QIODevice::ReadOnly)) {
        const QByteArray metadataBytes = metaFile.readAll();
        QJsonParseError metadataError;
        const QJsonDocument meta = QJsonDocument::fromJson(metadataBytes, &metadataError);
        if (metadataError.error != QJsonParseError::NoError || !meta.isObject()) {
            fail(diagnostic, QStringLiteral("cached catalog metadata is invalid"));
            return false;
        }
        currentSequence = meta.object().value(QStringLiteral("sequence")).toVariant().toLongLong();
        const QString cachedSha = meta.object().value(QStringLiteral("sha256")).toString().toLower();
        if (currentSequence < 1 || cachedSha.isEmpty()
            || cachedSha != QString::fromLatin1(QCryptographicHash::hash(
                previousCatalog, QCryptographicHash::Sha256).toHex())) {
            fail(diagnostic, QStringLiteral("cached catalog integrity verification failed"));
            return false;
        }
    } else if (hadPreviousMetadata) {
        fail(diagnostic, QStringLiteral("cached catalog metadata is unreadable"));
        return false;
    }
    if (sequence <= currentSequence) {
        fail(diagnostic, QStringLiteral("signed catalog sequence is a rollback or duplicate"));
        return false;
    }

    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile catalogFile(target);
    if (!catalogFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || catalogFile.write(canonicalPayload) != canonicalPayload.size()
        || !catalogFile.commit()) {
        fail(diagnostic, QStringLiteral("signed catalog could not be activated atomically"));
        return false;
    }

    QSaveFile metadataFile(metaPath);
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (hadPreviousCatalog && hadPreviousMetadata) {
            QSaveFile restoreCatalog(target);
            if (restoreCatalog.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                restoreCatalog.write(previousCatalog);
                restoreCatalog.commit();
            }
            QSaveFile restoreMetadata(metaPath);
            if (restoreMetadata.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                restoreMetadata.write(previousMetadata);
                restoreMetadata.commit();
            }
        } else {
            QFile::remove(target);
            QFile::remove(metaPath);
        }
        fail(diagnostic, QStringLiteral("signed catalog metadata could not be written"));
        return false;
    }
    const QJsonObject metadata{
        {QStringLiteral("sequence"), sequence},
        {QStringLiteral("sha256"), actualSha},
        {QStringLiteral("state"), QStringLiteral("remote_verified")},
        {QStringLiteral("installedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    };
    const QByteArray metadataBytes = QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if (metadataFile.write(metadataBytes) != metadataBytes.size() || !metadataFile.commit()) {
        if (hadPreviousCatalog && hadPreviousMetadata) {
            QSaveFile restoreCatalog(target);
            if (restoreCatalog.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                restoreCatalog.write(previousCatalog);
                restoreCatalog.commit();
            }
            QSaveFile restoreMetadata(metaPath);
            if (restoreMetadata.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                restoreMetadata.write(previousMetadata);
                restoreMetadata.commit();
            }
        } else {
            QFile::remove(target);
            QFile::remove(metaPath);
        }
        fail(diagnostic, QStringLiteral("signed catalog metadata could not be activated atomically"));
        return false;
    }
    if (diagnostic) diagnostic->clear();
    m_status = QStringLiteral("remote-verified");
    m_diagnostic.clear();
    Q_EMIT statusChanged();
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
