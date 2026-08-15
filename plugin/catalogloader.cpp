#include "catalogloader.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>
#include <QVariantMap>
#include <QDebug>

#include <utility>

namespace {
constexpr int DEFAULT_STALE_DAYS = 30;

QString sourceCatalogCandidate(const QDir &dir, const QString &fileName)
{
    return dir.filePath(QStringLiteral("package/contents/catalog/%1").arg(fileName));
}

bool verifiedCacheCandidate(const QString &candidate)
{
    if (!QFile::exists(candidate)) {
        return false;
    }
    QFile metaFile(candidate + QStringLiteral(".meta.json"));
    if (!metaFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument meta = QJsonDocument::fromJson(metaFile.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !meta.isObject()) {
        return false;
    }
    QFile catalogFile(candidate);
    if (!catalogFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QString expected = meta.object().value(QStringLiteral("sha256")).toString().toLower();
    const QString actual = QString::fromLatin1(
        QCryptographicHash::hash(catalogFile.readAll(), QCryptographicHash::Sha256).toHex());
    return !expected.isEmpty() && expected == actual;
}

void countFlagsInValue(const QJsonValue &value, int &manualReview, int &sourceConflict)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("needsManualReview")).toBool(false)) {
            manualReview++;
        }
        if (object.value(QStringLiteral("sourceConflict")).toBool(false)) {
            sourceConflict++;
        }
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            countFlagsInValue(it.value(), manualReview, sourceConflict);
        }
        return;
    }

    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            countFlagsInValue(entry, manualReview, sourceConflict);
        }
    }
}

QString firstSourceRef(const QJsonObject &object)
{
    const QJsonArray refs = object.value(QStringLiteral("sourceRefs")).toArray();
    if (refs.isEmpty()) {
        return QString();
    }
    const QJsonObject ref = refs.first().toObject();
    const QString label = ref.value(QStringLiteral("label")).toString();
    const QString reviewedAt = ref.value(QStringLiteral("reviewedAt")).toString();
    if (label.isEmpty()) {
        return reviewedAt;
    }
    if (reviewedAt.isEmpty()) {
        return label;
    }
    return QStringLiteral("%1 (%2)").arg(label, reviewedAt);
}

QVariantList collectReviewItems(const QJsonObject &root)
{
    QVariantList result;
    const QString collectionKey = root.contains(QStringLiteral("providers"))
        ? QStringLiteral("providers")
        : QStringLiteral("tools");
    const QJsonArray entries = root.value(collectionKey).toArray();

    for (const QJsonValue &entryValue : entries) {
        const QJsonObject entry = entryValue.toObject();
        const bool needsReview = entry.value(QStringLiteral("needsManualReview")).toBool(false);
        const bool sourceConflict = entry.value(QStringLiteral("sourceConflict")).toBool(false);
        if (!needsReview && !sourceConflict) {
            continue;
        }

        QVariantMap row;
        row.insert(QStringLiteral("key"), entry.value(QStringLiteral("key")).toString());
        row.insert(QStringLiteral("label"), entry.value(QStringLiteral("label")).toString());
        row.insert(QStringLiteral("needsManualReview"), needsReview);
        row.insert(QStringLiteral("sourceConflict"), sourceConflict);
        row.insert(QStringLiteral("dataQuality"), entry.value(QStringLiteral("dataQuality")).toString());
        row.insert(QStringLiteral("pricingFreshness"), entry.value(QStringLiteral("pricingFreshness")).toString());
        row.insert(QStringLiteral("reviewReason"), entry.value(QStringLiteral("reviewReason")).toString());
        row.insert(QStringLiteral("sourceConflictReason"), entry.value(QStringLiteral("sourceConflictReason")).toString());
        row.insert(QStringLiteral("source"), firstSourceRef(entry));
        result << row;
    }

    return result;
}
} // namespace

CatalogLoader::CatalogLoader(QString fileName, int expectedSchemaVersion, QObject *parent)
    : QObject(parent)
    , m_fileName(std::move(fileName))
    , m_expectedSchemaVersion(expectedSchemaVersion)
{
}

bool CatalogLoader::load()
{
    m_loadAttempted = true;
    m_valid = false;
    m_schemaVersion = 0;
    m_catalogVersion.clear();
    m_lastReviewed.clear();
    m_runtimeScraping = false;
    m_manualReviewCount = 0;
    m_sourceConflictCount = 0;
    m_reviewItems.clear();
    m_catalogPath.clear();
    m_sourceFingerprint.clear();
    m_verificationState.clear();
    m_sequence = 0;
    m_hardExpiresAt.clear();
    m_estimatesAllowed = true;
    m_freshnessSloDays = DEFAULT_STALE_DAYS;
    m_diagnostics.clear();
    m_root = QJsonObject();

    const QString path = locateCatalogFile(m_fileName);
    if (path.isEmpty()) {
        m_diagnostics << QStringLiteral("Catalog file not found: %1").arg(m_fileName);
        qWarning() << "AI Usage Monitor:" << m_diagnostics.constLast();
        Q_EMIT statusChanged();
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_diagnostics << QStringLiteral("Catalog file is not readable: %1").arg(path);
        qWarning() << "AI Usage Monitor:" << m_diagnostics.constLast();
        Q_EMIT statusChanged();
        return false;
    }
    constexpr qint64 MaxCatalogBytes = 4 * 1024 * 1024;
    if (file.size() <= 0 || file.size() > MaxCatalogBytes) {
        m_diagnostics << QStringLiteral("Catalog size is outside the allowed 1..4194304 byte range");
        qWarning() << "AI Usage Monitor:" << m_diagnostics.constLast();
        Q_EMIT statusChanged();
        return false;
    }

    const QByteArray catalogBytes = file.readAll();
    m_sourceFingerprint = QString::fromLatin1(
        QCryptographicHash::hash(catalogBytes, QCryptographicHash::Sha256).toHex());
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(catalogBytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        m_diagnostics << QStringLiteral("Catalog JSON is invalid: %1").arg(error.errorString());
        qWarning() << "AI Usage Monitor:" << m_diagnostics.constLast();
        Q_EMIT statusChanged();
        return false;
    }

    m_root = document.object();
    m_catalogPath = path;
    m_schemaVersion = m_root.value(QStringLiteral("schemaVersion")).toInt(0);
    m_catalogVersion = m_root.value(QStringLiteral("catalogVersion")).toString();
    m_lastReviewed = m_root.value(QStringLiteral("lastReviewed")).toString();
    m_runtimeScraping = m_root.value(QStringLiteral("runtimeScraping")).toBool(true);
    m_sequence = m_root.value(QStringLiteral("sequence")).toVariant().toLongLong();
    m_hardExpiresAt = m_root.value(QStringLiteral("hardExpiresAt")).toString();
    m_freshnessSloDays = qMax(1, m_root.value(QStringLiteral("freshnessSloDays")).toInt(DEFAULT_STALE_DAYS));
    if (verifiedCacheCandidate(path)) {
        m_verificationState = QStringLiteral("remote_verified");
        QFile metadataFile(path + QStringLiteral(".meta.json"));
        if (metadataFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument metadata = QJsonDocument::fromJson(metadataFile.readAll());
            const QString state = metadata.object().value(QStringLiteral("state")).toString();
            if (state == QLatin1String("remote_verified")
                || state == QLatin1String("offline_cached")) {
                m_verificationState = state;
            }
        }
    } else {
        m_verificationState = m_root.value(QStringLiteral("verificationState")).toString(
            QStringLiteral("packaged"));
    }
    m_estimatesAllowed = m_root.value(QStringLiteral("estimatesAllowed")).toBool(true);
    const QDateTime expiry = QDateTime::fromString(m_hardExpiresAt, Qt::ISODateWithMs).toUTC();
    if (!expiry.isValid()) {
        const QDateTime isoExpiry = QDateTime::fromString(m_hardExpiresAt, Qt::ISODate).toUTC();
        if (isoExpiry.isValid() && QDateTime::currentDateTimeUtc() >= isoExpiry) {
            m_estimatesAllowed = false;
            m_verificationState = QStringLiteral("expired");
        }
    } else if (QDateTime::currentDateTimeUtc() >= expiry) {
        m_estimatesAllowed = false;
        m_verificationState = QStringLiteral("expired");
    }
    countReviewFlags();

    if (m_schemaVersion != m_expectedSchemaVersion) {
        m_diagnostics << QStringLiteral("Catalog schema mismatch: got %1 expected %2")
            .arg(m_schemaVersion)
            .arg(m_expectedSchemaVersion);
    }
    if (m_runtimeScraping) {
        m_diagnostics << QStringLiteral("Catalog runtimeScraping must be false");
    }
    if (!QDate::fromString(m_lastReviewed, Qt::ISODate).isValid()) {
        m_diagnostics << QStringLiteral("Catalog lastReviewed is not an ISO date");
    }
    if (!m_root.value(QStringLiteral("providers")).isArray()
        && m_fileName.startsWith(QLatin1String("providers"))) {
        m_diagnostics << QStringLiteral("Provider catalog must contain a providers array");
    }
    if (m_expectedSchemaVersion >= 7 && m_sequence <= 0) {
        m_diagnostics << QStringLiteral("Catalog sequence must be a positive integer");
    }
    if (m_expectedSchemaVersion >= 7 && !QDateTime::fromString(m_hardExpiresAt, Qt::ISODate).isValid()
        && !QDateTime::fromString(m_hardExpiresAt, Qt::ISODateWithMs).isValid()) {
        m_diagnostics << QStringLiteral("Catalog hardExpiresAt is not an ISO timestamp");
    }

    m_valid = m_diagnostics.isEmpty();
    if (!m_valid) {
        for (const QString &diagnostic : std::as_const(m_diagnostics)) {
            qWarning() << "AI Usage Monitor:" << diagnostic;
        }
    }

    Q_EMIT statusChanged();
    return m_valid;
}

bool CatalogLoader::ensureLoaded() const
{
    if (!m_loadAttempted) {
        return const_cast<CatalogLoader *>(this)->load();
    }
    return m_valid;
}

bool CatalogLoader::isValid() const
{
    ensureLoaded();
    return m_valid;
}

bool CatalogLoader::stale() const
{
    ensureLoaded();
    return isStale(m_freshnessSloDays);
}

bool CatalogLoader::isStale(int maxAgeDays) const
{
    ensureLoaded();
    const QDate reviewed = QDate::fromString(m_lastReviewed, Qt::ISODate);
    if (!reviewed.isValid()) {
        return true;
    }
    return reviewed.daysTo(QDate::currentDate()) > maxAgeDays;
}

int CatalogLoader::schemaVersion() const
{
    ensureLoaded();
    return m_schemaVersion;
}

QString CatalogLoader::catalogVersion() const
{
    ensureLoaded();
    return m_catalogVersion;
}

QString CatalogLoader::lastReviewed() const
{
    ensureLoaded();
    return m_lastReviewed;
}

bool CatalogLoader::runtimeScraping() const
{
    ensureLoaded();
    return m_runtimeScraping;
}

int CatalogLoader::manualReviewCount() const
{
    ensureLoaded();
    return m_manualReviewCount;
}

int CatalogLoader::sourceConflictCount() const
{
    ensureLoaded();
    return m_sourceConflictCount;
}

QVariantList CatalogLoader::reviewItems() const
{
    ensureLoaded();
    return m_reviewItems;
}

QString CatalogLoader::catalogPath() const
{
    ensureLoaded();
    return m_catalogPath;
}

QString CatalogLoader::sourceFingerprint() const
{
    ensureLoaded();
    return m_sourceFingerprint;
}

QString CatalogLoader::verificationState() const
{
    ensureLoaded();
    return m_verificationState;
}

qint64 CatalogLoader::sequence() const
{
    ensureLoaded();
    return m_sequence;
}

QString CatalogLoader::hardExpiresAt() const
{
    ensureLoaded();
    return m_hardExpiresAt;
}

bool CatalogLoader::estimatesAllowed() const
{
    ensureLoaded();
    return m_estimatesAllowed;
}

int CatalogLoader::freshnessSloDays() const
{
    ensureLoaded();
    return m_freshnessSloDays;
}

QStringList CatalogLoader::diagnostics() const
{
    ensureLoaded();
    return m_diagnostics;
}

QJsonObject CatalogLoader::rootObject() const
{
    ensureLoaded();
    return m_root;
}

QString CatalogLoader::locateCatalogFile(const QString &fileName)
{
    const QString envDir = QString::fromLocal8Bit(qgetenv("AIUSAGE_MONITOR_CATALOG_DIR")).trimmed();
    if (!envDir.isEmpty()) {
        const QString candidate = QDir(envDir).filePath(fileName);
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    const QStringList cacheDirs = {
        QString::fromLocal8Bit(qgetenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR")).trimmed(),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/catalog-cache")
    };
    for (const QString &cacheDir : cacheDirs) {
        if (cacheDir.isEmpty()) {
            continue;
        }
        const QString candidate = QDir(cacheDir).filePath(fileName);
        if (verifiedCacheCandidate(candidate)) {
            return candidate;
        }
    }

    const QByteArray developmentOverride = qgetenv("AIUSAGE_MONITOR_DEVELOPMENT");
    const bool allowSourceTree = developmentOverride == "1"
        || QCoreApplication::applicationDirPath().contains(QStringLiteral("/build"));
    if (allowSourceTree) {
        QDir dir = QDir::current();
        while (true) {
            const QString candidate = sourceCatalogCandidate(dir, fileName);
            if (QFile::exists(candidate)) {
                return candidate;
            }
            if (!dir.cdUp()) {
                break;
            }
        }

        dir = QDir(QCoreApplication::applicationDirPath());
        while (true) {
            const QString candidate = sourceCatalogCandidate(dir, fileName);
            if (QFile::exists(candidate)) {
                return candidate;
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    const QString installed = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("plasma/plasmoids/com.github.loofi.aiusagemonitor/contents/catalog/%1").arg(fileName));
    if (!installed.isEmpty()) {
        return installed;
    }

    return QString();
}

void CatalogLoader::countReviewFlags()
{
    m_manualReviewCount = 0;
    m_sourceConflictCount = 0;
    countFlagsInValue(QJsonValue(m_root), m_manualReviewCount, m_sourceConflictCount);
    m_reviewItems = collectReviewItems(m_root);
}
