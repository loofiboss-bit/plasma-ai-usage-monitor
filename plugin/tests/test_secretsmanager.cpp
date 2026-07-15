#include "secretsmanager.h"

#include <QSignalSpy>
#include <QtTest>

namespace {
class EnvironmentGuard
{
public:
    explicit EnvironmentGuard(const char *name)
        : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name)), m_value(qgetenv(name))
    {
    }

    ~EnvironmentGuard()
    {
        if (m_wasSet) qputenv(m_name.constData(), m_value);
        else qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    bool m_wasSet;
    QByteArray m_value;
};
}

class SecretsManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void demoModeNeverOpensReadsOrWritesWallet();
};

void SecretsManagerTest::demoModeNeverOpensReadsOrWritesWallet()
{
    EnvironmentGuard demoGuard("PLASMA_AI_MONITOR_DEMO");
    qputenv("PLASMA_AI_MONITOR_DEMO", QByteArrayLiteral("1"));

    SecretsManager manager;
    QSignalSpy storedSpy(&manager, &SecretsManager::keyStored);
    QSignalSpy removedSpy(&manager, &SecretsManager::keyRemoved);

    QVERIFY(manager.isDemoIsolated());
    QVERIFY(!manager.isWalletOpen());
    QCOMPARE(manager.secretReadCount(), 0);
    QVERIFY(!manager.hasKey(QStringLiteral("openai")));
    QVERIFY(manager.getKey(QStringLiteral("openai")).isEmpty());

    manager.storeKey(QStringLiteral("openai"), QStringLiteral("must-not-be-written"));
    manager.removeKey(QStringLiteral("openai"));
    manager.retryOpenWallet();

    QCOMPARE(manager.secretReadCount(), 0);
    QVERIFY(!manager.isWalletOpen());
    QCOMPARE(storedSpy.count(), 0);
    QCOMPARE(removedSpy.count(), 0);
}

QTEST_GUILESS_MAIN(SecretsManagerTest)
#include "test_secretsmanager.moc"
