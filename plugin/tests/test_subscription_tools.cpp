#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QFileSystemWatcher>
#include <QSemaphore>
#include <QScopeGuard>
#include <QThreadPool>
#include <QtConcurrentRun>

#include "claudecodemonitor.h"
#include "codexclimonitor.h"
#include "copilotmonitor.h"

class EnvVarGuard
{
public:
    explicit EnvVarGuard(const char *name)
        : m_name(name)
        , m_oldValue(qgetenv(name))
        , m_hadValue(!m_oldValue.isNull())
    {
    }

    ~EnvVarGuard()
    {
        if (m_hadValue) {
            qputenv(m_name.constData(), m_oldValue);
        } else {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    QByteArray m_oldValue;
    bool m_hadValue = false;
};

class TestActivityMonitor : public CopilotMonitor
{
public:
    explicit TestActivityMonitor(const QString &root)
    {
        setInstallPaths({root});
        setWatchedPaths({root});
        setIgnoredPathSuffixes({QStringLiteral(".log")});
        setDebounceIntervalMs(250);
    }
    void checkToolInstalled() override { LocalActivityMonitorBase::checkToolInstalled(); }
};

class SubscriptionToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void planDefaults();
    void installDetectionWithTemporaryHome();
    void usageIncrementAndReset();
    void copilotDetectActivityIncrementsUsage();
    void browserSyncEmptyCookieDiagnostics();
    void browserSyncChromeEmptyCookieDiagnostics();
    void codexSyncWithoutLiveQuotaKeepsConfiguredPro();
    void codexLiveQuotaPayload();
    void copilotBillingModeLabels();
    void copilotOrgMetricsRequireEnabledMonitor();
    void localWatchLifecycleAndReplacement();
    void localWatchBounds();
    void localWatchActivityDuringBaseline_data();
    void localWatchActivityDuringBaseline();
};

void SubscriptionToolsTest::planDefaults()
{
    ClaudeCodeMonitor claude;
    QCOMPARE(claude.defaultLimitForPlan(QStringLiteral("Pro")), 0);
    QCOMPARE(claude.defaultLimitForPlan(QStringLiteral("Max 5x")), 0);
    QCOMPARE(claude.defaultSecondaryLimitForPlan(QStringLiteral("Max 5x")), 0);
    QCOMPARE(claude.defaultCostForPlan(QStringLiteral("Max 20x")), 200.0);
    claude.setPlanTier(QStringLiteral("max_20x"));
    QVERIFY(!claude.quotaWindows().isEmpty());

    CodexCliMonitor codex;
    QCOMPARE(codex.defaultLimitForPlan(QStringLiteral("Plus")), 0);
    QCOMPARE(codex.defaultLimitForPlan(QStringLiteral("Pro")), 0);
    QCOMPARE(codex.defaultSecondaryLimitForPlan(QStringLiteral("Pro")), 0);
    QCOMPARE(codex.defaultCostForPlan(QStringLiteral("Pro 100")), 100.0);
    QCOMPARE(codex.defaultCostForPlan(QStringLiteral("Pro 200")), 200.0);
    QCOMPARE(codex.defaultCostForPlan(QStringLiteral("Pro 500")), 500.0);
    QVERIFY(codex.availablePlans().size() > 4);
    QCOMPARE(codex.availablePlans().at(3), QStringLiteral("Pro 100"));
    QCOMPARE(codex.availablePlans().at(4), QStringLiteral("Pro 200"));
    QCOMPARE(codex.availablePlans().at(5), QStringLiteral("Pro 500"));
    codex.setPlanTier(QStringLiteral("pro"));
    QVERIFY(!codex.quotaWindows().isEmpty());

    CopilotMonitor copilot;
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Free")), 0);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Pro")), 0);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Pro+")), 0);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Business")), 0);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Enterprise")), 0);
    QCOMPARE(copilot.defaultCostForPlan(QStringLiteral("Pro+")), 39.0);
    QCOMPARE(copilot.defaultCostForPlan(QStringLiteral("Business")), 19.0);
}

void SubscriptionToolsTest::installDetectionWithTemporaryHome()
{
    QTemporaryDir tempHome;
    QVERIFY(tempHome.isValid());

    EnvVarGuard homeGuard("HOME");
    EnvVarGuard pathGuard("PATH");

    qputenv("HOME", tempHome.path().toUtf8());
    qputenv("PATH", QByteArray());

    ClaudeCodeMonitor claude;
    CodexCliMonitor codex;
    CopilotMonitor copilot;

    claude.checkToolInstalled();
    codex.checkToolInstalled();
    copilot.checkToolInstalled();

    QVERIFY(!claude.isInstalled());
    QVERIFY(!codex.isInstalled());
    QVERIFY(!copilot.isInstalled());

    QVERIFY(QDir().mkpath(tempHome.path() + QStringLiteral("/.claude")));
    QVERIFY(QDir().mkpath(tempHome.path() + QStringLiteral("/.codex")));
    QVERIFY(QDir().mkpath(tempHome.path() + QStringLiteral("/.vscode/extensions/github.copilot-test")));

    claude.checkToolInstalled();
    codex.checkToolInstalled();
    copilot.checkToolInstalled();

    QVERIFY(claude.isInstalled());
    QVERIFY(codex.isInstalled());
    QVERIFY(copilot.isInstalled());
}

void SubscriptionToolsTest::usageIncrementAndReset()
{
    CodexCliMonitor codex;
    codex.setUsageLimit(2);

    codex.incrementUsage();
    codex.incrementUsage();

    QCOMPARE(codex.usageCount(), 2);
    QVERIFY(codex.isLimitReached());

    codex.resetUsage();
    QCOMPARE(codex.usageCount(), 0);
    QVERIFY(!codex.isLimitReached());
}

void SubscriptionToolsTest::copilotDetectActivityIncrementsUsage()
{
    QTemporaryDir tempHome;
    QVERIFY(tempHome.isValid());

    EnvVarGuard homeGuard("HOME");
    qputenv("HOME", tempHome.path().toUtf8());

    const QString stateDir = tempHome.path() + QStringLiteral("/.config/Code/User/globalStorage/github.copilot-chat");
    QVERIFY(QDir().mkpath(stateDir));
    QVERIFY(QDir().mkpath(tempHome.path() + QStringLiteral("/.vscode/extensions/github.copilot")));
    const QString stateFilePath = stateDir + QStringLiteral("/state.db");

    QFile stateFile(stateFilePath);
    QVERIFY(stateFile.open(QIODevice::WriteOnly | QIODevice::Text));
    stateFile.write("{\"status\":\"idle\"}\n");
    stateFile.close();

    CopilotMonitor copilot;
    copilot.setUsageLimit(10);
    copilot.setEnabled(true);
    copilot.checkToolInstalled();
    QVERIFY(copilot.isInstalled());

    QSignalSpy activitySpy(&copilot, &SubscriptionToolBackend::activityDetected);
    QSignalSpy usageSpy(&copilot, &SubscriptionToolBackend::usageUpdated);

    // Installation establishes the baseline and watches files automatically.
    auto *watcher = copilot.findChild<QFileSystemWatcher *>();
    QVERIFY(watcher);
    QTRY_VERIFY_WITH_TIMEOUT(watcher->files().contains(stateFilePath), 3000);
    QCOMPARE(copilot.usageCount(), 0);

    QTest::qWait(2100);
    QVERIFY(stateFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    stateFile.write("{\"status\":\"active\"}\n");
    stateFile.close();

    // Detect filesystem activity without a manual scan.
    QTRY_COMPARE_WITH_TIMEOUT(copilot.usageCount(), 1, 3000);

    QCOMPARE(copilot.usageCount(), 1);
    QCOMPARE(activitySpy.count(), 1);
    QVERIFY(usageSpy.count() >= 1);
}

void SubscriptionToolsTest::browserSyncEmptyCookieDiagnostics()
{
    QTemporaryDir tempHome;
    QVERIFY(tempHome.isValid());
    EnvVarGuard homeGuard("HOME");
    qputenv("HOME", tempHome.path().toUtf8());

    ClaudeCodeMonitor claude;
    claude.setEnabled(true);
    QSignalSpy claudeCompletedSpy(&claude, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy claudeDiagnosticSpy(&claude, &SubscriptionToolBackend::syncDiagnostic);

    claude.syncFromBrowser(QString(), 0);

    QCOMPARE(claudeCompletedSpy.count(), 1);
    QCOMPARE(claudeDiagnosticSpy.count(), 1);
    QCOMPARE(claude.syncStatus(), QStringLiteral("Not logged in"));

    const QList<QVariant> claudeCompletionArgs = claudeCompletedSpy.takeFirst();
    QCOMPARE(claudeCompletionArgs.at(0).toBool(), false);
    QVERIFY(claudeCompletionArgs.at(1).toString().contains(QStringLiteral("Not logged in"), Qt::CaseInsensitive));

    const QList<QVariant> claudeDiagnosticArgs = claudeDiagnosticSpy.takeFirst();
    QCOMPARE(claudeDiagnosticArgs.at(0).toString(), QStringLiteral("Claude Code"));
    QCOMPARE(claudeDiagnosticArgs.at(1).toString(), QStringLiteral("not_logged_in"));

    CodexCliMonitor codex;
    codex.setEnabled(true);
    QSignalSpy codexCompletedSpy(&codex, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy codexDiagnosticSpy(&codex, &SubscriptionToolBackend::syncDiagnostic);

    codex.syncFromBrowser(QString(), 0);

    QCOMPARE(codexCompletedSpy.count(), 1);
    QCOMPARE(codexDiagnosticSpy.count(), 1);
    QCOMPARE(codex.syncStatus(), QStringLiteral("Not logged in"));

    const QList<QVariant> codexCompletionArgs = codexCompletedSpy.takeFirst();
    QCOMPARE(codexCompletionArgs.at(0).toBool(), false);
    QVERIFY(codexCompletionArgs.at(1).toString().contains(QStringLiteral("Not logged in"), Qt::CaseInsensitive));

    const QList<QVariant> codexDiagnosticArgs = codexDiagnosticSpy.takeFirst();
    QCOMPARE(codexDiagnosticArgs.at(0).toString(), QStringLiteral("Codex CLI"));
    QCOMPARE(codexDiagnosticArgs.at(1).toString(), QStringLiteral("not_logged_in"));
}

void SubscriptionToolsTest::browserSyncChromeEmptyCookieDiagnostics()
{
    QTemporaryDir tempHome;
    QVERIFY(tempHome.isValid());
    EnvVarGuard homeGuard("HOME");
    qputenv("HOME", tempHome.path().toUtf8());

    ClaudeCodeMonitor claude;
    claude.setEnabled(true);
    QSignalSpy claudeCompletedSpy(&claude, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy claudeDiagnosticSpy(&claude, &SubscriptionToolBackend::syncDiagnostic);

    claude.syncFromBrowser(QString(), 1);

    QCOMPARE(claudeCompletedSpy.count(), 1);
    QCOMPARE(claudeDiagnosticSpy.count(), 1);
    QCOMPARE(claude.syncStatus(), QStringLiteral("Not logged in"));

    const QList<QVariant> claudeCompletionArgs = claudeCompletedSpy.takeFirst();
    QCOMPARE(claudeCompletionArgs.at(0).toBool(), false);
    QVERIFY(claudeCompletionArgs.at(1).toString().contains(QStringLiteral("claude.ai"), Qt::CaseInsensitive));

    const QList<QVariant> claudeDiagnosticArgs = claudeDiagnosticSpy.takeFirst();
    QCOMPARE(claudeDiagnosticArgs.at(0).toString(), QStringLiteral("Claude Code"));
    QCOMPARE(claudeDiagnosticArgs.at(1).toString(), QStringLiteral("not_logged_in"));

    CodexCliMonitor codex;
    codex.setEnabled(true);
    QSignalSpy codexCompletedSpy(&codex, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy codexDiagnosticSpy(&codex, &SubscriptionToolBackend::syncDiagnostic);

    codex.syncFromBrowser(QString(), 1);

    QCOMPARE(codexCompletedSpy.count(), 1);
    QCOMPARE(codexDiagnosticSpy.count(), 1);
    QCOMPARE(codex.syncStatus(), QStringLiteral("Not logged in"));

    const QList<QVariant> codexCompletionArgs = codexCompletedSpy.takeFirst();
    QCOMPARE(codexCompletionArgs.at(0).toBool(), false);
    QVERIFY(codexCompletionArgs.at(1).toString().contains(QStringLiteral("chatgpt.com"), Qt::CaseInsensitive));

    const QList<QVariant> codexDiagnosticArgs = codexDiagnosticSpy.takeFirst();
    QCOMPARE(codexDiagnosticArgs.at(0).toString(), QStringLiteral("Codex CLI"));
    QCOMPARE(codexDiagnosticArgs.at(1).toString(), QStringLiteral("not_logged_in"));
}

void SubscriptionToolsTest::codexSyncWithoutLiveQuotaKeepsConfiguredPro()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    connect(&server, &QTcpServer::newConnection, this, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            socket->readAll();
            const QByteArray body = R"JSON({"accounts":{"default":{"entitlement":"plus"}}})JSON";
            socket->write("HTTP/1.1 200 OK\r\n");
            socket->write("Content-Type: application/json\r\n");
            socket->write("Content-Length: " + QByteArray::number(body.size()) + "\r\n");
            socket->write("Connection: close\r\n\r\n");
            socket->write(body);
            socket->disconnectFromHost();
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    });

    EnvVarGuard demoGuard("PLASMA_AI_MONITOR_DEMO");
    EnvVarGuard demoBaseGuard("PLASMA_AI_MONITOR_DEMO_BASE_URL");
    qputenv("PLASMA_AI_MONITOR_DEMO", QByteArray("1"));
    qputenv("PLASMA_AI_MONITOR_DEMO_BASE_URL", QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

    CodexCliMonitor codex;
    codex.setEnabled(true);
    codex.setPlanTier(QStringLiteral("pro"));

    QSignalSpy completedSpy(&codex, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy diagnosticSpy(&codex, &SubscriptionToolBackend::syncDiagnostic);

    codex.syncFromBrowser(QStringLiteral("session=test"), 0);

    QTRY_VERIFY_WITH_TIMEOUT(completedSpy.count() >= 1, 3000);
    QCOMPARE(codex.planTier(), QStringLiteral("pro"));
    QCOMPARE(codex.syncStatus(), QStringLiteral("Plan presets"));
    QVERIFY(diagnosticSpy.count() >= 1);
    QCOMPARE(diagnosticSpy.takeFirst().at(1).toString(), QStringLiteral("no_live_quota"));
    QVERIFY(completedSpy.takeFirst().at(0).toBool());
}

void SubscriptionToolsTest::codexLiveQuotaPayload()
{
    const QByteArray payload = R"JSON({
        "rate_limit": {
            "primary_window": {"used_percent": 25.0, "limit_window_seconds": 18000, "reset_at": 4102444800},
            "secondary_window": {"used_percent": 66.0, "limit_window_seconds": 604800, "reset_at": 4103049600}
        }
    })JSON";

    const QVariantList windows = CodexCliMonitor::quotaWindowsFromUsagePayload(payload);
    QCOMPARE(windows.size(), 2);
    const QVariantMap primary = windows.at(0).toMap();
    QCOMPARE(primary.value(QStringLiteral("kind")).toString(), QStringLiteral("rolling_5h"));
    QCOMPARE(primary.value(QStringLiteral("percentRemaining")).toDouble(), 75.0);
    const QVariantMap weekly = windows.at(1).toMap();
    QCOMPARE(weekly.value(QStringLiteral("kind")).toString(), QStringLiteral("rolling_weekly"));
    QCOMPARE(weekly.value(QStringLiteral("percentRemaining")).toDouble(), 34.0);
    QVERIFY(!weekly.value(QStringLiteral("resetAt")).toString().isEmpty());
}

void SubscriptionToolsTest::copilotBillingModeLabels()
{
    CopilotMonitor copilot;
    QCOMPARE(copilot.billingMode(), QStringLiteral("auto"));
    QCOMPARE(copilot.billingModeForDate(QStringLiteral("2026-05-31")), QStringLiteral("premium_requests_legacy"));
    QCOMPARE(copilot.billingModeForDate(QStringLiteral("2026-06-01")), QStringLiteral("ai_credits_usage_based"));
    QVERIFY(copilot.usageSourceLabel().contains(QStringLiteral("Auto")));

    copilot.setBillingMode(QStringLiteral("premium_requests"));
    QCOMPARE(copilot.billingMode(), QStringLiteral("premium_requests_legacy"));
    QVERIFY(copilot.usageSourceLabel().contains(QStringLiteral("Premium request")));

    copilot.setBillingMode(QStringLiteral("credits"));
    QCOMPARE(copilot.billingMode(), QStringLiteral("ai_credits_usage_based"));
    QVERIFY(copilot.usageSourceLabel().contains(QStringLiteral("AI credits")));
}

void SubscriptionToolsTest::copilotOrgMetricsRequireEnabledMonitor()
{
    EnvVarGuard demoGuard("PLASMA_AI_MONITOR_DEMO");
    EnvVarGuard baseUrlGuard("PLASMA_AI_MONITOR_DEMO_BASE_URL");
    qputenv("PLASMA_AI_MONITOR_DEMO", "1");

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    qputenv("PLASMA_AI_MONITOR_DEMO_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        while (server.hasPendingConnections()) {
            QTcpSocket *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
                const QByteArray body =
                    R"({"total_seats":12,"seat_breakdown":{"active_this_cycle":8}})";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(body.size())
                              + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    CopilotMonitor monitor;
    monitor.setGithubToken(QStringLiteral("saved-token"));
    monitor.setOrgName(QStringLiteral("example-org"));
    QVERIFY(!monitor.isEnabled());
    monitor.fetchOrgMetrics();
    QTest::qWait(100);
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!monitor.hasOrgMetrics());

    monitor.setEnabled(true);
    monitor.fetchOrgMetrics();
    QTRY_VERIFY_WITH_TIMEOUT(monitor.hasOrgMetrics(), 3000);
    QCOMPARE(monitor.orgTotalSeats(), 12);
    QCOMPARE(monitor.orgActiveUsers(), 8);
    QVERIFY(monitor.lastSyncTime().isValid());
    QVERIFY(!monitor.isSyncing());
}

void SubscriptionToolsTest::localWatchLifecycleAndReplacement()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = root.filePath(QStringLiteral("activity.db"));
    const auto write = [](const QString &path, const QByteArray &contents)
    {
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit();
    };
    QVERIFY(write(filePath, "baseline"));
    TestActivityMonitor monitor(root.path());
    monitor.setEnabled(true);
    auto *watcher = monitor.findChild<QFileSystemWatcher *>();
    QTRY_VERIFY_WITH_TIMEOUT(watcher->files().contains(filePath), 3000);
    QCOMPARE(monitor.usageCount(), 0);

    QVERIFY(write(filePath, "pending activity"));
    QTest::qWait(50);
    monitor.setEnabled(false);
    QTest::qWait(500);
    QCOMPARE(monitor.usageCount(), 0);
    QVERIFY(watcher->files().isEmpty());
    QVERIFY(watcher->directories().isEmpty());

    QVERIFY(write(filePath, "disabled activity"));
    monitor.setEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(watcher->files().contains(filePath), 3000);
    QCOMPARE(monitor.usageCount(), 0);
    const QString session = root.filePath(QStringLiteral("new-session"));
    QVERIFY(QDir().mkpath(session));
    const QString sessionFile = session + QStringLiteral("/activity.db");
    QVERIFY(write(sessionFile, "new session activity"));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.usageCount(), 1, 3000);
    QVERIFY(watcher->files().contains(sessionFile));

    QVERIFY(write(sessionFile, "atomically replaced session activity"));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.usageCount(), 2, 3000);
    QVERIFY(watcher->files().contains(sessionFile));
    QVERIFY(write(session + QStringLiteral("/ignored.log"), "ignore"));
    QTest::qWait(500);
    QCOMPARE(monitor.usageCount(), 2);

    for (int i = 0; i < 20; ++i)
        QVERIFY(write(sessionFile, QByteArray::number(i)));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.usageCount(), 3, 3000);
    QTest::qWait(500);
    QCOMPARE(monitor.usageCount(), 3);
}

void SubscriptionToolsTest::localWatchActivityDuringBaseline_data()
{
    QTest::addColumn<bool>("disableBeforeCompletion");
    QTest::newRow("activity during baseline") << false;
    QTest::newRow("disabled during baseline") << true;
}

void SubscriptionToolsTest::localWatchActivityDuringBaseline()
{
    QFETCH(bool, disableBeforeCompletion);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString session = root.filePath(QStringLiteral("existing-session"));
    QVERIFY(QDir().mkpath(session));
    const QString path = session + QStringLiteral("/activity.db");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("historical activity"), qint64(19));
    file.close();

    // Hold the worker before its first metadata read. A nested file has no
    // watcher yet, so this write must be found by the baseline itself.
    auto *pool = QThreadPool::globalInstance();
    pool->waitForDone();
    const int previousMaximum = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore entered;
    QSemaphore release;
    auto blocker = QtConcurrent::run([&]() { entered.release(); release.acquire(); });
    const auto cleanup = qScopeGuard([&]() {
        release.release();
        blocker.waitForFinished();
        pool->setMaxThreadCount(previousMaximum);
    });
    entered.acquire();
    TestActivityMonitor monitor(root.path());
    monitor.setEnabled(true);
    auto *watcher = monitor.findChild<QFileSystemWatcher *>();
    QVERIFY(!watcher->files().contains(path));
    QTest::qWait(5);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write("new activity"), qint64(12));
    file.close();
    if (disableBeforeCompletion)
        monitor.setEnabled(false);
    release.release();
    if (disableBeforeCompletion)
    {
        QTest::qWait(500);
        QCOMPARE(monitor.usageCount(), 0);
        QVERIFY(watcher->files().isEmpty());
        QVERIFY(watcher->directories().isEmpty());
        return;
    }
    QTRY_COMPARE_WITH_TIMEOUT(monitor.usageCount(), 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(watcher->files().contains(path), 3000);
    QTest::qWait(500);
    QCOMPARE(monitor.usageCount(), 1);
}

void SubscriptionToolsTest::localWatchBounds()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    for (int i = 0; i < 4010; ++i)
    {
        QFile file(root.filePath(QString::number(i)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("baseline");
    }
    TestActivityMonitor monitor(root.path());
    monitor.setEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(monitor.watchDiagnosticCode(), QStringLiteral("watch_incomplete"), 10000);
    auto *watcher = monitor.findChild<QFileSystemWatcher *>();
    QVERIFY(watcher->files().size() + watcher->directories().size() <= 4000);
    QCOMPARE(monitor.usageCount(), 0);
    monitor.setEnabled(false);
    QCOMPARE(monitor.watchDiagnosticCode(), QString());
}
QTEST_MAIN(SubscriptionToolsTest)
#include "test_subscription_tools.moc"
