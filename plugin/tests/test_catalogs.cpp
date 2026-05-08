#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "providerpricingcatalog.h"
#include "subscriptionplancatalog.h"

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

class CatalogsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void providerCatalogLoads();
    void subscriptionCatalogLoads();
    void staleCatalogDetection();
    void invalidCatalogExposesStatus();
};

void CatalogsTest::providerCatalogLoads()
{
    ProviderPricingCatalog catalog;

    QVERIFY(catalog.isValid());
    QCOMPARE(catalog.schemaVersion(), 3);
    QCOMPARE(catalog.catalogVersion(), QStringLiteral("2026.05.08"));
    QCOMPARE(catalog.runtimeScraping(), false);
    QVERIFY(catalog.manualReviewCount() > 0);
    QVERIFY(!catalog.reviewItems().isEmpty());
    QVERIFY(catalog.reviewItems().first().toMap().contains(QStringLiteral("reviewReason")));

    const QVariantList openAiModels = catalog.tokenModelsForProvider(QStringLiteral("openai"));
    QVERIFY(openAiModels.size() >= 3);
    const QVariantMap gpt54Pricing = catalog.pricing(QStringLiteral("openai"), QStringLiteral("gpt-5.4"));
    QCOMPARE(gpt54Pricing.value(QStringLiteral("unit")).toString(), QStringLiteral("1M_tokens"));
    QCOMPARE(gpt54Pricing.value(QStringLiteral("precision")).toString(), QStringLiteral("official_exact"));

    QVERIFY(qAbs(catalog.amountForModelUnit(QStringLiteral("googleveo"), QStringLiteral("veo-2"), QStringLiteral("video_second")) - 0.35) < 0.000001);
}

void CatalogsTest::subscriptionCatalogLoads()
{
    SubscriptionPlanCatalog catalog;

    QVERIFY(catalog.isValid());
    QCOMPARE(catalog.schemaVersion(), 1);
    QCOMPARE(catalog.catalogVersion(), QStringLiteral("2026.05.08"));
    QCOMPARE(catalog.runtimeScraping(), false);
    QVERIFY(catalog.manualReviewCount() > 0);
    QVERIFY(catalog.sourceConflictCount() > 0);
    QVERIFY(!catalog.reviewItems().isEmpty());
    QCOMPARE(catalog.reviewItems().first().toMap().value(QStringLiteral("key")).toString(), QStringLiteral("windsurf"));
    QVERIFY(catalog.reviewItems().first().toMap().value(QStringLiteral("sourceConflictReason")).toString().contains(QStringLiteral("Windsurf")));

    QCOMPARE(catalog.planIdForLabel(QStringLiteral("claude-code"), QStringLiteral("Max 20x")), QStringLiteral("max_20x"));
    QCOMPARE(catalog.planIdForLabel(QStringLiteral("codex-cli"), QStringLiteral("Pro $100")), QStringLiteral("pro_100"));
    QCOMPARE(catalog.planLabelForId(QStringLiteral("github-copilot"), QStringLiteral("pro_plus")), QStringLiteral("Pro+"));
    QCOMPARE(catalog.planLabelForId(QStringLiteral("cursor"), QStringLiteral("pro_plus")), QStringLiteral("Pro+"));
    QVERIFY(!catalog.quotaWindows(QStringLiteral("claude-code"), QStringLiteral("pro")).isEmpty());

    const QVariantList copilotProPlusRows = catalog.quotaWindows(QStringLiteral("github-copilot"), QStringLiteral("pro_plus"));
    QVERIFY(!copilotProPlusRows.isEmpty());
    QCOMPARE(copilotProPlusRows.first().toMap().value(QStringLiteral("limit")).toInt(), 1500);

    const QVariantList creditsRows = catalog.billingModeQuotaWindows(QStringLiteral("github-copilot"), QStringLiteral("ai_credits_usage_based"));
    QVERIFY(!creditsRows.isEmpty());
    QCOMPARE(creditsRows.first().toMap().value(QStringLiteral("creditUsdValue")).toDouble(), 0.01);
}

void CatalogsTest::staleCatalogDetection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    EnvVarGuard guard("AIUSAGE_MONITOR_CATALOG_DIR");
    qputenv("AIUSAGE_MONITOR_CATALOG_DIR", dir.path().toUtf8());

    QFile file(dir.filePath(QStringLiteral("providers-v3.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"JSON({
        "schemaVersion": 3,
        "catalogVersion": "2026.01.01",
        "release": "8.0.0",
        "lastReviewed": "2026-01-01",
        "runtimeScraping": false,
        "providers": []
    })JSON");
    file.close();

    ProviderPricingCatalog catalog;
    QVERIFY(catalog.isValid());
    QVERIFY(catalog.isStale(1));
}

void CatalogsTest::invalidCatalogExposesStatus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    EnvVarGuard guard("AIUSAGE_MONITOR_CATALOG_DIR");
    qputenv("AIUSAGE_MONITOR_CATALOG_DIR", dir.path().toUtf8());

    QFile file(dir.filePath(QStringLiteral("subscriptions-v1.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"JSON({
        "schemaVersion": 99,
        "catalogVersion": "bad",
        "lastReviewed": "not-a-date",
        "runtimeScraping": true,
        "tools": []
    })JSON");
    file.close();

    SubscriptionPlanCatalog catalog;
    QVERIFY(!catalog.isValid());
    QVERIFY(catalog.diagnostics().size() >= 3);
}

QTEST_MAIN(CatalogsTest)
#include "test_catalogs.moc"
