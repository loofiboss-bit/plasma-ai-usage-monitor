#include "codexclimonitor.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
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
    using CodexCliMonitor::handleCodexUsageReplyFailure;
    using CodexCliMonitor::recordSyncHttpFailure;
    using CodexCliMonitor::setSyncing;
};

class RejectedReply final : public QNetworkReply
{
public:
    explicit RejectedReply(int status)
    {
        setOpenMode(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        setError(QNetworkReply::AuthenticationRequiredError, QStringLiteral("Authentication rejected"));
        setFinished(true);
    }

    void abort() override {}
    qint64 bytesAvailable() const override { return 0; }

protected:
    qint64 readData(char *, qint64) override { return -1; }
};
}

class CodexLocalAuthTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localAuthIsInvokableAndReportsMissingLogin();
    void localAuthRecoversAfterCredentialsChange();
    void localAuthHttpRejectionReportsCodexLoginInsteadOfGenericSyncFailure_data();
    void localAuthHttpRejectionReportsCodexLoginInsteadOfGenericSyncFailure();
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

void CodexLocalAuthTest::localAuthHttpRejectionReportsCodexLoginInsteadOfGenericSyncFailure_data()
{
    QTest::addColumn<int>("status");

    QTest::newRow("http-401") << 401;
    QTest::newRow("http-403") << 403;
}

void CodexLocalAuthTest::localAuthHttpRejectionReportsCodexLoginInsteadOfGenericSyncFailure()
{
    QFETCH(int, status);

    QTemporaryDir home;
    QVERIFY(home.isValid());
    HomeGuard homeGuard;
    qputenv("HOME", home.path().toUtf8());

    QVERIFY(QDir().mkpath(home.filePath(QStringLiteral(".codex"))));
    QFile authFile(home.filePath(QStringLiteral(".codex/auth.json")));
    QVERIFY(authFile.open(QIODevice::WriteOnly));
    const QByteArray auth = QByteArrayLiteral("{\"tokens\":{\"access_token\":\"dummy-token\"}}");
    QCOMPARE(authFile.write(auth), auth.size());
    authFile.close();

    TestCodexCliMonitor monitor;
    QVERIFY(monitor.canAutoSyncFromLocalAuth());
    QSignalSpy completionSpy(&monitor, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy diagnosticSpy(&monitor, &SubscriptionToolBackend::syncDiagnostic);

    monitor.setSyncing(true);
    RejectedReply reply(status);
    monitor.handleCodexUsageReplyFailure(&reply, QString());

    const QString completionMessage = completionSpy.count() == 1 ? completionSpy.first().at(1).toString() : QString();
    const QString observed = QStringLiteral("Expected a not_logged_in Codex CLI diagnostic containing 'codex login'; "
                                            "observed status='%1', diagnostics=%2, completions=%3, completion message='%4'")
                                 .arg(monitor.syncStatus())
                                 .arg(diagnosticSpy.count())
                                 .arg(completionSpy.count())
                                 .arg(completionMessage);
    const bool hasActionableDiagnostic = diagnosticSpy.count() == 1
        && diagnosticSpy.first().at(0).toString() == QStringLiteral("Codex CLI")
        && diagnosticSpy.first().at(1).toString() == QStringLiteral("not_logged_in")
        && diagnosticSpy.first().at(2).toString().contains(QStringLiteral("codex login"));
    QVERIFY2(monitor.syncStatus() == QStringLiteral("Run codex login") && hasActionableDiagnostic, qPrintable(observed));

    QCOMPARE(diagnosticSpy.first().at(0).toString(), QStringLiteral("Codex CLI"));
    QCOMPARE(diagnosticSpy.first().at(1).toString(), QStringLiteral("not_logged_in"));
    const QString actionableMessage = diagnosticSpy.first().at(2).toString();
    QVERIFY(actionableMessage.contains(QStringLiteral("codex login")));
    QCOMPARE(completionSpy.count(), 1);
    QCOMPARE(completionSpy.first().at(0).toBool(), false);
    QCOMPARE(completionSpy.first().at(1).toString(), actionableMessage);
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
}

QTEST_MAIN(CodexLocalAuthTest)
#include "test_codexlocalauth.moc"
