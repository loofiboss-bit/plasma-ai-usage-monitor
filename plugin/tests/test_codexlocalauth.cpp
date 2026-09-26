#include "codexclimonitor.h"

#include <QDir>
#include <QFile>
#include <QNetworkRequest>
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

class RejectedReply final : public QNetworkReply
{
public:
    explicit RejectedReply(int status, QObject *parent = nullptr)
        : QNetworkReply(parent)
    {
        setOpenMode(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        setError(QNetworkReply::AuthenticationRequiredError, QStringLiteral("Authentication rejected"));
    }

    void abort() override {}
    qint64 bytesAvailable() const override { return 0; }
    void finish()
    {
        QMetaObject::invokeMethod(this, [this]() {
            setFinished(true);
            Q_EMIT finished();
        }, Qt::QueuedConnection);
    }

protected:
    qint64 readData(char *, qint64) override { return -1; }
};

class TestCodexCliMonitor : public CodexCliMonitor
{
public:
    using CodexCliMonitor::recordSyncHttpFailure;
    using CodexCliMonitor::setSyncing;

    void setRejectedStatus(int status) { m_rejectedStatus = status; }
    RejectedReply *pendingReply() const { return m_pendingReply; }
    int browserFallbackCount() const { return m_browserFallbackCount; }
    QString browserFallbackCookie() const { return m_browserFallbackCookie; }

protected:
    QNetworkReply *requestCodexUsage(const QNetworkRequest &) override
    {
        m_pendingReply = new RejectedReply(m_rejectedStatus, this);
        return m_pendingReply;
    }

    void fetchAccountCheck(const QString &cookieHeader) override
    {
        ++m_browserFallbackCount;
        m_browserFallbackCookie = cookieHeader;
    }

private:
    int m_rejectedStatus = 401;
    RejectedReply *m_pendingReply = nullptr;
    int m_browserFallbackCount = 0;
    QString m_browserFallbackCookie;
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
    void explicitBrowserHttpRejectionFallsBackWithOriginalCookie_data();
    void explicitBrowserHttpRejectionFallsBackWithOriginalCookie();
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
    monitor.setRejectedStatus(status);
    QVERIFY(monitor.canAutoSyncFromLocalAuth());
    QSignalSpy completionSpy(&monitor, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy diagnosticSpy(&monitor, &SubscriptionToolBackend::syncDiagnostic);

    monitor.syncFromLocalAuth();
    RejectedReply *reply = monitor.pendingReply();
    QVERIFY(reply);
    QVERIFY(!reply->isFinished());
    QCOMPARE(completionSpy.count(), 0);
    QCOMPARE(diagnosticSpy.count(), 0);
    reply->finish();
    QTRY_COMPARE(completionSpy.count(), 1);

    const QString expectedMessage = QStringLiteral("Not logged in - run codex login to enable local Codex quota sync");
    const QString completionMessage = completionSpy.first().at(1).toString();
    const QString observed = QStringLiteral("Expected a not_logged_in Codex CLI diagnostic with the local login action; "
                                            "observed status='%1', diagnostics=%2, completions=%3, completion message='%4'")
                                 .arg(monitor.syncStatus())
                                 .arg(diagnosticSpy.count())
                                 .arg(completionSpy.count())
                                 .arg(completionMessage);
    const bool hasActionableDiagnostic = diagnosticSpy.count() == 1
        && diagnosticSpy.first().at(0).toString() == QStringLiteral("Codex CLI")
        && diagnosticSpy.first().at(1).toString() == QStringLiteral("not_logged_in")
        && diagnosticSpy.first().at(2).toString() == expectedMessage;
    QVERIFY2(monitor.syncStatus() == QStringLiteral("Run codex login") && hasActionableDiagnostic, qPrintable(observed));

    QCOMPARE(monitor.syncStatus(), QStringLiteral("Run codex login"));
    QCOMPARE(diagnosticSpy.first().at(0).toString(), QStringLiteral("Codex CLI"));
    QCOMPARE(diagnosticSpy.first().at(1).toString(), QStringLiteral("not_logged_in"));
    const QString actionableMessage = diagnosticSpy.first().at(2).toString();
    QCOMPARE(actionableMessage, expectedMessage);
    QCOMPARE(completionSpy.count(), 1);
    QCOMPARE(completionSpy.first().at(0).toBool(), false);
    QCOMPARE(completionSpy.first().at(1).toString(), actionableMessage);
    QVERIFY(!monitor.canAutoSyncFromLocalAuth());
}

void CodexLocalAuthTest::explicitBrowserHttpRejectionFallsBackWithOriginalCookie_data()
{
    QTest::addColumn<int>("status");

    QTest::newRow("http-401") << 401;
    QTest::newRow("http-403") << 403;
}

void CodexLocalAuthTest::explicitBrowserHttpRejectionFallsBackWithOriginalCookie()
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
    monitor.setRejectedStatus(status);
    QSignalSpy completionSpy(&monitor, &SubscriptionToolBackend::syncCompleted);
    QSignalSpy diagnosticSpy(&monitor, &SubscriptionToolBackend::syncDiagnostic);
    const QString cookie = QStringLiteral("session=original-browser-cookie");

    monitor.syncFromBrowser(cookie, 0);
    RejectedReply *reply = monitor.pendingReply();
    QVERIFY(reply);
    QVERIFY(!reply->isFinished());
    QCOMPARE(monitor.browserFallbackCount(), 0);
    reply->finish();
    QTRY_COMPARE(monitor.browserFallbackCount(), 1);

    QCOMPARE(monitor.browserFallbackCookie(), cookie);
    QCOMPARE(diagnosticSpy.count(), 0);
    QCOMPARE(completionSpy.count(), 0);
}

QTEST_MAIN(CodexLocalAuthTest)
#include "test_codexlocalauth.moc"
