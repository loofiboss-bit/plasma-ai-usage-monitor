#include <QtTest>

#include "providermanager.h"

class ProviderManagerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void loadsCatalogAndCreatesDescriptorAdapters();
    void replacesBackendByStableId();
};

void ProviderManagerTest::loadsCatalogAndCreatesDescriptorAdapters()
{
    ProviderManager manager;
    QCOMPARE(manager.rowCount(), 18);
    for (int row = 0; row < manager.rowCount(); ++row) {
        const QModelIndex index = manager.index(row);
        const QString id = manager.data(index, ProviderManager::StableIdRole).toString();
        QVERIFY(!id.isEmpty());
        QVERIFY2(manager.backend(id), qPrintable(id));
        QCOMPARE(manager.descriptor(id).value(QStringLiteral("stableId")).toString(), id);
    }
}

void ProviderManagerTest::replacesBackendByStableId()
{
    ProviderManager manager;
    QObject backend;
    manager.registerBackend(QStringLiteral("openai"), &backend);
    QCOMPARE(manager.backend(QStringLiteral("openai")), &backend);
}

QTEST_MAIN(ProviderManagerTest)
#include "test_providermanager.moc"
