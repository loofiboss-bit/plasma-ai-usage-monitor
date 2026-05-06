#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

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
    void copilotBillingModeLabels();
};

void SubscriptionToolsTest::planDefaults()
{
    ClaudeCodeMonitor claude;
    QCOMPARE(claude.defaultLimitForPlan(QStringLiteral("Pro")), 0);
    QCOMPARE(claude.defaultSecondaryLimitForPlan(QStringLiteral("Max 5x")), 0);
    QCOMPARE(claude.defaultCostForPlan(QStringLiteral("Max 20x")), 200.0);
    claude.setPlanTier(QStringLiteral("max_20x"));
    QVERIFY(!claude.quotaWindows().isEmpty());

    CodexCliMonitor codex;
    QCOMPARE(codex.defaultLimitForPlan(QStringLiteral("Plus")), 0);
    QCOMPARE(codex.defaultSecondaryLimitForPlan(QStringLiteral("Pro")), 0);
    QCOMPARE(codex.defaultCostForPlan(QStringLiteral("Pro $100")), 100.0);
    QCOMPARE(codex.defaultCostForPlan(QStringLiteral("Pro")), 200.0);
    codex.setPlanTier(QStringLiteral("pro"));
    QVERIFY(!codex.quotaWindows().isEmpty());

    CopilotMonitor copilot;
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Free")), 50);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Pro")), 300);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Pro+")), 1500);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Business")), 300);
    QCOMPARE(copilot.defaultLimitForPlan(QStringLiteral("Enterprise")), 1000);
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
    const QString stateFilePath = stateDir + QStringLiteral("/state.json");

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

    // Baseline only — first pass should not increment usage.
    copilot.detectActivity();
    QCOMPARE(copilot.usageCount(), 0);

    QTest::qWait(2100);
    QVERIFY(stateFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    stateFile.write("{\"status\":\"active\"}\n");
    stateFile.close();

    copilot.detectActivity();

    // Wait for debounce timer (250ms + buffer)
    QTest::qWait(500);

    QCOMPARE(copilot.usageCount(), 1);
    QCOMPARE(activitySpy.count(), 1);
    QVERIFY(usageSpy.count() >= 1);
}

void SubscriptionToolsTest::browserSyncEmptyCookieDiagnostics()
{
    ClaudeCodeMonitor claude;
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
    ClaudeCodeMonitor claude;
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
    codex.setPlanTier(QStringLiteral("pro"));

    QSignalSpy completedSpy(&codex, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy diagnosticSpy(&codex, &SubscriptionToolBackend::syncDiagnostic);

    codex.syncFromBrowser(QStringLiteral("session=test"), 0);

    QTRY_VERIFY_WITH_TIMEOUT(completedSpy.count() >= 1, 3000);
    QCOMPARE(codex.planTier(), QStringLiteral("pro"));
    QCOMPARE(codex.syncStatus(), QStringLiteral("Catalog fallback"));
    QVERIFY(diagnosticSpy.count() >= 1);
    QCOMPARE(diagnosticSpy.takeFirst().at(1).toString(), QStringLiteral("no_live_quota"));
    QVERIFY(completedSpy.takeFirst().at(0).toBool());
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

QTEST_MAIN(SubscriptionToolsTest)
#include "test_subscription_tools.moc"
