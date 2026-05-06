#include "catalogloader.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>
#include <QDebug>

#include <utility>

namespace {
constexpr int DEFAULT_STALE_DAYS = 30;

QString sourceCatalogCandidate(const QDir &dir, const QString &fileName)
{
    return dir.filePath(QStringLiteral("package/contents/catalog/%1").arg(fileName));
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
    m_catalogPath.clear();
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

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
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
    return isStale(DEFAULT_STALE_DAYS);
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

QString CatalogLoader::catalogPath() const
{
    ensureLoaded();
    return m_catalogPath;
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
}
