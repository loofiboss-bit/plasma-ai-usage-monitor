#include <QtTest>

#include "providerbackend.h"
#include "sourcereadinessmodel.h"
#include "subscriptiontoolbackend.h"

class ReadinessProvider final : public ProviderBackend
{
    Q_OBJECT
    Q_PROPERTY(QString secretAccessKey READ secretAccessKey WRITE setSecretAccessKey NOTIFY credentialsChanged)

public:
    QString name() const override { return QStringLiteral("Readiness provider"); }
    QString iconName() const override { return QStringLiteral("network-server"); }
    void refreshImpl() override { setLoading(true); }
    QString secretAccessKey() const { return m_secretAccessKey; }
    void setSecretAccessKey(const QString &secret)
    {
        if (m_secretAccessKey == secret) return;
        m_secretAccessKey = secret;
        Q_EMIT credentialsChanged();
    }

    using ProviderBackend::setConnected;
    using ProviderBackend::setErrorDetails;
    using ProviderBackend::setLoading;
    using ProviderBackend::setProviderMetric;
    using ProviderBackend::updateLastRefreshed;

Q_SIGNALS:
    void credentialsChanged();

private:
    QString m_secretAccessKey;
};

class ReadinessTool final : public SubscriptionToolBackend
{
public:
    QString toolName() const override { return QStringLiteral("Readiness tool"); }
    QString iconName() const override { return QStringLiteral("applications-development"); }
    QString toolColor() const override { return QStringLiteral("#000000"); }
    QString periodLabel() const override { return QStringLiteral("Local activity"); }
    void checkToolInstalled() override {}
    void detectActivity() override {}
    QStringList availablePlans() const override { return {}; }
    int defaultLimitForPlan(const QString &) const override { return 0; }

    void setDetected(bool installed) { setInstalled(installed); }
    void setActivity(const QDateTime &time) { setLastActivity(time); }
    void setSyncInProgress(bool syncing) { setSyncing(syncing); }
    void setVerified(const QDateTime &time) { setLastSyncTime(time); }
    void diagnostic(const QString &code) { Q_EMIT syncDiagnostic(toolName(), code, QStringLiteral("redacted")); }
    void complete(bool success) { Q_EMIT syncCompleted(success, QStringLiteral("redacted")); }

protected:
    UsagePeriod primaryPeriodType() const override { return Daily; }
};

class SourceReadinessModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void catalogContainsEverySourceExactlyOnce();
    void providerStateTransitions();
    void typedErrorsHaveDistinctActions_data();
    void typedErrorsHaveDistinctActions();
    void staleDataHasSpecificAction();
    void credentialPropertyChangesInvalidateSource();
    void localToolStateTransitions();
    void explicitVerificationUsesSafeReadOnlyContract();
    void candidateRankingIsDeterministic();
};

void SourceReadinessModelTest::catalogContainsEverySourceExactlyOnce()
{
    SourceReadinessModel model;
    QCOMPARE(model.rowCount(), 24);

    QSet<QString> ids;
    int providers = 0;
    int tools = 0;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row);
        const QString id = model.data(index, SourceReadinessModel::StableIdRole).toString();
        QVERIFY2(!id.isEmpty(), qPrintable(QStringLiteral("Empty source at row %1").arg(row)));
        QVERIFY2(!ids.contains(id), qPrintable(id));
        ids.insert(id);

        const QString kind = model.data(index, SourceReadinessModel::SourceKindKeyRole).toString();
        if (kind == QLatin1String("provider")) ++providers;
        if (kind == QLatin1String("local_tool")) ++tools;
        QVERIFY(!model.data(index, SourceReadinessModel::MonitoringLevelRole).toString().isEmpty());
        QVERIFY(model.data(index, SourceReadinessModel::SafeVerificationRole).toBool());
    }
    QCOMPARE(providers, 18);
    QCOMPARE(tools, 6);

    const QVariantMap openAi = model.source(QStringLiteral("openai"));
    QCOMPARE(openAi.value(QStringLiteral("requiredCredentialSlots")).toStringList(),
             QStringList{QStringLiteral("openai")});
    QVERIFY(!openAi.contains(QStringLiteral("backend")));
}

void SourceReadinessModelTest::providerStateTransitions()
{
    SourceReadinessModel model;
    ReadinessProvider provider;
    model.registerProviderBackend(QStringLiteral("openai"), &provider);

    auto state = [&model]() {
        return model.source(QStringLiteral("openai")).value(QStringLiteral("readinessStateKey")).toString();
    };

    QCOMPARE(state(), QStringLiteral("disabled"));
    model.setSourceEnabled(QStringLiteral("openai"), true);
    QCOMPARE(state(), QStringLiteral("needs_configuration"));

    provider.setApiKey(QStringLiteral("test-key"));
    QCOMPARE(state(), QStringLiteral("ready_to_verify"));

    provider.setLoading(true);
    QCOMPARE(state(), QStringLiteral("verifying"));
    provider.setConnected(true);
    provider.setLoading(false);
    QCOMPARE(state(), QStringLiteral("connected_connectivity_only"));

    provider.setProviderMetric(ProviderBackend::MetricKind::Cost, 1.0,
                               QStringLiteral("USD"), QStringLiteral("USD"), {}, {},
                               ProviderBackend::MetricSource::EstimatedPricing,
                               QStringLiteral("estimate"));
    QCOMPARE(state(), QStringLiteral("reporting_estimate"));

    provider.setProviderMetric(ProviderBackend::MetricKind::Cost, 1.0,
                               QStringLiteral("USD"), QStringLiteral("USD"), {}, {},
                               ProviderBackend::MetricSource::BillingApi,
                               QStringLiteral("actual"));
    QCOMPARE(state(), QStringLiteral("reporting_actual"));

    provider.updateLastRefreshed();
    provider.setErrorDetails(QStringLiteral("offline"), ProviderBackend::ProviderErrorKind::Network);
    QCOMPARE(state(), QStringLiteral("degraded"));
}

void SourceReadinessModelTest::typedErrorsHaveDistinctActions_data()
{
    QTest::addColumn<int>("errorKind");
    QTest::addColumn<QString>("action");
    QTest::addColumn<QString>("errorCode");

    QTest::newRow("configuration") << int(ProviderBackend::ProviderErrorKind::Configuration)
                                    << QStringLiteral("complete_configuration") << QStringLiteral("configuration");
    QTest::newRow("authentication") << int(ProviderBackend::ProviderErrorKind::Authentication)
                                     << QStringLiteral("replace_credentials") << QStringLiteral("authentication");
    QTest::newRow("permission") << int(ProviderBackend::ProviderErrorKind::Permission)
                                 << QStringLiteral("grant_read_only_permission") << QStringLiteral("permission");
    QTest::newRow("unsupported") << int(ProviderBackend::ProviderErrorKind::Unsupported)
                                  << QStringLiteral("review_unsupported_metric") << QStringLiteral("unsupported_metric");
    QTest::newRow("schema") << int(ProviderBackend::ProviderErrorKind::Schema)
                             << QStringLiteral("review_unsupported_metric") << QStringLiteral("schema");
    QTest::newRow("network") << int(ProviderBackend::ProviderErrorKind::Network)
                              << QStringLiteral("check_network") << QStringLiteral("network");
    QTest::newRow("rate-limit") << int(ProviderBackend::ProviderErrorKind::RateLimit)
                                 << QStringLiteral("retry_later") << QStringLiteral("rate_limit");
}

void SourceReadinessModelTest::typedErrorsHaveDistinctActions()
{
    QFETCH(int, errorKind);
    QFETCH(QString, action);
    QFETCH(QString, errorCode);

    SourceReadinessModel model;
    ReadinessProvider provider;
    model.registerProviderBackend(QStringLiteral("anthropic"), &provider);
    model.setSourceEnabled(QStringLiteral("anthropic"), true);
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setErrorDetails(QStringLiteral("localized text is ignored"),
                             static_cast<ProviderBackend::ProviderErrorKind>(errorKind));

    const QVariantMap source = model.source(QStringLiteral("anthropic"));
    QCOMPARE(source.value(QStringLiteral("nextActionKey")).toString(), action);
    QCOMPARE(source.value(QStringLiteral("errorCode")).toString(), errorCode);
    QVERIFY(!source.value(QStringLiteral("nextActionText")).toString().contains(QStringLiteral("localized text")));
}

void SourceReadinessModelTest::staleDataHasSpecificAction()
{
    SourceReadinessModel model;
    ReadinessProvider provider;
    model.registerProviderBackend(QStringLiteral("anthropic"), &provider);
    model.setSourceEnabled(QStringLiteral("anthropic"), true);
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setConnected(true);
    provider.updateLastRefreshed(QDateTime::currentDateTimeUtc().addDays(-2));

    const QVariantMap source = model.source(QStringLiteral("anthropic"));
    QCOMPARE(source.value(QStringLiteral("readinessStateKey")).toString(), QStringLiteral("degraded"));
    QCOMPARE(source.value(QStringLiteral("errorCode")).toString(), QStringLiteral("stale"));
    QCOMPARE(source.value(QStringLiteral("nextActionKey")).toString(), QStringLiteral("refresh_stale_data"));
}

void SourceReadinessModelTest::credentialPropertyChangesInvalidateSource()
{
    SourceReadinessModel model;
    ReadinessProvider provider;
    model.registerProviderBackend(QStringLiteral("bedrock"), &provider);
    model.setSourceEnabled(QStringLiteral("bedrock"), true);
    provider.setApiKey(QStringLiteral("access-key-id"));
    QCOMPARE(model.source(QStringLiteral("bedrock")).value(QStringLiteral("readinessStateKey")).toString(),
             QStringLiteral("needs_configuration"));

    QSignalSpy changedSpy(&model, &SourceReadinessModel::sourceChanged);
    provider.setSecretAccessKey(QStringLiteral("secret-access-key"));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(model.source(QStringLiteral("bedrock")).value(QStringLiteral("readinessStateKey")).toString(),
             QStringLiteral("ready_to_verify"));
}

void SourceReadinessModelTest::localToolStateTransitions()
{
    SourceReadinessModel model;
    ReadinessTool tool;
    model.registerLocalTool(QStringLiteral("claude-code"), &tool);

    auto state = [&model]() {
        return model.source(QStringLiteral("claude-code")).value(QStringLiteral("readinessStateKey")).toString();
    };

    QCOMPARE(state(), QStringLiteral("disabled"));
    tool.setEnabled(true);
    QCOMPARE(state(), QStringLiteral("unavailable_locally"));
    tool.setDetected(true);
    QCOMPARE(state(), QStringLiteral("ready_to_verify"));
    tool.setSyncInProgress(true);
    QCOMPARE(state(), QStringLiteral("verifying"));
    tool.setSyncInProgress(false);
    tool.setActivity(QDateTime::currentDateTimeUtc());
    QCOMPARE(state(), QStringLiteral("reporting_estimate"));
    tool.setVerified(QDateTime::currentDateTimeUtc());
    QCOMPARE(state(), QStringLiteral("reporting_actual"));
    tool.diagnostic(QStringLiteral("network_error"));
    QCOMPARE(state(), QStringLiteral("degraded"));
    QCOMPARE(model.source(QStringLiteral("claude-code")).value(QStringLiteral("nextActionKey")).toString(),
             QStringLiteral("check_network"));
    tool.complete(true);
    QCOMPARE(state(), QStringLiteral("reporting_actual"));
}

void SourceReadinessModelTest::explicitVerificationUsesSafeReadOnlyContract()
{
    SourceReadinessModel model;
    ReadinessTool tool;
    model.registerLocalTool(QStringLiteral("codex-cli"), &tool);
    tool.setEnabled(true);
    tool.setDetected(true);

    QVERIFY(model.verifySource(QStringLiteral("codex-cli")));
    const QVariantMap source = model.source(QStringLiteral("codex-cli"));
    QCOMPARE(source.value(QStringLiteral("readinessStateKey")).toString(),
             QStringLiteral("reporting_estimate"));
    QVERIFY(source.value(QStringLiteral("lastVerified")).toDateTime().isValid());

    SourceReadinessModel disabledModel;
    ReadinessProvider disabledProvider;
    disabledModel.registerProviderBackend(QStringLiteral("openai"), &disabledProvider);
    QVERIFY(!disabledModel.verifySource(QStringLiteral("openai")));

    const QVariantMap liteLlm = disabledModel.source(QStringLiteral("litellm"));
    QVERIFY(liteLlm.value(QStringLiteral("customEndpointRequired")).toBool());
}

void SourceReadinessModelTest::candidateRankingIsDeterministic()
{
    SourceReadinessModel model;
    ReadinessTool codex;
    ReadinessTool claude;
    model.registerLocalTool(QStringLiteral("codex-cli"), &codex);
    model.registerLocalTool(QStringLiteral("claude-code"), &claude);
    codex.setDetected(true);

    const QStringList first = model.rankedSourceIds();
    const QStringList second = model.rankedSourceIds();
    QCOMPARE(first, second);
    QCOMPARE(first.first(), QStringLiteral("codex-cli"));
    QVERIFY(first.indexOf(QStringLiteral("openai")) < first.indexOf(QStringLiteral("anthropic")));
    QVERIFY(first.indexOf(QStringLiteral("litellm")) < first.indexOf(QStringLiteral("deepseek")));
    QVERIFY(first.indexOf(QStringLiteral("deepseek")) < first.indexOf(QStringLiteral("claude-code")));
}

QTEST_MAIN(SourceReadinessModelTest)
#include "test_sourcereadinessmodel.moc"
