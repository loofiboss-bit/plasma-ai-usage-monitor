#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

#include <unistd.h>

#include "antigravity_local.pb.h"
#include "antigravitymonitor.h"

class TestableAntigravityMonitor : public AntigravityMonitor
{
  public:
    void seedSnapshotAndFail()
    {
        setSyncedQuotaWindows({QVariantMap{{QStringLiteral("label"), QStringLiteral("Gemini fixture")},
                                           {QStringLiteral("percentUsed"), 20.0}}});
        setLastSyncTime(QDateTime::currentDateTimeUtc());
        finishFailure(QStringLiteral("timeout"), QStringLiteral("fixture timeout"));
    }
};

class AntigravityMonitorTest : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void grpcFrames();
    void planNormalization_data();
    void planNormalization();
    void payloadPlanLabels_data();
    void payloadPlanLabels();
    void payloadModelsAndQuota();
    void connectPayloadModelsAndQuota();
    void quotaSummaryPayload();
    void discoverySecurity();
    void staleSnapshotIsPreserved();
    void liveSync();
};

void AntigravityMonitorTest::grpcFrames()
{
    const QByteArray payload("quota-payload");
    QString error;
    QCOMPARE(AntigravityMonitor::firstGrpcMessage(AntigravityMonitor::grpcFrame(payload), &error), payload);
    QVERIFY(error.isEmpty());

    QVERIFY(AntigravityMonitor::firstGrpcMessage(QByteArray("bad"), &error).isEmpty());
    QCOMPARE(error, QStringLiteral("missing_frame"));

    QByteArray compressed = AntigravityMonitor::grpcFrame(payload);
    compressed[0] = '\x01';
    QVERIFY(AntigravityMonitor::firstGrpcMessage(compressed, &error).isEmpty());
    QCOMPARE(error, QStringLiteral("compressed_frame"));

    QByteArray broken(5, '\0');
    broken[4] = '\x20';
    QVERIFY(AntigravityMonitor::firstGrpcMessage(broken, &error).isEmpty());
    QCOMPARE(error, QStringLiteral("invalid_frame_size"));
}

void AntigravityMonitorTest::planNormalization_data()
{
    QTest::addColumn<QString>("plan");
    QTest::addColumn<QString>("tier");
    QTest::addColumn<bool>("enterprise");
    QTest::addColumn<QString>("expected");
    QTest::newRow("standard") << "Standard" << "g1-standard-tier" << false << "standard";
    QTest::newRow("pro") << "Pro" << "g1-pro-tier" << false << "pro";
    QTest::newRow("generic ultra") << "Ultra" << "g1-ultra-tier" << false << "ultra";
    QTest::newRow("ultra 5x") << "Ultra 5x" << "" << false << "ultra_5x";
    QTest::newRow("ultra 20x") << "" << "ultra-20x" << false << "ultra_20x";
    QTest::newRow("enterprise") << "Custom" << "" << true << "enterprise";
    QTest::newRow("unknown") << "Preview" << "g1-future-tier" << false << "unknown";
}

void AntigravityMonitorTest::planNormalization()
{
    QFETCH(QString, plan);
    QFETCH(QString, tier);
    QFETCH(bool, enterprise);
    QFETCH(QString, expected);
    QCOMPARE(AntigravityMonitor::normalizedPlanId(plan, tier, enterprise), expected);
}

void AntigravityMonitorTest::payloadPlanLabels_data()
{
    QTest::addColumn<QString>("plan");
    QTest::addColumn<QString>("tierId");
    QTest::addColumn<QString>("tierName");
    QTest::addColumn<bool>("enterprise");
    QTest::addColumn<QString>("expectedId");
    QTest::addColumn<QString>("expectedLabel");
    QTest::newRow("standard") << "Standard" << "g1-standard-tier" << "Standard" << false << "standard"
                              << "Google AI Standard";
    QTest::newRow("pro") << "Pro" << "g1-pro-tier" << "Pro" << false << "pro" << "Google AI Pro";
    QTest::newRow("ultra") << "Ultra" << "g1-ultra-tier" << "Ultra" << false << "ultra" << "Google AI Ultra";
    QTest::newRow("ultra-5x") << "Ultra" << "g1-ultra-5x-tier" << "Google AI Ultra" << false << "ultra_5x"
                              << "Google AI Ultra 5x";
    QTest::newRow("ultra-20x") << "Ultra" << "g1-ultra-20x-tier" << "Google AI Ultra" << false << "ultra_20x"
                               << "Google AI Ultra 20x";
    QTest::newRow("enterprise") << "Enterprise" << "enterprise" << "Enterprise" << true << "enterprise"
                                << "Enterprise";
    QTest::newRow("unknown") << "Preview" << "g1-future-tier" << "Future plan" << false << "unknown"
                             << "Future plan";
}

void AntigravityMonitorTest::payloadPlanLabels()
{
    QFETCH(QString, plan);
    QFETCH(QString, tierId);
    QFETCH(QString, tierName);
    QFETCH(bool, enterprise);
    QFETCH(QString, expectedId);
    QFETCH(QString, expectedLabel);

    exa::language_server_pb::GetUserStatusResponse response;
    auto *status = response.mutable_user_status();
    status->mutable_plan_status()->mutable_plan_info()->set_plan_name(plan.toStdString());
    status->mutable_plan_status()->mutable_plan_info()->set_is_enterprise(enterprise);
    status->mutable_user_tier()->set_id(tierId.toStdString());
    status->mutable_user_tier()->set_name(tierName.toStdString());
    status->mutable_cascade_model_config_data()->add_client_model_configs()->set_label("Gemini fixture");
    std::string serialized;
    QVERIFY(response.SerializeToString(&serialized));
    const QVariantMap parsed = AntigravityMonitor::parseUserStatusPayload(
        QByteArray(serialized.data(), static_cast<qsizetype>(serialized.size())));
    QCOMPARE(parsed.value(QStringLiteral("planId")).toString(), expectedId);
    QCOMPARE(parsed.value(QStringLiteral("planLabel")).toString(), expectedLabel);
}

void AntigravityMonitorTest::payloadModelsAndQuota()
{
    exa::language_server_pb::GetUserStatusResponse response;
    auto *status = response.mutable_user_status();
    status->mutable_plan_status()->mutable_plan_info()->set_plan_name("Pro");
    status->mutable_user_tier()->set_id("g1-pro-tier");
    status->mutable_user_tier()->set_name("Google AI Pro");
    auto *models = status->mutable_cascade_model_config_data();

    auto *high = models->add_client_model_configs();
    high->set_label("Gemini 3 Pro (High)");
    high->set_provider(4);
    high->mutable_model_or_alias()->set_model(1132);
    high->mutable_quota_info()->set_remaining_fraction(0.25f);
    high->mutable_quota_info()->mutable_reset_time()->set_seconds(4102444800);

    auto *low = models->add_client_model_configs();
    low->set_label("Gemini 3 Pro (Low)");
    low->set_provider(4);
    low->mutable_model_or_alias()->set_alias(7);
    low->mutable_quota_info()->set_remaining_fraction(0.75f);

    auto *claude = models->add_client_model_configs();
    claude->set_label("Claude Sonnet");
    claude->set_provider(3);
    claude->set_disabled(true);
    claude->set_description("Temporarily unavailable");

    auto *gpt = models->add_client_model_configs();
    gpt->set_label("GPT-OSS 120B");
    gpt->set_provider(2);
    gpt->mutable_quota_info()->mutable_reset_time()->set_seconds(1);

    std::string serialized;
    QVERIFY(response.SerializeToString(&serialized));
    QByteArray payload(serialized.data(), static_cast<qsizetype>(serialized.size()));
    payload.append(QByteArray::fromHex("a00601")); // Unknown future field 100.

    const QVariantMap parsed = AntigravityMonitor::parseUserStatusPayload(payload);
    QVERIFY(parsed.value(QStringLiteral("ok")).toBool());
    QCOMPARE(parsed.value(QStringLiteral("planId")).toString(), QStringLiteral("pro"));
    QCOMPARE(parsed.value(QStringLiteral("planLabel")).toString(), QStringLiteral("Google AI Pro"));
    QCOMPARE(parsed.value(QStringLiteral("maximumPercentUsed")).toDouble(), 75.0);

    const QVariantList rows = parsed.value(QStringLiteral("rows")).toList();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Gemini 3 Pro (High)"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Gemini 3 Pro (Low)"));
    const QVariantMap exact = rows.at(0).toMap();
    QCOMPARE(exact.value(QStringLiteral("modelId")).toString(), QStringLiteral("model:1132"));
    QCOMPARE(exact.value(QStringLiteral("source")).toString(), QStringLiteral("antigravity_local"));
    QCOMPARE(exact.value(QStringLiteral("precision")).toString(), QStringLiteral("local_daemon_actual"));
    QCOMPARE(exact.value(QStringLiteral("percentRemaining")).toDouble(), 25.0);
    QVERIFY(exact.value(QStringLiteral("resetAt")).toString().endsWith(QLatin1Char('Z')));

    const QVariantMap unavailable = rows.at(2).toMap();
    QCOMPARE(unavailable.value(QStringLiteral("availability")).toString(), QStringLiteral("disabled"));
    QVERIFY(!unavailable.contains(QStringLiteral("percentUsed")));
    const QVariantMap availabilityOnly = rows.at(3).toMap();
    QCOMPARE(availabilityOnly.value(QStringLiteral("precision")).toString(), QStringLiteral("availability_only"));
    QVERIFY(!availabilityOnly.contains(QStringLiteral("percentUsed")));
    QCOMPARE(availabilityOnly.value(QStringLiteral("timeUntilReset")).toString(), QStringLiteral("now"));

    QCOMPARE(AntigravityMonitor::parseUserStatusPayload(QByteArray("broken")).value(QStringLiteral("error")).toString(),
             QStringLiteral("invalid_response"));

    exa::language_server_pb::GetUserStatusResponse signedOut;
    QVERIFY(signedOut.SerializeToString(&serialized));
    QCOMPARE(AntigravityMonitor::parseUserStatusPayload(
                 QByteArray(serialized.data(), static_cast<qsizetype>(serialized.size())))
                 .value(QStringLiteral("error"))
                 .toString(),
             QStringLiteral("not_signed_in"));
}

void AntigravityMonitorTest::connectPayloadModelsAndQuota()
{
    const QByteArray payload = R"JSON({
      "userStatus": {
        "planStatus": {"planInfo": {"planName": "Pro"}},
        "userTier": {"id": "g1-pro-tier", "name": "Google AI Pro"},
        "cascadeModelConfigData": {
          "clientModelConfigs": [
            {
              "label": "Gemini 3.5 Flash (High)",
              "modelOrAlias": {"model": "MODEL_GEMINI_3_5_FLASH_HIGH"},
              "quotaInfo": {"remainingFraction": 0.4, "resetTime": "2100-01-01T00:00:00Z"}
            },
            {
              "label": "Claude Sonnet 4.6",
              "modelOrAlias": {"model": "MODEL_CLAUDE_4_6_SONNET"},
              "quotaInfo": {"remainingFraction": 0.9, "resetTime": {"seconds": "4102444800"}}
            }
          ]
        }
      }
    })JSON";

    const QVariantMap parsed = AntigravityMonitor::parseConnectUserStatusPayload(payload);
    QVERIFY(parsed.value(QStringLiteral("ok")).toBool());
    QCOMPARE(parsed.value(QStringLiteral("planId")).toString(), QStringLiteral("pro"));
    QCOMPARE(parsed.value(QStringLiteral("planLabel")).toString(), QStringLiteral("Google AI Pro"));
    QCOMPARE(parsed.value(QStringLiteral("maximumPercentUsed")).toDouble(), 60.0);

    const QVariantList rows = parsed.value(QStringLiteral("rows")).toList();
    QCOMPARE(rows.size(), 2);
    const QVariantMap gemini = rows.at(0).toMap();
    QCOMPARE(gemini.value(QStringLiteral("modelId")).toString(), QStringLiteral("model:MODEL_GEMINI_3_5_FLASH_HIGH"));
    QCOMPARE(gemini.value(QStringLiteral("modelFamily")).toString(), QStringLiteral("google"));
    QCOMPARE(gemini.value(QStringLiteral("percentRemaining")).toDouble(), 40.0);
    QCOMPARE(gemini.value(QStringLiteral("precision")).toString(), QStringLiteral("local_daemon_actual"));
    QVERIFY(gemini.value(QStringLiteral("resetAt")).toString().endsWith(QLatin1Char('Z')));

    QCOMPARE(AntigravityMonitor::parseConnectUserStatusPayload(QByteArray("broken"))
                 .value(QStringLiteral("error"))
                 .toString(),
             QStringLiteral("invalid_response"));
}

void AntigravityMonitorTest::quotaSummaryPayload()
{
    const QByteArray payload = R"JSON({
      "response": {
        "groups": [
          {
            "displayName": "Gemini Models",
            "buckets": [
              {"bucketId": "gemini-weekly", "displayName": "Weekly Limit", "window": "weekly", "remainingFraction": 0.7, "resetTime": "2100-01-07T00:00:00Z"},
              {"bucketId": "gemini-5h", "displayName": "Five Hour Limit", "window": "5h", "remainingFraction": 0.25, "resetTime": "2100-01-01T05:00:00Z"}
            ]
          },
          {
            "displayName": "Claude and GPT models",
            "buckets": [
              {"bucketId": "3p-weekly", "displayName": "Weekly Limit", "window": "weekly", "remainingFraction": 1.0, "resetTime": "2100-01-07T00:00:00Z"},
              {"bucketId": "3p-5h", "displayName": "Five Hour Limit", "window": "5h", "remainingFraction": 0.5, "resetTime": "2100-01-01T05:00:00Z"}
            ]
          }
        ]
      }
    })JSON";

    const QVariantMap parsed = AntigravityMonitor::parseQuotaSummaryPayload(payload);
    QVERIFY(parsed.value(QStringLiteral("ok")).toBool());
    QCOMPARE(parsed.value(QStringLiteral("maximumPercentUsed")).toDouble(), 75.0);
    const QVariantList rows = parsed.value(QStringLiteral("rows")).toList();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Gemini Models — Weekly Limit"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("modelId")).toString(), QStringLiteral("bucket:gemini-weekly"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("window")).toString(), QStringLiteral("5h"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("modelFamily")).toString(), QStringLiteral("antigravity"));
}

void AntigravityMonitorTest::discoverySecurity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString validPath = directory.filePath(QStringLiteral("ls_valid.json"));
    QFile valid(validPath);
    QVERIFY(valid.open(QIODevice::WriteOnly));
    valid.write(R"JSON({"pid":1,"host":"127.0.0.1","httpsPort":443,"csrfToken":"memory-only"})JSON");
    valid.close();
    QVERIFY(valid.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    QVariantMap document;
    QString error;
    QVERIFY(AntigravityMonitor::validateDiscoveryFile(validPath, static_cast<quint32>(::getuid()), &document, &error));
    QCOMPARE(document.value(QStringLiteral("httpsPort")).toInt(), 443);
    QVERIFY(AntigravityMonitor::isLoopbackHost(QStringLiteral("::1")));
    QVERIFY(!AntigravityMonitor::isLoopbackHost(QStringLiteral("192.168.1.10")));
    QVERIFY(AntigravityMonitor::isSupportedLanguageServerPath(
        QStringLiteral("/opt/Antigravity/resources/bin/language_server")));
    QVERIFY(AntigravityMonitor::isSupportedLanguageServerPath(
        QStringLiteral("/usr/share/antigravity/resources/app/extensions/"
                       "antigravity/bin/language_server_linux_x64")));
    QVERIFY(!AntigravityMonitor::isSupportedLanguageServerPath(QStringLiteral("/tmp/language_server")));
    QVERIFY(
        !AntigravityMonitor::validateDiscoveryFile(validPath, static_cast<quint32>(::getuid() + 1), nullptr, &error));
    QCOMPARE(error, QStringLiteral("wrong_owner"));

    const QString linkPath = directory.filePath(QStringLiteral("ls_link.json"));
    QVERIFY(::symlink(QFile::encodeName(validPath).constData(), QFile::encodeName(linkPath).constData()) == 0);
    QVERIFY(!AntigravityMonitor::validateDiscoveryFile(linkPath, static_cast<quint32>(::getuid()), nullptr, &error));
    QCOMPARE(error, QStringLiteral("not_regular_file"));

    QVERIFY(valid.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::WriteGroup));
    QVERIFY(!AntigravityMonitor::validateDiscoveryFile(validPath, static_cast<quint32>(::getuid()), nullptr, &error));
    QCOMPARE(error, QStringLiteral("unsafe_permissions"));
}

void AntigravityMonitorTest::staleSnapshotIsPreserved()
{
    TestableAntigravityMonitor monitor;
    monitor.seedSnapshotAndFail();
    QCOMPARE(monitor.connectionState(), QStringLiteral("stale"));
    QCOMPARE(monitor.readinessCode(), QStringLiteral("timeout"));
    QVERIFY(monitor.syncStatus().startsWith(QStringLiteral("Stale")));
    QCOMPARE(monitor.quotaWindows().size(), 1);
    QCOMPARE(monitor.quotaWindows().first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Gemini fixture"));
}

void AntigravityMonitorTest::liveSync()
{
    if (!qEnvironmentVariableIsSet("ANTIGRAVITY_LIVE_TEST"))
        QSKIP("Set ANTIGRAVITY_LIVE_TEST=1 with Antigravity running and signed in.");

    AntigravityMonitor monitor;
    monitor.setEnabled(true);
    monitor.checkToolInstalled();
    QVERIFY(monitor.isInstalled());
    monitor.refreshQuota();
    QTRY_VERIFY_WITH_TIMEOUT(!monitor.isSyncing(), 10000);
    QCOMPARE(monitor.connectionState(), QStringLiteral("connected"));
    QVERIFY(!monitor.detectedPlanLabel().isEmpty());
    QVERIFY(!monitor.quotaWindows().isEmpty());
    for (const QVariant &value : monitor.quotaWindows())
    {
        const QVariantMap row = value.toMap();
        QVERIFY(!row.value(QStringLiteral("modelId")).toString().isEmpty());
        QVERIFY(!row.value(QStringLiteral("modelFamily")).toString().isEmpty());
        QVERIFY(!row.value(QStringLiteral("availability")).toString().isEmpty());
        QCOMPARE(row.value(QStringLiteral("source")).toString(), QStringLiteral("antigravity_local"));
    }
}

QTEST_MAIN(AntigravityMonitorTest)
#include "test_antigravitymonitor.moc"
