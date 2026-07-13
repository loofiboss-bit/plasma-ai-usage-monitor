#include <QtTest>

#include <QHash>
#include <QSet>
#include <QSignalSpy>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QJsonObject>

#include "anthropicprovider.h"
#include "cohereprovider.h"
#include "deepseekprovider.h"
#include "googleprovider.h"
#include "googleveoprovider.h"
#include "azureopenaiprovider.h"
#include "groqprovider.h"
#include "mistralprovider.h"
#include "ollamacloudprovider.h"
#include "openaiprovider.h"
#include "openrouterprovider.h"
#include "providerbackend.h"
#include "togetherprovider.h"
#include "xaiprovider.h"

class HttpStubServer : public QObject
{
    Q_OBJECT

public:
    struct Response {
        int status = 200;
        QByteArray body = "{}";
        QList<QPair<QByteArray, QByteArray>> headers;
        int delayMs = 0;
    };

    explicit HttpStubServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (m_server.hasPendingConnections()) {
                QTcpSocket *socket = m_server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                    m_buffers[socket] += socket->readAll();
                    if (!m_buffers[socket].contains("\r\n\r\n")) {
                        return;
                    }

                    const QList<QByteArray> lines = m_buffers[socket].split('\n');
                    if (lines.isEmpty()) {
                        socket->disconnectFromHost();
                        return;
                    }

                    const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
                    if (firstLine.size() < 2) {
                        socket->disconnectFromHost();
                        return;
                    }

                    const QString method = QString::fromUtf8(firstLine.at(0));
                    const QString rawTarget = QString::fromUtf8(firstLine.at(1));
                    const QString path = QUrl(rawTarget).path();

                    m_hitCount[path] = m_hitCount.value(path) + 1;

                    const QString key = method + QStringLiteral(" ") + path;
                    Response response;
                    if (m_routeSequences.contains(key) && !m_routeSequences[key].isEmpty()) {
                        response = m_routeSequences[key].takeFirst();
                        if (m_routeSequences[key].isEmpty()) {
                            m_routeSequences.remove(key);
                        }
                    } else {
                        response = m_routes.value(key, Response{404, "{\"error\":\"not found\"}", {}, 0});
                    }

                    QByteArray payload;
                    payload += "HTTP/1.1 " + QByteArray::number(response.status) + " OK\r\n";
                    payload += "Content-Type: application/json\r\n";
                    for (const auto &header : response.headers) {
                        payload += header.first + ": " + header.second + "\r\n";
                    }
                    payload += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
                    payload += "Connection: close\r\n\r\n";
                    payload += response.body;

                    auto sendPayload = [socket, payload]() {
                        if (socket->state() != QAbstractSocket::ConnectedState) {
                            return;
                        }
                        socket->write(payload);
                        socket->disconnectFromHost();
                    };

                    if (response.delayMs > 0) {
                        QTimer::singleShot(response.delayMs, socket, sendPayload);
                    } else {
                        sendPayload();
                    }
                });

                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
    }

    void setResponse(const QString &method,
                     const QString &path,
                     int status,
                     const QByteArray &body,
                     const QList<QPair<QByteArray, QByteArray>> &headers = {})
    {
        m_routes.insert(method + QStringLiteral(" ") + path, Response{status, body, headers});
    }

    void setResponseSequence(const QString &method,
                             const QString &path,
                             const QList<Response> &responses)
    {
        m_routeSequences.insert(method + QStringLiteral(" ") + path, responses);
    }

    int hitCount(const QString &path) const
    {
        return m_hitCount.value(path, 0);
    }

private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QString, Response> m_routes;
    QHash<QString, QList<Response>> m_routeSequences;
    QHash<QString, int> m_hitCount;
};

class ProvidersMockedHttpTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void openAiSuccessAndHeaders();
    void openAiLegacyNumericCostFallback();
    void openAiEmptyBucketsAndObjectCosts();
    void openAiMalformedUsageAndNonUsdCostWarning();
    void openAiRetryAfterThenSuccess();
    void openAiAuthError();
    void openAiPartialCapabilitySuccess();
    void anthropicRateLimitHeaders();
    void deepSeekUsageAndBalance();
    void openAiCompatibleProbeRefreshesDoNotAffectActualUsage();
    void googleKnownLimitsByTier();
    void googleDiscoveryPaginatesDeduplicatesAndMergesFallback();
    void googleVeoKnownLimitsByTier();
    void googleVeoUsesHeaderLimitsWhenPresent();
    void googleVeoPartialHeadersFallbackToKnownLimits();
    void googleVeoUsagePayloadEstimatedCost();
    void googleVeoDurationSecondsEstimatedCost();
    void googleVeoAuthError();
    void ollamaStaleGenerationDiscarded();
    void openRouterUsageAndCredits();
    void openRouterMissingFieldsRemainUnavailable();
    void togetherAiUsageAndHeaders();
    void cohereUsageAndHeaders();
    void mistralUsageAndHeaders();
    void groqUsageAndHeaders();
    void xaiUsageAndHeaders();
    void azureProviderSuccess();
    void azureProviderMeteredCostPreferred();
    void azureProviderAuthError();
    void azureNormalizeHappyPath();
    void azureNormalizeFailurePath();
};

static void assertProbeOnlyState(const ProviderBackend &provider,
                                 qint64 inputTokens,
                                 qint64 outputTokens,
                                 int requests = 1)
{
    QCOMPARE(provider.inputTokens(), 0);
    QCOMPARE(provider.outputTokens(), 0);
    QCOMPARE(provider.requestCount(), 0);
    QCOMPARE(provider.actualInputTokens(), 0);
    QCOMPARE(provider.actualOutputTokens(), 0);
    QCOMPARE(provider.actualRequestCount(), 0);
    QCOMPARE(provider.cost(), 0.0);
    QCOMPARE(provider.dailyCost(), 0.0);
    QCOMPARE(provider.monthlyCost(), 0.0);
    QCOMPARE(provider.probeInputTokens(), inputTokens);
    QCOMPARE(provider.probeOutputTokens(), outputTokens);
    QCOMPARE(provider.probeRequestCount(), requests);
    QCOMPARE(provider.usageSource(), QStringLiteral("connectivity_probe"));
    QCOMPARE(provider.costSource(), QStringLiteral("connectivity_probe"));
    QCOMPARE(provider.dataQuality(), QStringLiteral("probe_only"));
}

void ProvidersMockedHttpTest::openAiSuccessAndHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "data": [{
            "result": [{
                "input_tokens": 100,
                "output_tokens": 50,
                "num_model_requests": 7
            }]
        }]
    })JSON";

    const QByteArray costsBody = R"JSON({
        "data": [{
            "results": [{
                "amount": {
                    "value": 0.06,
                    "currency": "usd"
                }
            }]
        }]
    })JSON";

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1/organization/usage/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "100"},
            {"x-ratelimit-remaining-requests", "60"},
            {"x-ratelimit-limit-tokens", "2000"},
            {"x-ratelimit-remaining-tokens", "1500"},
            {"x-ratelimit-reset-requests", "30s"},
        });
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200, costsBody);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 100);
    QCOMPARE(provider.outputTokens(), 50);
    QCOMPARE(provider.requestCount(), 7);
    QCOMPARE(provider.actualInputTokens(), 100);
    QCOMPARE(provider.actualOutputTokens(), 50);
    QCOMPARE(provider.actualRequestCount(), 7);
    QCOMPARE(provider.usageSource(), QStringLiteral("actual_api"));
    QCOMPARE(provider.rateLimitRequests(), 100);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 60);
    QCOMPARE(provider.rateLimitTokens(), 2000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 1500);
    QCOMPARE(provider.rateLimitResetTime(), QStringLiteral("30s"));
    QCOMPARE(provider.dailyCost(), 0.06);
    QCOMPARE(provider.monthlyCost(), 0.06);
    QCOMPARE(provider.cost(), 0.06);
    QCOMPARE(provider.costSource(), QStringLiteral("billing_api"));
    QCOMPARE(provider.currency(), QStringLiteral("USD"));
    QCOMPARE(provider.dataQuality(), QStringLiteral("actual_billing"));
    QVERIFY(provider.isConnected());

    QVERIFY(server.hitCount(QStringLiteral("/v1/organization/usage/completions")) >= 1);
    QVERIFY(server.hitCount(QStringLiteral("/v1/organization/costs")) >= 2);
}

void ProvidersMockedHttpTest::openAiLegacyNumericCostFallback()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({"data":[{"results":[]}]})JSON";
    const QByteArray costsBody = R"JSON({
        "data": [{
            "result": [{
                "amount": 250
            }]
        }]
    })JSON";

    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200, usageBody);
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200, costsBody);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.dailyCost(), 2.5);
    QCOMPARE(provider.monthlyCost(), 2.5);
    QCOMPARE(provider.costSource(), QStringLiteral("billing_api"));
}

void ProvidersMockedHttpTest::openAiEmptyBucketsAndObjectCosts()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({"data":[{"results":[]}]})JSON";
    const QByteArray costsBody = R"JSON({
        "data": [{
            "results": [{
                "amount": {
                    "value": 0,
                    "currency": "usd"
                }
            }]
        }]
    })JSON";

    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200, usageBody);
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200, costsBody);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 0);
    QCOMPARE(provider.outputTokens(), 0);
    QCOMPARE(provider.requestCount(), 0);
    QCOMPARE(provider.dailyCost(), 0.0);
    QCOMPARE(provider.monthlyCost(), 0.0);
    QCOMPARE(provider.usageSource(), QStringLiteral("actual_api"));
    QCOMPARE(provider.costSource(), QStringLiteral("billing_api"));
}

void ProvidersMockedHttpTest::openAiMalformedUsageAndNonUsdCostWarning()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray malformedUsage = QByteArrayLiteral("{");
    const QByteArray costsBody = R"JSON({
        "data": [{
            "results": [{
                "amount": {
                    "value": 1.25,
                    "currency": "eur"
                }
            }]
        }]
    })JSON";

    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200, malformedUsage);
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200, costsBody);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    QTest::ignoreMessage(QtWarningMsg, "OpenAI cost currency is not USD: \"eur\"");
    QTest::ignoreMessage(QtWarningMsg, "OpenAI cost currency is not USD: \"eur\"");
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.dailyCost(), 1.25);
    QCOMPARE(provider.monthlyCost(), 1.25);
    QCOMPARE(provider.costSource(), QStringLiteral("billing_api"));
    QVERIFY(!provider.errorString().isEmpty());
    QCOMPARE(provider.errorKind(), ProviderBackend::ProviderErrorKind::Schema);
}

void ProvidersMockedHttpTest::openAiRetryAfterThenSuccess()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "data": [{
            "results": [{
                "input_tokens": 12,
                "output_tokens": 3,
                "num_model_requests": 1
            }]
        }]
    })JSON";
    const QByteArray costsBody = R"JSON({
        "data": [{
            "results": [{
                "amount": {
                    "value": 0.02,
                    "currency": "usd"
                }
            }]
        }]
    })JSON";

    server.setResponseSequence(
        QStringLiteral("GET"),
        QStringLiteral("/v1/organization/usage/completions"),
        {
            HttpStubServer::Response{429, QByteArrayLiteral(R"JSON({"error":"rate_limited"})JSON"), {{"Retry-After", "1"}}, 0},
            HttpStubServer::Response{200, usageBody, {}, 0},
        });
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200, costsBody);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 5000);

    QTRY_COMPARE_WITH_TIMEOUT(provider.inputTokens(), 12, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(provider.outputTokens(), 3, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(provider.requestCount(), 1, 5000);
    QVERIFY(server.hitCount(QStringLiteral("/v1/organization/usage/completions")) >= 2);
}

void ProvidersMockedHttpTest::openAiAuthError()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray authError = R"JSON({"error":"unauthorized"})JSON";
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 401, authError);
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 401, authError);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("bad-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy errorSpy(&provider, &ProviderBackend::errorChanged);
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(provider.errorCount() >= 1);
    QVERIFY(!provider.errorString().isEmpty());
    QVERIFY(!provider.isConnected());
    QCOMPARE(provider.errorKind(), ProviderBackend::ProviderErrorKind::Authentication);
    QCOMPARE(provider.httpStatus(), 401);
    QVERIFY(!provider.isRetryable());
}

void ProvidersMockedHttpTest::openAiPartialCapabilitySuccess()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponse(
        QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200,
        QByteArrayLiteral(R"JSON({"data":[{"results":[{"input_tokens":12,"output_tokens":3,"num_model_requests":1}]}]})JSON"));
    server.setResponseSequence(
        QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"),
        {HttpStubServer::Response{403, QByteArrayLiteral("{}"), {}, 0},
         HttpStubServer::Response{403, QByteArrayLiteral("{}"), {}, 0}});

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QVERIFY(provider.isConnected());
    QCOMPARE(provider.actualInputTokens(), 12);
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("usage")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("daily_billing")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("monthly_billing")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
}

void ProvidersMockedHttpTest::anthropicRateLimitHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/v1/messages/count_tokens"),
        200,
        QByteArrayLiteral("{}"),
        {
            {"anthropic-ratelimit-requests-limit", "80"},
            {"anthropic-ratelimit-requests-remaining", "20"},
            {"anthropic-ratelimit-input-tokens-limit", "1000"},
            {"anthropic-ratelimit-input-tokens-remaining", "400"},
            {"anthropic-ratelimit-output-tokens-limit", "2000"},
            {"anthropic-ratelimit-output-tokens-remaining", "900"},
            {"anthropic-ratelimit-requests-reset", "2026-02-16T12:34:56Z"},
        });

    AnthropicProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.countTokensDiagnostic();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.rateLimitRequests(), 80);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 20);
    QCOMPARE(provider.rateLimitTokens(), 3000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 1300);
    QVERIFY(provider.isConnected());
    QVERIFY(!provider.rateLimitResetTime().isEmpty());
}

void ProvidersMockedHttpTest::deepSeekUsageAndBalance()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 11,
            "completion_tokens": 9
        }
    })JSON";

    const QByteArray balanceBody = R"JSON({
        "is_available": true,
        "balance_infos": [
            {"total_balance": "12.34"},
            {"total_balance": "0.66"}
        ]
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "120"},
            {"x-ratelimit-remaining-requests", "110"},
            {"x-ratelimit-limit-tokens", "6000"},
            {"x-ratelimit-remaining-tokens", "5800"},
            {"x-ratelimit-reset-requests", "20s"},
        });
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/user/balance"), 200, balanceBody);
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/models"), 200, QByteArrayLiteral(R"JSON({"data":[]})JSON"));

    DeepSeekProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(provider.balance(), 13.0, 3000);
    provider.testConnectionNow();
    QTRY_COMPARE_WITH_TIMEOUT(provider.probeRequestCount(), 1, 3000);

    assertProbeOnlyState(provider, 11, 9);
    QCOMPARE(provider.rateLimitRequests(), 120);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 110);
    QCOMPARE(provider.rateLimitTokens(), 6000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 5800);
    QCOMPARE(provider.balance(), 13.0);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::openAiCompatibleProbeRefreshesDoNotAffectActualUsage()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 11,
            "completion_tokens": 9
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "120"},
            {"x-ratelimit-remaining-requests", "110"},
            {"x-ratelimit-limit-tokens", "6000"},
            {"x-ratelimit-remaining-tokens", "5800"},
        });

    MistralProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());
    provider.setDailyBudget(0.01);
    provider.setMonthlyBudget(0.01);

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    QSignalSpy budgetWarningSpy(&provider, &ProviderBackend::budgetWarning);
    QSignalSpy budgetExceededSpy(&provider, &ProviderBackend::budgetExceeded);

    for (int i = 0; i < 100; ++i) {
        const int expectedProbeCount = provider.probeRequestCount() + 1;
        provider.testConnectionNow();
        QTRY_COMPARE_WITH_TIMEOUT(provider.probeRequestCount(), expectedProbeCount, 3000);
    }

    QCOMPARE(provider.inputTokens(), 0);
    QCOMPARE(provider.outputTokens(), 0);
    QCOMPARE(provider.requestCount(), 0);
    QCOMPARE(provider.actualInputTokens(), 0);
    QCOMPARE(provider.actualOutputTokens(), 0);
    QCOMPARE(provider.actualRequestCount(), 0);
    QCOMPARE(provider.cost(), 0.0);
    QCOMPARE(provider.dailyCost(), 0.0);
    QCOMPARE(provider.monthlyCost(), 0.0);
    QCOMPARE(provider.probeInputTokens(), 1100);
    QCOMPARE(provider.probeOutputTokens(), 900);
    QCOMPARE(provider.probeRequestCount(), 100);
    QCOMPARE(provider.usageSource(), QStringLiteral("connectivity_probe"));
    QCOMPARE(provider.costSource(), QStringLiteral("connectivity_probe"));
    QCOMPARE(budgetWarningSpy.count(), 0);
    QCOMPARE(budgetExceededSpy.count(), 0);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleKnownLimitsByTier()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models"),
        200,
        QByteArrayLiteral(R"JSON({"models":[{"name":"models/gemini-2.5-flash"}]})JSON"));

    GoogleProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("gemini-2.5-flash"));
    provider.setTier(QStringLiteral("paid"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.rateLimitRequests(), 0);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 0);
    QCOMPARE(provider.rateLimitTokens(), 0);
    QCOMPARE(provider.rateLimitTokensRemaining(), 0);
    QVERIFY(provider.selectedModelAvailable());
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleDiscoveryPaginatesDeduplicatesAndMergesFallback()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponseSequence(
        QStringLiteral("GET"), QStringLiteral("/v1beta/models"),
        {HttpStubServer::Response{200, QByteArrayLiteral(R"JSON({"models":[{"name":"models/live-a"}],"nextPageToken":"page-2"})JSON"), {}, 0},
         HttpStubServer::Response{200, QByteArrayLiteral(R"JSON({"models":[{"name":"models/live-a"},{"name":"models/live-preview"}]})JSON"), {}, 0}});
    GoogleProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("live-preview"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refreshModelsNow();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QCOMPARE(server.hitCount(QStringLiteral("/v1beta/models")), 2);
    QSet<QString> ids;
    for (const QVariant &entry : provider.discoveredModels())
        ids.insert(entry.toMap().value(QStringLiteral("id")).toString());
    QCOMPARE(ids.size(), provider.discoveredModels().size());
    QVERIFY(ids.contains(QStringLiteral("live-a")));
    QVERIFY(ids.contains(QStringLiteral("live-preview")));
    QVERIFY(ids.contains(QStringLiteral("gemini-2.5-pro")));
    QVERIFY(provider.selectedModelAvailable());
    QVERIFY(!provider.selectedModelWarning().isEmpty());
}

void ProvidersMockedHttpTest::googleVeoKnownLimitsByTier()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray modelInfoBody = R"JSON({
        "name": "models/veo-2",
        "displayName": "Veo 2"
    })JSON";

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models/veo-3.1-generate-preview"),
        200,
        modelInfoBody);

    GoogleVeoProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("veo-3.1-generate-preview"));
    provider.setTier(QStringLiteral("free"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.rateLimitRequests(), 0);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 0);
    QCOMPARE(provider.rateLimitTokens(), 0);
    QCOMPARE(provider.rateLimitTokensRemaining(), 0);
    assertProbeOnlyState(provider, 0, 0);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleVeoUsesHeaderLimitsWhenPresent()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray modelInfoBody = R"JSON({
        "name": "models/veo-3",
        "displayName": "Veo 3"
    })JSON";

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models/veo-3"),
        200,
        modelInfoBody,
        {
            {"x-ratelimit-limit-requests", "77"},
            {"x-ratelimit-remaining-requests", "66"},
            {"x-ratelimit-limit-tokens", "12345"},
            {"x-ratelimit-remaining-tokens", "12000"},
            {"x-ratelimit-reset-requests", "45s"},
        });

    GoogleVeoProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("veo-3"));
    provider.setTier(QStringLiteral("paid"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.rateLimitRequests(), 77);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 66);
    QCOMPARE(provider.rateLimitTokens(), 12345);
    QCOMPARE(provider.rateLimitTokensRemaining(), 12000);
    QCOMPARE(provider.rateLimitResetTime(), QStringLiteral("45s"));
    assertProbeOnlyState(provider, 0, 0);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleVeoPartialHeadersFallbackToKnownLimits()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray modelInfoBody = R"JSON({
        "name": "models/veo-3",
        "displayName": "Veo 3"
    })JSON";

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models/veo-3"),
        200,
        modelInfoBody,
        {
            {"x-ratelimit-limit-requests", "77"},
            {"x-ratelimit-limit-tokens", "12345"},
            {"x-ratelimit-remaining-tokens", "12000"},
        });

    GoogleVeoProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("veo-3"));
    provider.setTier(QStringLiteral("paid"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    // A partial header pair must remain unavailable, not become a published cap.
    QCOMPARE(provider.rateLimitRequests(), 0);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 0);
    QCOMPARE(provider.rateLimitTokens(), 12345);
    QCOMPARE(provider.rateLimitTokensRemaining(), 12000);
    assertProbeOnlyState(provider, 0, 0);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleVeoUsagePayloadEstimatedCost()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray modelInfoBody = R"JSON({
        "name": "models/veo-3.1-generate-preview",
        "usage": {
            "prompt_tokens": 120000,
            "completion_tokens": 30000,
            "total_tokens": 150000
        }
    })JSON";

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models/veo-3.1-generate-preview"),
        200,
        modelInfoBody,
        {
            {"x-ratelimit-limit-requests", "44"},
            {"x-ratelimit-remaining-requests", "40"},
        });

    GoogleVeoProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("veo-3.1-generate-preview"));
    provider.setTier(QStringLiteral("paid"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 120000);
    QCOMPARE(provider.outputTokens(), 30000);
    QCOMPARE(provider.requestCount(), 1);
    QCOMPARE(provider.cost(), 0.0);
    QVERIFY(!provider.isEstimatedCost());
    QCOMPARE(provider.rateLimitRequests(), 44);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 40);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleVeoDurationSecondsEstimatedCost()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray modelInfoBody = R"JSON({
        "name": "models/veo-3.1-generate-preview",
        "usage": {
            "prompt_tokens": 10,
            "completion_tokens": 5,
            "video_duration_seconds": 8
        }
    })JSON";

    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models/veo-3.1-generate-preview"),
        200,
        modelInfoBody,
        {
            {"x-ratelimit-limit-requests", "44"},
            {"x-ratelimit-remaining-requests", "40"},
        });

    GoogleVeoProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("veo-3.1-generate-preview"));
    provider.setTier(QStringLiteral("paid"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 10);
    QCOMPARE(provider.outputTokens(), 5);
    QCOMPARE(provider.requestCount(), 1);
    QCOMPARE(provider.cost(), 3.2);
    QVERIFY(provider.isEstimatedCost());
    QCOMPARE(provider.rateLimitRequests(), 44);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 40);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::googleVeoAuthError()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray authError = R"JSON({"error":"unauthorized"})JSON";
    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1beta/models/veo-3"),
        404,
        authError);

    GoogleVeoProvider provider;
    provider.setApiKey(QStringLiteral("bad-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1beta"));
    provider.setModel(QStringLiteral("veo-3"));

    QSignalSpy errorSpy(&provider, &ProviderBackend::errorChanged);
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(provider.errorCount() >= 1);
    QVERIFY(!provider.errorString().isEmpty());
    QVERIFY(!provider.isConnected());
    QCOMPARE(provider.errorKind(), ProviderBackend::ProviderErrorKind::Configuration);
    QCOMPARE(provider.httpStatus(), 404);
}

void ProvidersMockedHttpTest::ollamaStaleGenerationDiscarded()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray staleBody = R"JSON({
        "usage": {
            "prompt_tokens": 10,
            "completion_tokens": 1,
            "total_tokens": 11
        }
    })JSON";

    const QByteArray freshBody = R"JSON({
        "usage": {
            "prompt_tokens": 99,
            "completion_tokens": 5,
            "total_tokens": 104
        }
    })JSON";

    server.setResponseSequence(
        QStringLiteral("POST"),
        QStringLiteral("/v1/chat/completions"),
        {
            HttpStubServer::Response{
                200,
                staleBody,
                {
                    {"x-ratelimit-limit-requests", "100"},
                    {"x-ratelimit-remaining-requests", "90"},
                    {"x-ratelimit-limit-tokens", "2000"},
                    {"x-ratelimit-remaining-tokens", "1900"},
                },
                250
            },
            HttpStubServer::Response{
                200,
                freshBody,
                {
                    {"x-ratelimit-limit-requests", "100"},
                    {"x-ratelimit-remaining-requests", "10"},
                    {"x-ratelimit-limit-tokens", "2000"},
                    {"x-ratelimit-remaining-tokens", "1500"},
                },
                0
            }
        });

    OllamaCloudProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);

    provider.testConnectionNow();
    QTest::qWait(50);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QTest::qWait(300);

    assertProbeOnlyState(provider, 99, 5);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 10);
    QCOMPARE(provider.rateLimitTokensRemaining(), 1500);
    QCOMPARE(server.hitCount(QStringLiteral("/v1/chat/completions")), 2);
}

void ProvidersMockedHttpTest::openRouterUsageAndCredits()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray creditsBody = R"JSON({
        "data": {
            "label": "my-key",
            "usage": 3.50,
            "usage_daily": 0.25,
            "usage_weekly": 1.25,
            "usage_monthly": 2.75,
            "limit": 25.00,
            "limit_remaining": 21.50,
            "limit_reset": "monthly"
        }
    })JSON";

    server.setResponse(QStringLiteral("GET"), QStringLiteral("/models"), 200, QByteArrayLiteral(R"JSON({"data":[]})JSON"));
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/key"), 200, creditsBody);

    OpenRouterProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.probeRequestCount(), 0);
    QCOMPARE(provider.cost(), 3.50);
    QCOMPARE(provider.usageDaily(), 0.25);
    QCOMPARE(provider.usageWeekly(), 1.25);
    QCOMPARE(provider.usageMonthly(), 2.75);
    QCOMPARE(provider.rateLimitRequests(), 0);
    QCOMPARE(provider.credits(), 21.50);
    QVERIFY(provider.isConnected());

    QCOMPARE(server.hitCount(QStringLiteral("/chat/completions")), 0);
    QVERIFY(server.hitCount(QStringLiteral("/key")) >= 1);
    QCOMPARE(server.hitCount(QStringLiteral("/models")), 0);
}

void ProvidersMockedHttpTest::openRouterMissingFieldsRemainUnavailable()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/key"), 200,
                       QByteArrayLiteral(R"JSON({"data":{"label":"limited-key"}})JSON"));
    OpenRouterProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QVERIFY(provider.isConnected());
    QVERIFY(!provider.creditsAvailable());
    const QVariantMap credits = provider.metric(QStringLiteral("credit_balance"));
    const QVariantMap daily = provider.metric(QStringLiteral("cost"), QStringLiteral("api_key"), QStringLiteral("day"));
    QVERIFY(!credits.value(QStringLiteral("available")).toBool());
    QVERIFY(!daily.value(QStringLiteral("available")).toBool());
    QCOMPARE(server.hitCount(QStringLiteral("/key")), 1);
}

void ProvidersMockedHttpTest::togetherAiUsageAndHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 50,
            "completion_tokens": 30
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "60"},
            {"x-ratelimit-remaining-requests", "55"},
            {"x-ratelimit-limit-tokens", "4000"},
            {"x-ratelimit-remaining-tokens", "3800"},
            {"x-ratelimit-reset-requests", "60s"},
        });

    TogetherProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 50, 30);
    QCOMPARE(provider.rateLimitRequests(), 60);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 55);
    QCOMPARE(provider.rateLimitTokens(), 4000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 3800);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::cohereUsageAndHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 75,
            "completion_tokens": 25
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "40"},
            {"x-ratelimit-remaining-requests", "35"},
            {"x-ratelimit-limit-tokens", "8000"},
            {"x-ratelimit-remaining-tokens", "7700"},
        });

    CohereProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 75, 25);
    QCOMPARE(provider.rateLimitRequests(), 40);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 35);
    QCOMPARE(provider.rateLimitTokens(), 8000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 7700);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::mistralUsageAndHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 90,
            "completion_tokens": 45
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "75"},
            {"x-ratelimit-remaining-requests", "71"},
            {"x-ratelimit-limit-tokens", "5000"},
            {"x-ratelimit-remaining-tokens", "4865"},
        });

    MistralProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 90, 45);
    QCOMPARE(provider.rateLimitRequests(), 75);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 71);
    QCOMPARE(provider.rateLimitTokens(), 5000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 4865);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::groqUsageAndHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 64,
            "completion_tokens": 16
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "120"},
            {"x-ratelimit-remaining-requests", "118"},
            {"x-ratelimit-limit-tokens", "6400"},
            {"x-ratelimit-remaining-tokens", "6320"},
        });

    GroqProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 64, 16);
    QCOMPARE(provider.rateLimitRequests(), 120);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 118);
    QCOMPARE(provider.rateLimitTokens(), 6400);
    QCOMPARE(provider.rateLimitTokensRemaining(), 6320);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::xaiUsageAndHeaders()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "usage": {
            "prompt_tokens": 300,
            "completion_tokens": 120
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "33"},
            {"x-ratelimit-remaining-requests", "30"},
            {"x-ratelimit-limit-tokens", "3300"},
            {"x-ratelimit-remaining-tokens", "2880"},
        });

    XAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 300, 120);
    QCOMPARE(provider.rateLimitRequests(), 33);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 30);
    QCOMPARE(provider.rateLimitTokens(), 3300);
    QCOMPARE(provider.rateLimitTokensRemaining(), 2880);
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::azureProviderSuccess()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "id": "chatcmpl-azure-test",
        "object": "chat.completion",
        "usage": {
            "prompt_tokens": 42,
            "completion_tokens": 8,
            "total_tokens": 50
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/openai/deployments/my-deployment/chat/completions"),
        200,
        usageBody,
        {
            {"x-ratelimit-limit-requests", "90"},
            {"x-ratelimit-remaining-requests", "70"},
            {"x-ratelimit-limit-tokens", "9000"},
            {"x-ratelimit-remaining-tokens", "8750"},
            {"x-ratelimit-reset-requests", "25s"},
        });

    AzureOpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setDeploymentId(QStringLiteral("my-deployment"));
    provider.setModel(QStringLiteral("gpt-5.4"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 42, 8);
    QCOMPARE(provider.rateLimitRequests(), 90);
    QCOMPARE(provider.rateLimitRequestsRemaining(), 70);
    QCOMPARE(provider.rateLimitTokens(), 9000);
    QCOMPARE(provider.rateLimitTokensRemaining(), 8750);
    QCOMPARE(provider.rateLimitResetTime(), QStringLiteral("25s"));
    QVERIFY(!provider.isEstimatedCost());
    QVERIFY(provider.isConnected());

    QVERIFY(server.hitCount(QStringLiteral("/openai/deployments/my-deployment/chat/completions")) >= 1);
}

void ProvidersMockedHttpTest::azureProviderMeteredCostPreferred()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray usageBody = R"JSON({
        "id": "chatcmpl-azure-metered",
        "object": "chat.completion",
        "usage": {
            "prompt_tokens": 120,
            "completion_tokens": 30,
            "total_tokens": 150
        },
        "cost": {
            "total_cost": 0.0125,
            "daily_cost": 0.05,
            "monthly_cost": 0.25
        }
    })JSON";

    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/openai/deployments/my-deployment/chat/completions"),
        200,
        usageBody);

    AzureOpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setDeploymentId(QStringLiteral("my-deployment"));
    provider.setModel(QStringLiteral("gpt-5.4"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    assertProbeOnlyState(provider, 120, 30);
    QCOMPARE(provider.dailyCost(), 0.0);
    QCOMPARE(provider.monthlyCost(), 0.0);
    QVERIFY(!provider.isEstimatedCost());
    QVERIFY(provider.isConnected());
}

void ProvidersMockedHttpTest::azureProviderAuthError()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    const QByteArray authError = R"JSON({"error":"unauthorized"})JSON";
    server.setResponse(
        QStringLiteral("POST"),
        QStringLiteral("/openai/deployments/my-deployment/chat/completions"),
        401,
        authError);

    AzureOpenAIProvider provider;
    provider.setApiKey(QStringLiteral("bad-key"));
    provider.setDeploymentId(QStringLiteral("my-deployment"));
    provider.setCustomBaseUrl(server.baseUrl());

    QSignalSpy errorSpy(&provider, &ProviderBackend::errorChanged);
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.testConnectionNow();

    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(provider.errorCount() >= 1);
    QVERIFY(!provider.errorString().isEmpty());
    QVERIFY(!provider.isConnected());
    QCOMPARE(provider.errorKind(), ProviderBackend::ProviderErrorKind::Authentication);
    QCOMPARE(provider.httpStatus(), 401);
}

void ProvidersMockedHttpTest::azureNormalizeHappyPath()
{
    QJsonObject usage;
    usage.insert(QStringLiteral("prompt_tokens"), 321);
    usage.insert(QStringLiteral("completion_tokens"), 123);
    usage.insert(QStringLiteral("total_tokens"), 444);

    QJsonObject cost;
    cost.insert(QStringLiteral("total_cost"), 1.5);
    cost.insert(QStringLiteral("daily_cost"), 1.25);
    cost.insert(QStringLiteral("monthly_cost"), 9.75);

    QJsonObject payload;
    payload.insert(QStringLiteral("usage"), usage);
    payload.insert(QStringLiteral("cost"), cost);

    const ProviderBackend::NormalizedUsageCost normalized =
        ProviderBackend::normalizeUsageCost(ProviderBackend::ProviderId::AzureOpenAI, payload);

    QVERIFY(normalized.parsed);
    QCOMPARE(normalized.inputTokens, 321);
    QCOMPARE(normalized.outputTokens, 123);
    QCOMPARE(normalized.requestCount, 1);
    QCOMPARE(normalized.cost, 1.5);
    QCOMPARE(normalized.dailyCost, 1.25);
    QCOMPARE(normalized.monthlyCost, 9.75);
}

void ProvidersMockedHttpTest::azureNormalizeFailurePath()
{
    const QJsonObject payload;

    const ProviderBackend::NormalizedUsageCost normalized =
        ProviderBackend::normalizeUsageCost(ProviderBackend::ProviderId::AzureOpenAI, payload);

    QVERIFY(!normalized.parsed);
    QCOMPARE(normalized.inputTokens, 0);
    QCOMPARE(normalized.outputTokens, 0);
    QCOMPARE(normalized.requestCount, 0);
    QCOMPARE(normalized.cost, 0.0);
    QCOMPARE(normalized.dailyCost, 0.0);
    QCOMPARE(normalized.monthlyCost, 0.0);
}

QTEST_MAIN(ProvidersMockedHttpTest)
#include "test_providers_mocked_http.moc"
