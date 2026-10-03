#ifndef LOCALACTIVITYMONITORBASE_H
#define LOCALACTIVITYMONITORBASE_H

#include "subscriptiontoolbackend.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <memory>

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
    QString watchDiagnosticCode() const override;

protected:
    void setInstallExecutableNames(const QStringList &names);
    void setInstallPaths(const QStringList &paths);
    void setWatchedPaths(const QStringList &paths);
    void setIgnoredPathSuffixes(const QStringList &suffixes);
    void setDebounceIntervalMs(int intervalMs);

    QStringList watchedPaths() const;

private Q_SLOTS:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);

private:
    void setupWatcher();
    void stopWatching();
    void requestScan();
    void updateWatchHealth(bool incomplete);

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounceTimer = nullptr;
    QStringList m_installExecutableNames;
    QStringList m_installPaths;
    QStringList m_watchedPaths;
    QStringList m_ignoredPathSuffixes;
    QHash<QString, QString> m_fileSnapshot;
    bool m_watching = false;
    bool m_baselinePending = true;
    qint64 m_baselineStartedNs = 0;
    bool m_scanRunning = false;
    bool m_scanAgain = false;
    quint64 m_generation = 0;
    std::shared_ptr<std::atomic_bool> m_cancelScan;
    QString m_watchDiagnosticCode;
};

#endif // LOCALACTIVITYMONITORBASE_H
