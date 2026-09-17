#include "codexclimonitor.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
class HomeGuard
{
public:
    HomeGuard()
        : m_hadHome(qEnvironmentVariableIsSet("HOME"))
        , m_home(qgetenv("HOME"))
    {
    }

    ~HomeGuard()
    {
        if (m_hadHome) {
            qputenv("HOME", m_home);
        } else {
            qunsetenv("HOME");
        }
    }

private:
    bool m_hadHome;
    QByteArray m_home;
};

class TestCodexCliMonitor : public CodexCliMonitor
{
public:
    using CodexCliMonitor::recordSyncHttpFailure;
    using CodexCliMonitor::setSyncing;
};
}

class CodexLocalAuthTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localAuthIsInvokableAndReportsMissingLogin();
    void localAuthRecoversAfterCredentialsChange();
};

void CodexLocalAuthTest::localAuthIsInvokableAndReportsMissingLogin()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    HomeGuard homeGuard;
    qputenv("HOME", home.path().toUtf8());

    CodexCliMonitor monitor;
    QSignalSpy completionSpy(&monitor, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy diagnosticSpy(&monitor, &SubscriptionToolBackend::syncDiagnostic);

    QVERIFY(QMetaObject::invokeMethod(&monitor, "syncFromLocalAuth"));

    QCOMPARE(completionSpy.count(), 1);
    QCOMPARE(diagnosticSpy.count(), 1);
    QCOMPARE(monitor.syncStatus(), QStringLiteral("Not logged in"));
    QCOMPARE(diagnosticSpy.first().at(1).toString(), QStringLiteral("not_logged_in"));
    QVERIFY(completionSpy.first().at(1).toString().contains(QStringLiteral("codex login")));
    QVERIFY(!completionSpy.first().at(1).toString().contains(QStringLiteral("browser"), Qt::CaseInsensitive));
}

void CodexLocalAuthTest::localAuthRecoversAfterCredentialsChange()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    HomeGuard homeGuard;
    qputenv("HOME", home.path().toUtf8());

    QVERIFY(QDir().mkpath(home.filePath(QStringLiteral(".codex"))));
    const QString authPath = home.filePath(QStringLiteral(".codex/auth.json"));
    auto writeAuth = [&authPath](const QByteArray &contents) {
        QFile authFile(authPath);
        QVERIFY(authFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(authFile.write(contents), contents.size());
    };
    auto validAuth = [](const QByteArray &token) {
        return QByteArrayLiteral("{\"tokens\":{\"access_token\":\"") + token + QByteArrayLiteral("\"}}");
    };
    writeAuth(validAuth(QByteArrayLiteral("dummy-token-a")));

    TestCodexCliMonitor monitor;
    QVERIFY(monitor.canAutoSyncFromLocalAuth());

    monitor.setSyncing(true);
    writeAuth(validAuth(QByteArrayLiteral("dummy-token-b")));
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
    monitor.recordSyncHttpFailure(401, QByteArray());
    monitor.setSyncing(false);
    QVERIFY(monitor.canAutoSyncFromLocalAuth());

    monitor.recordSyncHttpFailure(401, QByteArray());
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
    writeAuth(validAuth(QByteArrayLiteral("dummy-token-c")));
    QVERIFY(monitor.canAutoSyncFromLocalAuth());

    monitor.recordSyncHttpFailure(403, QByteArray());
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
    QVERIFY(QFile::remove(authPath));
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
    writeAuth(QByteArrayLiteral("{\"tokens\":{}}"));
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
    writeAuth(QByteArrayLiteral("{\"tokens\":{\"access_token\":\"dummy-token-d\""));
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
    writeAuth(validAuth(QByteArrayLiteral("dummy-token-d")));
    QVERIFY(monitor.canAutoSyncFromLocalAuth());
}

QTEST_MAIN(CodexLocalAuthTest)
#include "test_codexlocalauth.moc"
