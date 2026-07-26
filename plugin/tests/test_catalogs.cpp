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
    void pricingSchemaV5EstimatesWithoutFalsePrecision();
    void subscriptionCatalogLoads();
    void staleCatalogDetection();
    void invalidCatalogExposesStatus();
};

void CatalogsTest::pricingSchemaV5EstimatesWithoutFalsePrecision()
{
    ProviderPricingCatalog catalog;
    QVariantMap usage{{QStringLiteral("inputTokens"), 250000},
                      {QStringLiteral("cachedInputTokens"), 100000},
                      {QStringLiteral("outputTokens"), 10000},
                      {QStringLiteral("modality"), QStringLiteral("text")},
                      {QStringLiteral("serviceTier"), QStringLiteral("standard")}};
    QVariantMap estimate = catalog.estimateCost(QStringLiteral("google"), QStringLiteral("gemini-2.5-pro"), usage);
    QVERIFY(estimate.value(QStringLiteral("complete")).toBool());
    QVERIFY(qAbs(estimate.value(QStringLiteral("amount")).toDouble() - 0.55) < 0.000001);
    QCOMPARE(estimate.value(QStringLiteral("currency")).toString(), QStringLiteral("USD"));

    usage.insert(QStringLiteral("serviceTier"), QStringLiteral("batch"));
    estimate = catalog.estimateCost(QStringLiteral("google"), QStringLiteral("gemini-2.5-pro"), usage);
    QVERIFY(estimate.value(QStringLiteral("complete")).toBool());
    QVERIFY(qAbs(estimate.value(QStringLiteral("amount")).toDouble() - 0.275) < 0.000001);

    usage.insert(QStringLiteral("serviceTier"), QStringLiteral("standard"));
    usage.insert(QStringLiteral("additiveUsage"), QVariantMap{{QStringLiteral("google_search_grounding"), 2}});
    estimate = catalog.estimateCost(QStringLiteral("google"), QStringLiteral("gemini-2.5-pro"), usage);
    QVERIFY(!estimate.value(QStringLiteral("complete")).toBool());
    QVERIFY(estimate.value(QStringLiteral("missingDimensions")).toStringList()
                .contains(QStringLiteral("allowanceConsumed:google_search_grounding")));

    usage.insert(QStringLiteral("allowanceConsumed"), QVariantMap{{QStringLiteral("google_search_grounding"), 1500}});
    estimate = catalog.estimateCost(QStringLiteral("google"), QStringLiteral("gemini-2.5-pro"), usage);
    QVERIFY(estimate.value(QStringLiteral("complete")).toBool());
    QVERIFY(qAbs(estimate.value(QStringLiteral("amount")).toDouble() - 0.62) < 0.000001);

    const QVariantMap incomplete = catalog.estimateCost(
        QStringLiteral("google"), QStringLiteral("gemini-3.1-flash-lite"),
        QVariantMap{{QStringLiteral("inputTokens"), 1000},
                    {QStringLiteral("cachedInputTokens"), 500},
                    {QStringLiteral("outputTokens"), 100}});
    QVERIFY(!incomplete.value(QStringLiteral("complete")).toBool());
    QVERIFY(incomplete.value(QStringLiteral("missingDimensions")).toStringList()
                .contains(QStringLiteral("cachedInputRate")));
}

void CatalogsTest::providerCatalogLoads()
{
    ProviderPricingCatalog catalog;

    QVERIFY(catalog.isValid());
    QCOMPARE(catalog.schemaVersion(), 5);
    QCOMPARE(catalog.catalogVersion(), QStringLiteral("2026.07.13"));
    QCOMPARE(catalog.runtimeScraping(), false);
    QVERIFY(catalog.manualReviewCount() > 0);
    QVERIFY(!catalog.reviewItems().isEmpty());
    QVERIFY(catalog.reviewItems().first().toMap().contains(QStringLiteral("reviewReason")));

    const QVariantList openAiModels = catalog.tokenModelsForProvider(QStringLiteral("openai"));
    QVERIFY(openAiModels.size() >= 7);
    const QVariantMap gpt56Pricing = catalog.pricing(QStringLiteral("openai"), QStringLiteral("gpt-5.6-terra"));
    QCOMPARE(gpt56Pricing.value(QStringLiteral("unit")).toString(), QStringLiteral("1M_tokens"));
    QCOMPARE(gpt56Pricing.value(QStringLiteral("precision")).toString(), QStringLiteral("official_exact"));
    QCOMPARE(gpt56Pricing.value(QStringLiteral("input")).toDouble(), 2.5);
    QCOMPARE(gpt56Pricing.value(QStringLiteral("output")).toDouble(), 15.0);
    QVERIFY(!catalog.pricing(QStringLiteral("deepseek"), QStringLiteral("deepseek-v4-flash")).isEmpty());
    QCOMPARE(catalog.effectiveModelIdAt(QStringLiteral("deepseek"), QStringLiteral("deepseek-chat"),
                                        QDate(2026, 7, 23)), QStringLiteral("deepseek-chat"));
    QCOMPARE(catalog.effectiveModelIdAt(QStringLiteral("deepseek"), QStringLiteral("deepseek-chat"),
                                        QDate(2026, 7, 24)), QStringLiteral("deepseek-v4-flash"));
    const QVariantMap openAi = catalog.provider(QStringLiteral("openai"));
    QCOMPARE(openAi.value(QStringLiteral("stableId")).toString(), QStringLiteral("openai"));
    QVERIFY(openAi.value(QStringLiteral("capabilities")).toList().contains(QStringLiteral("cost")));

    const QVariantMap anthropic = catalog.provider(QStringLiteral("anthropic"));
    const QVariantMap anthropicAuth = anthropic.value(QStringLiteral("auth")).toMap();
    QCOMPARE(anthropic.value(QStringLiteral("monitoringLevel")).toString(),
             QStringLiteral("actual_usage_spend"));
    QCOMPARE(anthropicAuth.value(QStringLiteral("credentialSlots")).toStringList(),
             QStringList({QStringLiteral("anthropic"), QStringLiteral("anthropic_admin")}));
    QCOMPARE(anthropicAuth.value(QStringLiteral("acceptAnyCredentialSet")).toList().size(), 2);
    const QVariantMap capabilityCredentials =
        anthropicAuth.value(QStringLiteral("capabilityCredentialSets")).toMap();
    QVERIFY(capabilityCredentials.contains(QStringLiteral("connectivity")));
    QVERIFY(capabilityCredentials.contains(QStringLiteral("usage")));
    QVERIFY(capabilityCredentials.contains(QStringLiteral("cost")));
    QCOMPARE(anthropic.value(QStringLiteral("safeRefresh")).toMap()
                 .value(QStringLiteral("minimumIntervalSeconds")).toInt(), 300);

    QVERIFY(qAbs(catalog.amountForModelUnit(QStringLiteral("googleveo"), QStringLiteral("veo-3.1-generate-preview"), QStringLiteral("video_second")) - 0.4) < 0.000001);
}

void CatalogsTest::subscriptionCatalogLoads()
{
    SubscriptionPlanCatalog catalog;

    QVERIFY(catalog.isValid());
    QCOMPARE(catalog.schemaVersion(), 1);
    QCOMPARE(catalog.catalogVersion(), QStringLiteral("2026.07.18"));
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
    QCOMPARE(catalog.planLabelForId(QStringLiteral("google-antigravity"), QStringLiteral("ultra_20x")),
             QStringLiteral("Google AI Ultra 20x"));
    QVERIFY(catalog.quotaWindows(QStringLiteral("google-antigravity"), QStringLiteral("pro")).isEmpty());
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

    QFile file(dir.filePath(QStringLiteral("providers-v4.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"JSON({
        "schemaVersion": 5,
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
