#ifndef LOCALACTIVITYMONITORBASE_H
#define LOCALACTIVITYMONITORBASE_H

#include "subscriptiontoolbackend.h"

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QTimer>

/**
 * Shared helper for local subscription-tool monitors that infer usage from
 * filesystem activity under one or more tool-specific directories.
 */
class LocalActivityMonitorBase : public SubscriptionToolBackend
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit LocalActivityMonitorBase(QObject *parent = nullptr);
    ~LocalActivityMonitorBase() override;

    Q_INVOKABLE void checkToolInstalled() override;
    Q_INVOKABLE void detectActivity() override;

protected:
    void setInstallExecutableNames(const QStringList &names);
    void setInstallPaths(const QStringList &paths);
    void setWatchedPaths(const QStringList &paths);
    void setIgnoredPathSuffixes(const QStringList &suffixes);
    void setDebounceIntervalMs(int intervalMs);

    QStringList watchedPaths() const;
    QDateTime latestKnownModification() const;
    void setLatestKnownModification(const QDateTime &time);

    static QDateTime latestModification(const QString &path, int maxEntries = 4000);

private Q_SLOTS:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);

private:
    void setupWatcher();
    void scheduleIncrement(const QDateTime &modified);
    bool shouldIgnorePath(const QString &path) const;

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounceTimer = nullptr;
    QStringList m_installExecutableNames;
    QStringList m_installPaths;
    QStringList m_watchedPaths;
    QStringList m_ignoredPathSuffixes;
    QDateTime m_lastKnownModification;
    bool m_pendingIncrement = false;
};

#endif // LOCALACTIVITYMONITORBASE_H
