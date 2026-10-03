#include "localactivitymonitorbase.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#include <QFile>
#include <QFutureWatcher>
#ifdef Q_OS_LINUX
#include <sys/stat.h>
#endif
#include <QtConcurrentRun>
#include <chrono>

namespace
{
struct ActivityScan
{
    QHash<QString, QString> files;
    QStringList watchPaths;
    bool incomplete = false;
    bool activitySinceBaseline = false;
};

ActivityScan scanActivity(const QStringList &roots, const QStringList &ignoredSuffixes,
                          const std::shared_ptr<std::atomic_bool> &cancel, qint64 baselineStartedNs)
{
    ActivityScan result;
    for (const auto &root : roots)
    {
        if (cancel->load())
            break;
        QFileInfo rootInfo(root);
        QString anchor = rootInfo.absoluteFilePath();
        while (!QFileInfo::exists(anchor))
        {
            const QString parent = QFileInfo(anchor).absolutePath();
            if (parent == anchor)
                break;
            anchor = parent;
        }
        result.watchPaths.append(anchor);
        int entries = 1;
        const auto visit = [&](const QFileInfo &entry)
        {
            // Ignore metadata files for activity, but watch their parent for
            // replacement.
            bool ignored = false;
            for (const auto &suffix : ignoredSuffixes)
                ignored |= entry.filePath().endsWith(suffix);
            if (entry.isDir() || (entry.isFile() && !ignored))
                result.watchPaths.append(entry.absoluteFilePath());
            if (entry.isFile() && !ignored)
            {
                const auto modified = entry.lastModified();
                qint64 changedAtNs = modified.toMSecsSinceEpoch() * 1000000;
                QString fingerprint = QString::number(modified.toMSecsSinceEpoch()) + QLatin1Char(':') +
                                      QString::number(entry.size());
#ifdef Q_OS_LINUX
                // Inode and nanosecond change time also detect same-size writes
                // and atomic replacements within one millisecond.
                struct stat metadata{};
                if (::stat(QFile::encodeName(entry.absoluteFilePath()).constData(), &metadata) == 0)
                {
                    changedAtNs = qint64(metadata.st_ctim.tv_sec) * 1000000000 + metadata.st_ctim.tv_nsec;
                    fingerprint += QLatin1Char(':') + QString::number(metadata.st_ino) + QLatin1Char(':') +
                                   QString::number(metadata.st_ctim.tv_sec) + QLatin1Char(':') +
                                   QString::number(metadata.st_ctim.tv_nsec);
                }
#endif
                result.activitySinceBaseline |= baselineStartedNs > 0 && changedAtNs > baselineStartedNs;
                result.files.insert(entry.absoluteFilePath(), fingerprint);
            }
        };
        if (rootInfo.isFile())
            visit(rootInfo);
        if (!rootInfo.isDir())
            continue;
        QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (!cancel->load() && entries < 4000 && it.hasNext())
        {
            it.next();
            visit(it.fileInfo());
            ++entries;
        }
        result.incomplete |= it.hasNext();
    }
    result.watchPaths.removeDuplicates();
    return result;
}
} // namespace

LocalActivityMonitorBase::LocalActivityMonitorBase(QObject *parent)
    : SubscriptionToolBackend(parent), m_watcher(new QFileSystemWatcher(this)),
      m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(5000);
    connect(m_debounceTimer, &QTimer::timeout, this, &LocalActivityMonitorBase::requestScan);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &LocalActivityMonitorBase::onDirectoryChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &LocalActivityMonitorBase::onFileChanged);
    connect(this, &SubscriptionToolBackend::enabledChanged, this,
            [this]()
            {
                if (!isEnabled())
                    stopWatching();
            });
    connect(this, &SubscriptionToolBackend::installedChanged, this,
            [this]()
            {
                if (isEnabled() && isInstalled())
                    setupWatcher();
                else
                    stopWatching();
            });
}

LocalActivityMonitorBase::~LocalActivityMonitorBase()
{
    if (m_cancelScan)
        m_cancelScan->store(true);
}

QString LocalActivityMonitorBase::watchDiagnosticCode() const { return m_watchDiagnosticCode; }

void LocalActivityMonitorBase::updateWatchHealth(bool incomplete)
{
    const QString code = incomplete ? QStringLiteral("watch_incomplete") : QString();
    if (code != m_watchDiagnosticCode)
    {
        m_watchDiagnosticCode = code;
        Q_EMIT watchHealthChanged();
    }
}

void LocalActivityMonitorBase::setInstallExecutableNames(const QStringList &names)
{
    m_installExecutableNames = names;
}

void LocalActivityMonitorBase::setInstallPaths(const QStringList &paths) { m_installPaths = paths; }

void LocalActivityMonitorBase::setWatchedPaths(const QStringList &paths)
{
    if (m_watchedPaths == paths)
        return;
    m_watchedPaths = paths;
    if (isEnabled() && isInstalled())
    {
        stopWatching();
        setupWatcher();
    }
}

void LocalActivityMonitorBase::setIgnoredPathSuffixes(const QStringList &suffixes)
{
    if (m_ignoredPathSuffixes == suffixes)
        return;
    m_ignoredPathSuffixes = suffixes;
    if (isEnabled() && isInstalled())
    {
        stopWatching();
        setupWatcher();
    }
}

void LocalActivityMonitorBase::setDebounceIntervalMs(int intervalMs)
{
    m_debounceTimer->setInterval(qMax(250, intervalMs));
}

QStringList LocalActivityMonitorBase::watchedPaths() const { return m_watchedPaths; }

void LocalActivityMonitorBase::checkToolInstalled()
{
    if (qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO"))
    {
        setInstalled(true);
        if (isEnabled())
            setupWatcher();
        return;
    }

    bool found = false;

    for (const QString &name : std::as_const(m_installExecutableNames))
    {
        if (!QStandardPaths::findExecutable(name).isEmpty())
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        for (const QString &path : std::as_const(m_installPaths))
        {
            if (QFileInfo::exists(path))
            {
                found = true;
                break;
            }
        }
    }

    setInstalled(found);

    if (found && isEnabled())
    {
        setupWatcher();
    }
}

void LocalActivityMonitorBase::detectActivity()
{
    if (isEnabled() && isInstalled() && !m_debounceTimer->isActive())
        m_debounceTimer->start();
}

void LocalActivityMonitorBase::requestScan()
{
    if (!isEnabled() || !isInstalled())
        return;
    if (m_scanRunning)
    {
        m_scanAgain = true;
        return;
    }
    m_scanRunning = true;
    m_scanAgain = false;
    const quint64 generation = m_generation;
    const auto cancellation = m_cancelScan;
    const qint64 baselineStartedNs = m_baselinePending ? m_baselineStartedNs : 0;
    auto *future = new QFutureWatcher<ActivityScan>(this);
    connect(future, &QFutureWatcher<ActivityScan>::finished, this,
            [this, future, generation]()
            {
                const auto result = future->result();
                future->deleteLater();
                if (generation != m_generation)
                    return;
                m_scanRunning = false;
                if (!isEnabled() || !isInstalled())
                    return;
                const auto previousPaths = m_watcher->directories() + m_watcher->files();
                const QSet<QString> previous(previousPaths.cbegin(), previousPaths.cend());
                const QSet<QString> desired(result.watchPaths.cbegin(), result.watchPaths.cend());
                QStringList removed;
                for (const auto &path : previousPaths)
                    if (!desired.contains(path))
                        removed.append(path);
                if (!removed.isEmpty())
                    m_watcher->removePaths(removed);
                QStringList added;
                for (const auto &path : result.watchPaths)
                    if (!previous.contains(path))
                        added.append(path);
                bool incomplete = result.incomplete;
                if (!added.isEmpty())
                    incomplete |= !m_watcher->addPaths(added).isEmpty();
                updateWatchHealth(incomplete);
                const bool establishingBaseline = m_baselinePending;
                bool changed = establishingBaseline && result.activitySinceBaseline;
                if (!m_baselinePending)
                {
                    for (auto it = result.files.cbegin(); it != result.files.cend(); ++it)
                    {
                        if (m_fileSnapshot.value(it.key()) != it.value())
                        {
                            changed = true;
                            break;
                        }
                    }
                }
                m_baselinePending = false;
                m_fileSnapshot = result.files;
                if (changed)
                    incrementUsage();
                // Close the gap between reading metadata and installing nested
                // watches: a second scan compares the captured baseline after
                // all watches are active. This is one follow-up, never polling.
                if (establishingBaseline)
                    requestScan();
                else if (m_scanAgain)
                    detectActivity();
            });
    future->setFuture(
        QtConcurrent::run([roots = m_watchedPaths, ignored = m_ignoredPathSuffixes, cancellation, baselineStartedNs]()
                          { return scanActivity(roots, ignored, cancellation, baselineStartedNs); }));
}

void LocalActivityMonitorBase::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path);
    detectActivity();
}

void LocalActivityMonitorBase::onFileChanged(const QString &path)
{
    Q_UNUSED(path);
    detectActivity();
}

void LocalActivityMonitorBase::stopWatching()
{
    m_watching = false;
    ++m_generation;
    if (m_cancelScan)
        m_cancelScan->store(true);
    m_debounceTimer->stop();
    const auto paths = m_watcher->directories() + m_watcher->files();
    if (!paths.isEmpty())
        m_watcher->removePaths(paths);
    m_scanRunning = false;
    m_scanAgain = false;
    m_baselinePending = true;
    m_fileSnapshot.clear();
    updateWatchHealth(false);
}

void LocalActivityMonitorBase::setupWatcher()
{
    if (m_watching)
        return;
    stopWatching();
    m_watching = true;
    m_baselineStartedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
    m_cancelScan = std::make_shared<std::atomic_bool>(false);
    // Establish root/ancestor watches before the asynchronous baseline, so newly
    // created session trees are discovered without polling.
    for (const auto &root : std::as_const(m_watchedPaths))
    {
        QString anchor = QFileInfo(root).absoluteFilePath();
        while (!QFileInfo::exists(anchor))
        {
            const QString parent = QFileInfo(anchor).absolutePath();
            if (parent == anchor)
                break;
            anchor = parent;
        }
        if (!m_watcher->directories().contains(anchor) && !m_watcher->files().contains(anchor))
            m_watcher->addPath(anchor);
    }
    requestScan();
}
