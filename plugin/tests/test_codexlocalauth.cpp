#include "codexclimonitor.h"

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
}

class CodexLocalAuthTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localAuthIsInvokableAndReportsMissingLogin();
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

QTEST_MAIN(CodexLocalAuthTest)
#include "test_codexlocalauth.moc"
