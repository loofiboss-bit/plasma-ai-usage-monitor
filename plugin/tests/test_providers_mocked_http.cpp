#include <QtTest>

#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

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
#include "scopebreakdownquery.h"
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
                    if (m_processedSockets.contains(socket)) {
                        return;
                    }

                    const QByteArray &buffer = m_buffers[socket];
                    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }

                    const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
                    if (lines.isEmpty()) {
                        socket->disconnectFromHost();
                        return;
                    }

                    qint64 contentLength = 0;
                    for (const QByteArray &line : lines) {
                        const QByteArray trimmed = line.trimmed();
                        constexpr QByteArrayView contentLengthHeader("content-length:");
                        if (trimmed.size() >= contentLengthHeader.size()
                            && QByteArrayView(trimmed).first(contentLengthHeader.size())
                                .compare(contentLengthHeader, Qt::CaseInsensitive) == 0) {
                            bool ok = false;
                            contentLength = trimmed.mid(sizeof("content-length:") - 1).trimmed().toLongLong(&ok);
                            if (!ok || contentLength < 0) {
                                socket->disconnectFromHost();
                                return;
                            }
                            break;
                        }
                    }
                    if (buffer.size() < headerEnd + 4 + contentLength) {
                        return;
                    }
                    m_processedSockets.insert(socket);

                    const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
                    if (firstLine.size() < 2) {
                        socket->disconnectFromHost();
                        return;
                    }

                    const QString method = QString::fromUtf8(firstLine.at(0));
                    const QString rawTarget = QString::fromUtf8(firstLine.at(1));
                    const QString path = QUrl(rawTarget).path();
                    m_targets.append(rawTarget);
                    m_requests.append(buffer.left(headerEnd + 4));

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

                connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                    m_buffers.remove(socket);
                    m_processedSockets.remove(socket);
                    socket->deleteLater();
                });
            }
        });
    }

    ~HttpStubServer() override
    {
        m_server.close();
        const auto sockets = m_server.findChildren<QTcpSocket *>();
        for (QTcpSocket *socket : sockets) {
            disconnect(socket, nullptr, this, nullptr);
            socket->abort();
        }
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

    QStringList targets() const { return m_targets; }
    QList<QByteArray> requests() const { return m_requests; }

private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QSet<QTcpSocket *> m_processedSockets;
    QHash<QString, Response> m_routes;
    QHash<QString, QList<Response>> m_routeSequences;
    QHash<QString, int> m_hitCount;
    QStringList m_targets;
    QList<QByteArray> m_requests;
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
    void openAiMoreThanSevenBucketsAndExplicitLimits();
    void openAiPaginatesAndDeduplicates();
    void openAiRejectsMissingOrRepeatedCursor();
    void openAiPartialPagesNeverReplaceCompleteValue();
    void openAiMalformedIntermediatePageRemainsUnavailable();
    void openAiCancellationBetweenPages();
    void openAiSupersededGenerationCannotPublish();
    void openAiRequestBudgetIsBounded();
    void openAiProviderSupportedScopeAttribution();
    void openAiCalendarBoundaries_data();
    void openAiCalendarBoundaries();
    void anthropicRateLimitHeaders();
    void anthropicAdminUsageCostPaginationAndDimensions();
    void anthropicAdminPartialCostAndPriorityRemainTruthful();
    void anthropicAdminTypedFailures();
    void anthropicAdminCancellationDiscardsOldGeneration();
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

class FixedClockOpenAIProvider : public OpenAIProvider {
public:
    QDateTime now;

protected:
    QDateTime currentDateTimeUtc() const override { return now; }
};

static QByteArray openAiUsagePage(
    const QList<QPair<qint64, qint64>> &startAndInputTokens, bool hasMore = false, const QString &nextPage = QString())
{
    QJsonArray buckets;
    for (const auto &[start, inputTokens] : startAndInputTokens) {
        QJsonObject result {
            { QStringLiteral("input_tokens"), inputTokens },
            { QStringLiteral("output_tokens"), inputTokens / 2 },
            { QStringLiteral("num_model_requests"), 1 },
        };
        buckets.append(QJsonObject {
            { QStringLiteral("start_time"), start },
            { QStringLiteral("end_time"), start + 86400 },
            { QStringLiteral("results"), QJsonArray { result } },
        });
    }
    QJsonObject root {
        { QStringLiteral("data"), buckets },
        { QStringLiteral("has_more"), hasMore },
        { QStringLiteral("next_page"), nextPage.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(nextPage) },
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

static QByteArray openAiCostPage(int bucketCount = 1, double amount = 0.01)
{
    QJsonArray buckets;
    for (int index = 0; index < bucketCount; ++index) {
        const qint64 start = 1767225600 + (index * 86400);
        QJsonObject result {
            { QStringLiteral("amount"),
                QJsonObject {
                    { QStringLiteral("value"), amount },
                    { QStringLiteral("currency"), QStringLiteral("usd") },
                } },
        };
        buckets.append(QJsonObject {
            { QStringLiteral("start_time"), start },
            { QStringLiteral("end_time"), start + 86400 },
            { QStringLiteral("results"), QJsonArray { result } },
        });
    }
    return QJsonDocument(QJsonObject {
                             { QStringLiteral("data"), buckets },
                             { QStringLiteral("has_more"), false },
                             { QStringLiteral("next_page"), QJsonValue(QJsonValue::Null) },
                         })
        .toJson(QJsonDocument::Compact);
}

static void configureOpenAiCostSuccess(HttpStubServer &server, int bucketCount = 1, double amount = 0.01)
{
    server.setResponse(
        QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200, openAiCostPage(bucketCount, amount));
}

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

void ProvidersMockedHttpTest::openAiProviderSupportedScopeAttribution()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    const qint64 start =
        QDateTime(QDate(2026, 7, 28), QTime(0, 0), QTimeZone::UTC)
            .toSecsSinceEpoch();
    const QJsonObject usagePage{
        { QStringLiteral("data"),
          QJsonArray{
              QJsonObject{
                  { QStringLiteral("start_time"), start },
                  { QStringLiteral("end_time"), start + 86400 },
                  { QStringLiteral("results"),
                    QJsonArray{
                        QJsonObject{
                            { QStringLiteral("input_tokens"), 60 },
                            { QStringLiteral("output_tokens"), 30 },
                            { QStringLiteral("num_model_requests"), 6 },
                            { QStringLiteral("model"),
                              QStringLiteral("gpt-a") },
                            { QStringLiteral("project_id"),
                              QStringLiteral("project-a") },
                        },
                        QJsonObject{
                            { QStringLiteral("input_tokens"), 40 },
                            { QStringLiteral("output_tokens"), 20 },
                            { QStringLiteral("num_model_requests"), 4 },
                            { QStringLiteral("model"),
                              QStringLiteral("gpt-b") },
                            { QStringLiteral("project_id"),
                              QStringLiteral("project-b") },
                        },
                    } },
              },
          } },
        { QStringLiteral("has_more"), false },
        { QStringLiteral("next_page"), QJsonValue(QJsonValue::Null) },
    };
    const QJsonObject costPage{
        { QStringLiteral("data"),
          QJsonArray{
              QJsonObject{
                  { QStringLiteral("start_time"), start },
                  { QStringLiteral("end_time"), start + 86400 },
                  { QStringLiteral("results"),
                    QJsonArray{
                        QJsonObject{
                            { QStringLiteral("amount"),
                              QJsonObject{
                                  { QStringLiteral("value"), 0.6 },
                                  { QStringLiteral("currency"),
                                    QStringLiteral("usd") },
                              } },
                            { QStringLiteral("project_id"),
                              QStringLiteral("project-a") },
                            { QStringLiteral("line_item"),
                              QStringLiteral("Responses") },
                        },
                        QJsonObject{
                            { QStringLiteral("amount"),
                              QJsonObject{
                                  { QStringLiteral("value"), 0.4 },
                                  { QStringLiteral("currency"),
                                    QStringLiteral("usd") },
                              } },
                            { QStringLiteral("project_id"),
                              QStringLiteral("project-b") },
                            { QStringLiteral("line_item"),
                              QStringLiteral("Batch") },
                        },
                    } },
              },
          } },
        { QStringLiteral("has_more"), false },
        { QStringLiteral("next_page"), QJsonValue(QJsonValue::Null) },
    };
    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1/organization/usage/completions"), 200,
        QJsonDocument(usagePage).toJson(QJsonDocument::Compact));
    server.setResponse(
        QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200,
        QJsonDocument(costPage).toJson(QJsonDocument::Compact));

    FixedClockOpenAIProvider provider;
    provider.now =
        QDateTime(QDate(2026, 7, 29), QTime(0, 0), QTimeZone::UTC);
    provider.setApiKey(QStringLiteral("admin-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 100);
    QCOMPARE(provider.outputTokens(), 50);
    QCOMPARE(provider.requestCount(), 10);
    QCOMPARE(provider.dailyCost(), 1.0);
    QCOMPARE(provider.monthlyCost(), 1.0);

    bool foundUsageScope = false;
    bool foundCostScope = false;
    for (const QVariant &entry : provider.metrics()) {
        const QVariantMap metric = entry.toMap();
        if (metric.value(QStringLiteral("kind"))
                == QLatin1String("input_tokens")
            && metric.value(QStringLiteral("modelScope"))
                == QLatin1String("gpt-a")
            && metric.value(QStringLiteral("projectScope"))
                == QLatin1String("project-a")) {
            foundUsageScope = true;
            QCOMPARE(metric.value(QStringLiteral("aggregationLevel")).toString(),
                     QStringLiteral("scoped"));
        }
        if (metric.value(QStringLiteral("kind")) == QLatin1String("cost")
            && metric.value(QStringLiteral("projectScope"))
                == QLatin1String("project-a")
            && metric.value(QStringLiteral("scope"))
                   .toString()
                   .endsWith(QLatin1String(":Responses"))) {
            foundCostScope = true;
            QVERIFY(!metric.contains(QStringLiteral("modelScope")));
        }
    }
    QVERIFY(foundUsageScope);
    QVERIFY(foundCostScope);

    const QVariantMap breakdown =
        ScopeBreakdownQuery::run(provider.metrics());
    const QVariantList reconciliations =
        breakdown.value(QStringLiteral("reconciliations")).toList();
    QVERIFY(reconciliations.size() >= 5);
    for (const QVariant &entry : reconciliations) {
        const QVariantMap row = entry.toMap();
        QVERIFY2(row.value(QStringLiteral("reconciled")).toBool(),
                 qPrintable(row.value(QStringLiteral("kind")).toString()));
        QCOMPARE(row.value(QStringLiteral("aggregateRowCount")).toInt(), 1);
    }

    bool usageGroupingVerified = false;
    int costGroupingRequests = 0;
    for (const QString &target : server.targets()) {
        const QUrl url(target);
        const QStringList groups =
            QUrlQuery(url).allQueryItemValues(QStringLiteral("group_by"));
        if (url.path().endsWith(QLatin1String("/usage/completions"))) {
            QVERIFY(groups.contains(QStringLiteral("model")));
            QVERIFY(groups.contains(QStringLiteral("project_id")));
            usageGroupingVerified = true;
        } else if (url.path().endsWith(QLatin1String("/organization/costs"))) {
            QVERIFY(groups.contains(QStringLiteral("project_id")));
            QVERIFY(groups.contains(QStringLiteral("line_item")));
            QVERIFY(!groups.contains(QStringLiteral("model")));
            ++costGroupingRequests;
        }
    }
    QVERIFY(usageGroupingVerified);
    QCOMPARE(costGroupingRequests, 2);

    const QJsonObject replacementUsagePage{
        { QStringLiteral("data"),
          QJsonArray{
              QJsonObject{
                  { QStringLiteral("start_time"), start + 86400 },
                  { QStringLiteral("end_time"), start + 172800 },
                  { QStringLiteral("results"),
                    QJsonArray{
                        QJsonObject{
                            { QStringLiteral("input_tokens"), 25 },
                            { QStringLiteral("output_tokens"), 5 },
                            { QStringLiteral("num_model_requests"), 2 },
                            { QStringLiteral("model"),
                              QStringLiteral("gpt-c") },
                            { QStringLiteral("project_id"),
                              QStringLiteral("project-c") },
                        },
                    } },
              },
          } },
        { QStringLiteral("has_more"), false },
        { QStringLiteral("next_page"), QJsonValue(QJsonValue::Null) },
    };
    const QJsonObject replacementCostPage{
        { QStringLiteral("data"),
          QJsonArray{
              QJsonObject{
                  { QStringLiteral("start_time"), start + 86400 },
                  { QStringLiteral("end_time"), start + 172800 },
                  { QStringLiteral("results"),
                    QJsonArray{
                        QJsonObject{
                            { QStringLiteral("amount"),
                              QJsonObject{
                                  { QStringLiteral("value"), 0.25 },
                                  { QStringLiteral("currency"),
                                    QStringLiteral("usd") },
                              } },
                            { QStringLiteral("project_id"),
                              QStringLiteral("project-c") },
                            { QStringLiteral("line_item"),
                              QStringLiteral("Responses") },
                        },
                    } },
              },
          } },
        { QStringLiteral("has_more"), false },
        { QStringLiteral("next_page"), QJsonValue(QJsonValue::Null) },
    };
    server.setResponse(
        QStringLiteral("GET"),
        QStringLiteral("/v1/organization/usage/completions"), 200,
        QJsonDocument(replacementUsagePage).toJson(QJsonDocument::Compact));
    server.setResponse(
        QStringLiteral("GET"), QStringLiteral("/v1/organization/costs"), 200,
        QJsonDocument(replacementCostPage).toJson(QJsonDocument::Compact));
    provider.now =
        QDateTime(QDate(2026, 7, 30), QTime(0, 0), QTimeZone::UTC);
    const int firstRefreshSignals = dataSpy.count();
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() > firstRefreshSignals, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!provider.isLoading(), 3000);

    int aggregateInputDays = 0;
    int aggregateCostDays = 0;
    int aggregateCostMonths = 0;
    bool foundReplacementScope = false;
    for (const QVariant &entry : provider.metrics()) {
        const QVariantMap metric = entry.toMap();
        QVERIFY(metric.value(QStringLiteral("projectScope")).toString()
                != QLatin1String("project-a"));
        QVERIFY(metric.value(QStringLiteral("projectScope")).toString()
                != QLatin1String("project-b"));
        const QString kind = metric.value(QStringLiteral("kind")).toString();
        const QString window =
            metric.value(QStringLiteral("window")).toString();
        const QString aggregation =
            metric.value(QStringLiteral("aggregationLevel")).toString();
        if (kind == QLatin1String("input_tokens")
            && window == QLatin1String("day")
            && aggregation == QLatin1String("aggregate")) {
            ++aggregateInputDays;
        }
        if (kind == QLatin1String("cost")
            && aggregation == QLatin1String("aggregate")) {
            if (window == QLatin1String("day")) {
                ++aggregateCostDays;
            } else if (window == QLatin1String("month")) {
                ++aggregateCostMonths;
            }
        }
        foundReplacementScope =
            foundReplacementScope
            || metric.value(QStringLiteral("projectScope")).toString()
                == QLatin1String("project-c");
    }
    QVERIFY(foundReplacementScope);
    QCOMPARE(aggregateInputDays, 1);
    QCOMPARE(aggregateCostDays, 1);
    QCOMPARE(aggregateCostMonths, 1);
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

void ProvidersMockedHttpTest::openAiMoreThanSevenBucketsAndExplicitLimits()
{
    HttpStubServer server;
    QVERIFY(server.listen());

    QList<QPair<qint64, qint64>> usageBuckets;
    for (int index = 0; index < 8; ++index) {
        usageBuckets.append({ 1767225600 + (index * 86400), index + 1 });
    }
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200,
        openAiUsagePage(usageBuckets));
    configureOpenAiCostSuccess(server, 8, 0.25);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 36);
    QCOMPARE(provider.requestCount(), 8);
    QCOMPARE(provider.dailyCost(), 2.0);
    QCOMPARE(provider.monthlyCost(), 2.0);

    bool usageLimitSeen = false;
    int costLimitCount = 0;
    for (const QString &target : server.targets()) {
        const QUrl url(target);
        const QUrlQuery query(url);
        if (url.path().endsWith(QStringLiteral("/organization/usage/completions"))) {
            usageLimitSeen = query.queryItemValue(QStringLiteral("limit")) == QStringLiteral("31");
        } else if (url.path().endsWith(QStringLiteral("/organization/costs"))
            && query.queryItemValue(QStringLiteral("limit")) == QStringLiteral("180")) {
            ++costLimitCount;
        }
    }
    QVERIFY(usageLimitSeen);
    QCOMPARE(costLimitCount, 2);
}

void ProvidersMockedHttpTest::openAiPaginatesAndDeduplicates()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    const auto first = openAiUsagePage({ { 1767225600, 10 } }, true, QStringLiteral("page-2"));
    const auto duplicate = openAiUsagePage({ { 1767225600, 10 } }, true, QStringLiteral("page-3"));
    const auto boundaryDuplicate = openAiUsagePage({ { 1767225600, 10 }, { 1767312000, 20 } }, false);
    server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
        {
            { 200, first, { }, 0 },
            { 200, duplicate, { }, 0 },
            { 200, boundaryDuplicate, { }, 0 },
        });
    configureOpenAiCostSuccess(server);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 30);
    QCOMPARE(provider.outputTokens(), 15);
    QCOMPARE(provider.requestCount(), 2);
    QCOMPARE(server.hitCount(QStringLiteral("/v1/organization/usage/completions")), 3);
    QVERIFY(server.targets().filter(QRegularExpression(QStringLiteral("[?&]page=page-2(?:&|$)"))).size() == 1);
    QVERIFY(server.targets().filter(QRegularExpression(QStringLiteral("[?&]page=page-3(?:&|$)"))).size() == 1);
}

void ProvidersMockedHttpTest::openAiRejectsMissingOrRepeatedCursor()
{
    {
        HttpStubServer server;
        QVERIFY(server.listen());
        const QJsonObject missingCursor {
            { QStringLiteral("data"),
                QJsonDocument::fromJson(openAiUsagePage({ { 1767225600, 10 } }))
                    .object()
                    .value(QStringLiteral("data")) },
            { QStringLiteral("has_more"), true },
        };
        server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200,
            QJsonDocument(missingCursor).toJson(QJsonDocument::Compact));
        configureOpenAiCostSuccess(server);

        OpenAIProvider provider;
        provider.setApiKey(QStringLiteral("test-key"));
        provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
        QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
        provider.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
        QCOMPARE(provider.inputTokens(), 0);
        QCOMPARE(provider.capabilityStatus()
                     .value(QStringLiteral("usage"))
                     .toMap()
                     .value(QStringLiteral("status"))
                     .toString(),
            QStringLiteral("partial"));
    }

    {
        HttpStubServer server;
        QVERIFY(server.listen());
        server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
            {
                { 200, openAiUsagePage({ { 1767225600, 10 } }, true, QStringLiteral("same-cursor")), { }, 0 },
                { 200, openAiUsagePage({ { 1767312000, 20 } }, true, QStringLiteral("same-cursor")), { }, 0 },
            });
        configureOpenAiCostSuccess(server);

        OpenAIProvider provider;
        provider.setApiKey(QStringLiteral("test-key"));
        provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
        QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
        provider.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
        QCOMPARE(provider.inputTokens(), 0);
        QCOMPARE(provider.capabilityStatus()
                     .value(QStringLiteral("usage"))
                     .toMap()
                     .value(QStringLiteral("status"))
                     .toString(),
            QStringLiteral("partial"));
        QCOMPARE(server.hitCount(QStringLiteral("/v1/organization/usage/completions")), 2);
    }
}

void ProvidersMockedHttpTest::openAiPartialPagesNeverReplaceCompleteValue()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
        {
            { 200, openAiUsagePage({ { 1767225600, 10 } }), { }, 0 },
            { 200, openAiUsagePage({ { 1767312000, 50 } }, true, QStringLiteral("terminal")), { }, 0 },
            { 403, QByteArrayLiteral(R"JSON({"error":"forbidden"})JSON"), { }, 0 },
        });
    configureOpenAiCostSuccess(server);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QCOMPARE(provider.inputTokens(), 10);

    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 2, 3000);
    QCOMPARE(provider.inputTokens(), 10);
    QCOMPARE(
        provider.capabilityStatus().value(QStringLiteral("usage")).toMap().value(QStringLiteral("status")).toString(),
        QStringLiteral("stale"));
}

void ProvidersMockedHttpTest::openAiMalformedIntermediatePageRemainsUnavailable()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
        {
            { 200, openAiUsagePage({ { 1767225600, 50 } }, true, QStringLiteral("malformed")), { }, 0 },
            { 200, QByteArrayLiteral("{"), { }, 0 },
        });
    configureOpenAiCostSuccess(server);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(provider.inputTokens(), 0);
    QCOMPARE(
        provider.capabilityStatus().value(QStringLiteral("usage")).toMap().value(QStringLiteral("status")).toString(),
        QStringLiteral("partial"));
    QCOMPARE(provider.errorKind(), ProviderBackend::ProviderErrorKind::Schema);
}

void ProvidersMockedHttpTest::openAiCancellationBetweenPages()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
        {
            { 200, openAiUsagePage({ { 1767225600, 50 } }, true, QStringLiteral("slow-page")), { }, 0 },
            { 200, openAiUsagePage({ { 1767312000, 70 } }), { }, 1000 },
        });
    configureOpenAiCostSuccess(server);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    provider.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(server.hitCount(QStringLiteral("/v1/organization/usage/completions")), 2, 3000);
    provider.cancelRefresh();
    QVERIFY(!provider.isLoading());
    QTest::qWait(1100);
    QCOMPARE(provider.inputTokens(), 0);
}

void ProvidersMockedHttpTest::openAiSupersededGenerationCannotPublish()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
        {
            { 200, openAiUsagePage({ { 1767225600, 100 } }), { }, 700 },
            { 200, openAiUsagePage({ { 1767225600, 7 } }), { }, 0 },
        });
    configureOpenAiCostSuccess(server);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(server.hitCount(QStringLiteral("/v1/organization/usage/completions")), 1, 3000);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QCOMPARE(provider.inputTokens(), 7);
    QTest::qWait(800);
    QCOMPARE(provider.inputTokens(), 7);
}

void ProvidersMockedHttpTest::openAiRequestBudgetIsBounded()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponseSequence(QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"),
        {
            { 200, openAiUsagePage({ { 1767225600, 1 } }, true, QStringLiteral("page-2")), { }, 0 },
            { 200, openAiUsagePage({ { 1767312000, 1 } }, true, QStringLiteral("page-3")), { }, 0 },
            { 200, openAiUsagePage({ { 1767398400, 1 } }, true, QStringLiteral("page-4")), { }, 0 },
            { 200, openAiUsagePage({ { 1767484800, 1 } }, true, QStringLiteral("page-5")), { }, 0 },
        });
    configureOpenAiCostSuccess(server);

    OpenAIProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    QCOMPARE(server.hitCount(QStringLiteral("/v1/organization/usage/completions")), 4);
    QCOMPARE(provider.inputTokens(), 0);
    QCOMPARE(
        provider.capabilityStatus().value(QStringLiteral("usage")).toMap().value(QStringLiteral("status")).toString(),
        QStringLiteral("partial"));
}

void ProvidersMockedHttpTest::openAiCalendarBoundaries_data()
{
    QTest::addColumn<QDate>("date");
    QTest::newRow("28-day-February") << QDate(2025, 2, 28);
    QTest::newRow("29-day-February") << QDate(2024, 2, 29);
    QTest::newRow("30-day-April") << QDate(2026, 4, 30);
    QTest::newRow("31-day-July") << QDate(2026, 7, 31);
}

void ProvidersMockedHttpTest::openAiCalendarBoundaries()
{
    QFETCH(QDate, date);
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponse(
        QStringLiteral("GET"), QStringLiteral("/v1/organization/usage/completions"), 200, openAiUsagePage({ }));
    configureOpenAiCostSuccess(server);

    FixedClockOpenAIProvider provider;
    provider.now = QDateTime(date, QTime(12, 34, 56), QTimeZone::UTC);
    provider.setApiKey(QStringLiteral("test-key"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

    const qint64 expectedEnd = provider.now.toSecsSinceEpoch();
    const qint64 expectedMonthStart = QDate(date.year(), date.month(), 1).startOfDay(QTimeZone::UTC).toSecsSinceEpoch();
    bool usageRangeSeen = false;
    bool monthlyRangeSeen = false;
    for (const QString &target : server.targets()) {
        const QUrl url(target);
        const QUrlQuery query(url);
        const qint64 start = query.queryItemValue(QStringLiteral("start_time")).toLongLong();
        const qint64 end = query.queryItemValue(QStringLiteral("end_time")).toLongLong();
        QCOMPARE(end, expectedEnd);
        if (url.path().endsWith(QStringLiteral("/organization/usage/completions"))) {
            QCOMPARE(start, provider.now.addDays(-1).toSecsSinceEpoch());
            QCOMPARE(query.queryItemValue(QStringLiteral("limit")), QStringLiteral("31"));
            usageRangeSeen = true;
        } else if (start == expectedMonthStart) {
            QCOMPARE(query.queryItemValue(QStringLiteral("limit")), QStringLiteral("180"));
            monthlyRangeSeen = true;
        }
    }
    QVERIFY(usageRangeSeen);
    QVERIFY(monthlyRangeSeen);
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

void ProvidersMockedHttpTest::anthropicAdminUsageCostPaginationAndDimensions()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("GET"), QStringLiteral("/v1/models"), 200,
                       QByteArrayLiteral(R"JSON({"data":[]})JSON"));
    server.setResponseSequence(
        QStringLiteral("GET"), QStringLiteral("/v1/organizations/usage_report/messages"),
        {
            {200, R"JSON({"data":[{"starting_at":"2026-07-25T00:00:00Z","ending_at":"2026-07-26T00:00:00Z","results":[{"uncached_input_tokens":100,"cache_read_input_tokens":20,"cache_creation":{"ephemeral_5m_input_tokens":30},"output_tokens":40,"workspace_id":"workspace-a","model":"claude-sonnet-4-20250514","service_tier":"standard"}]}],"has_more":true,"next_page":"usage-next"})JSON", {}, 0},
            {200, R"JSON({"data":[{"starting_at":"2026-07-26T00:00:00Z","ending_at":"2026-07-27T00:00:00Z","results":[{"uncached_input_tokens":10,"cache_read_input_tokens":2,"cache_creation":{"ephemeral_1h_input_tokens":3},"output_tokens":4,"workspace_id":"workspace-a","model":"claude-sonnet-4-20250514","service_tier":"standard"}]}],"has_more":false,"next_page":null})JSON", {}, 0},
        });
    server.setResponse(
        QStringLiteral("GET"), QStringLiteral("/v1/organizations/cost_report"), 200,
        R"JSON({"data":[{"starting_at":"2026-07-26T00:00:00Z","ending_at":"2026-07-27T00:00:00Z","results":[{"amount":"123.45","currency":"USD","workspace_id":"workspace-a","model":"claude-sonnet-4-20250514","service_tier":"standard"}]}],"has_more":false,"next_page":null})JSON");

    AnthropicProvider provider;
    provider.setApiKey(QStringLiteral("standard-secret"));
    provider.setAdminApiKey(QStringLiteral("admin-secret"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!provider.isLoading(), 3000);

    QCOMPARE(provider.inputTokens(), 165);
    QCOMPARE(provider.outputTokens(), 44);
    QCOMPARE(provider.cost(), 1.2345);
    QCOMPARE(provider.dataQuality(), QStringLiteral("actual"));
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("usage")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("cost")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    const QVariantMap cacheRead = provider.metric(
        QStringLiteral("cache_read_input_tokens"),
        QStringLiteral("organization_scoped:service_tier:standard"),
        QStringLiteral("day"));
    QVERIFY(cacheRead.value(QStringLiteral("available")).toBool());
    QCOMPARE(cacheRead.value(QStringLiteral("modelScope")).toString(),
             QStringLiteral("claude-sonnet-4-20250514"));
    QCOMPARE(cacheRead.value(QStringLiteral("projectScope")).toString(),
             QStringLiteral("workspace-a"));
    QCOMPARE(server.hitCount(QStringLiteral("/v1/organizations/usage_report/messages")), 2);
    QVERIFY(server.targets().filter(QRegularExpression(
                QStringLiteral("[?&]page=usage-next(?:&|$)"))).size() == 1);
    QVERIFY(server.targets().filter(QRegularExpression(
                QStringLiteral("[?&]limit=31(?:&|$)"))).size() >= 2);
    for (const QString &target : server.targets()) {
        QVERIFY(!target.contains(QStringLiteral("standard-secret")));
        QVERIFY(!target.contains(QStringLiteral("admin-secret")));
    }
    bool modelsUsedStandard = false;
    bool reportsUsedAdmin = false;
    for (const QByteArray &request : server.requests()) {
        if (request.startsWith("GET /v1/models "))
            modelsUsedStandard = request.contains("standard-secret")
                && !request.contains("admin-secret");
        if (request.startsWith("GET /v1/organizations/usage_report/messages"))
            reportsUsedAdmin = request.contains("admin-secret")
                && !request.contains("standard-secret");
    }
    QVERIFY(modelsUsedStandard);
    QVERIFY(reportsUsedAdmin);
}

void ProvidersMockedHttpTest::anthropicAdminPartialCostAndPriorityRemainTruthful()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    const QByteArray priorityUsageBody =
        R"JSON({"data":[{"starting_at":"2026-07-26T00:00:00Z","ending_at":"2026-07-27T00:00:00Z","results":[{"uncached_input_tokens":0,"cache_read_input_tokens":0,"cache_creation":{},"output_tokens":0,"workspace_id":"workspace-priority","model":"claude-opus-4-8","service_tier":"priority"}]}],"has_more":false,"next_page":null})JSON";
    server.setResponseSequence(
        QStringLiteral("GET"), QStringLiteral("/v1/organizations/usage_report/messages"),
        {{200, priorityUsageBody, {}, 0}, {200, priorityUsageBody, {}, 0}});
    server.setResponseSequence(
        QStringLiteral("GET"), QStringLiteral("/v1/organizations/cost_report"),
        {
            {200, R"JSON({"data":[{"starting_at":"2026-07-26T00:00:00Z","ending_at":"2026-07-27T00:00:00Z","results":[{"amount":"100","currency":"USD","workspace_id":"workspace-priority","model":"claude-opus-4-8","service_tier":"standard"}]}],"has_more":false,"next_page":null})JSON", {}, 0},
            {403, R"JSON({"error":"forbidden"})JSON", {}, 0},
        });

    AnthropicProvider provider;
    provider.setAdminApiKey(QStringLiteral("admin-secret"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QCOMPARE(provider.cost(), 1.0);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 2, 3000);

    QVERIFY(provider.isConnected());
    QCOMPARE(provider.inputTokens(), 0);
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("usage")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    QCOMPARE(provider.capabilityStatus().value(QStringLiteral("cost")).toMap()
                 .value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
    QCOMPARE(provider.cost(), 1.0);
    QCOMPARE(provider.metric(QStringLiteral("cost"), QStringLiteral("api_key"),
                             QStringLiteral("current"))
                 .value(QStringLiteral("quality")).toString(), QStringLiteral("stale"));
    const QVariantMap priorityUsage = provider.metric(
        QStringLiteral("input_tokens"),
        QStringLiteral("organization_scoped:service_tier:priority"),
        QStringLiteral("day"));
    QVERIFY(priorityUsage.value(QStringLiteral("available")).toBool());
    QCOMPARE(priorityUsage.value(QStringLiteral("value")).toLongLong(), 0);
    QVERIFY(!provider.metric(QStringLiteral("cost"),
                             QStringLiteral(
                                 "organization_scoped:service_tier:priority"),
                             QStringLiteral("day"))
                 .value(QStringLiteral("available")).toBool());
}

void ProvidersMockedHttpTest::anthropicAdminTypedFailures()
{
    const auto verifyFailure = [](int status, ProviderBackend::ProviderErrorKind expected) {
        HttpStubServer server;
        QVERIFY(server.listen());
        const QList<QPair<QByteArray, QByteArray>> headers = status == 429
            ? QList<QPair<QByteArray, QByteArray>>{{"retry-after", "120"}}
            : QList<QPair<QByteArray, QByteArray>>{};
        server.setResponse(QStringLiteral("GET"),
                           QStringLiteral("/v1/organizations/usage_report/messages"),
                           status, QByteArrayLiteral(R"JSON({"error":"failure"})JSON"), headers);
        server.setResponse(QStringLiteral("GET"),
                           QStringLiteral("/v1/organizations/cost_report"),
                           status, QByteArrayLiteral(R"JSON({"error":"failure"})JSON"), headers);
        AnthropicProvider provider;
        provider.setAdminApiKey(QStringLiteral("admin-secret"));
        provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
        QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
        provider.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
        QCOMPARE(provider.errorKind(), expected);
        QVERIFY(!provider.isConnected());
        if (status == 429) QVERIFY(provider.retryAfter().isValid());
    };
    verifyFailure(401, ProviderBackend::ProviderErrorKind::Authentication);
    verifyFailure(403, ProviderBackend::ProviderErrorKind::Permission);
    verifyFailure(429, ProviderBackend::ProviderErrorKind::RateLimit);

    HttpStubServer schemaServer;
    QVERIFY(schemaServer.listen());
    const QByteArray invalid = R"JSON({"data":"wrong","has_more":false})JSON";
    schemaServer.setResponse(QStringLiteral("GET"),
        QStringLiteral("/v1/organizations/usage_report/messages"), 200, invalid);
    schemaServer.setResponse(QStringLiteral("GET"),
        QStringLiteral("/v1/organizations/cost_report"), 200, invalid);
    AnthropicProvider schemaProvider;
    schemaProvider.setAdminApiKey(QStringLiteral("admin-secret"));
    schemaProvider.setCustomBaseUrl(schemaServer.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy schemaSpy(&schemaProvider, &ProviderBackend::dataUpdated);
    schemaProvider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(schemaSpy.count() >= 1, 3000);
    QCOMPARE(schemaProvider.errorKind(), ProviderBackend::ProviderErrorKind::Schema);

    HttpStubServer emptyServer;
    QVERIFY(emptyServer.listen());
    const QByteArray empty = R"JSON({"data":[],"has_more":false,"next_page":null})JSON";
    emptyServer.setResponse(QStringLiteral("GET"),
        QStringLiteral("/v1/organizations/usage_report/messages"), 200, empty);
    emptyServer.setResponse(QStringLiteral("GET"),
        QStringLiteral("/v1/organizations/cost_report"), 200, empty);
    AnthropicProvider emptyProvider;
    emptyProvider.setAdminApiKey(QStringLiteral("admin-secret"));
    emptyProvider.setCustomBaseUrl(emptyServer.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy emptySpy(&emptyProvider, &ProviderBackend::dataUpdated);
    emptyProvider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() >= 1, 3000);
    QVERIFY(emptyProvider.isConnected());
    QVERIFY(emptyProvider.metric(QStringLiteral("input_tokens"),
                                 QStringLiteral("organization"),
                                 QStringLiteral("current"))
                .value(QStringLiteral("available")).toBool());
    QVERIFY(emptyProvider.metric(QStringLiteral("cost"), QStringLiteral("api_key"),
                                 QStringLiteral("current"))
                .value(QStringLiteral("available")).toBool());
}

void ProvidersMockedHttpTest::anthropicAdminCancellationDiscardsOldGeneration()
{
    HttpStubServer server;
    QVERIFY(server.listen());
    const auto usageBody = [](int input) {
        return QStringLiteral(R"JSON({"data":[{"starting_at":"2026-07-26T00:00:00Z","ending_at":"2026-07-27T00:00:00Z","results":[{"uncached_input_tokens":%1,"cache_read_input_tokens":0,"cache_creation":{},"output_tokens":1,"workspace_id":"workspace-a","model":"claude-sonnet-4-20250514","service_tier":"standard"}]}],"has_more":false,"next_page":null})JSON").arg(input).toUtf8();
    };
    server.setResponseSequence(QStringLiteral("GET"),
        QStringLiteral("/v1/organizations/usage_report/messages"),
        {{200, usageBody(100), {}, 300}, {200, usageBody(5), {}, 0}});
    const QByteArray empty = R"JSON({"data":[],"has_more":false,"next_page":null})JSON";
    server.setResponseSequence(QStringLiteral("GET"),
        QStringLiteral("/v1/organizations/cost_report"),
        {{200, empty, {}, 300}, {200, empty, {}, 0}});
    AnthropicProvider provider;
    provider.setAdminApiKey(QStringLiteral("admin-secret"));
    provider.setCustomBaseUrl(server.baseUrl() + QStringLiteral("/v1"));
    QSignalSpy dataSpy(&provider, &ProviderBackend::dataUpdated);
    provider.refresh();
    QTest::qWait(30);
    provider.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);
    QCOMPARE(provider.inputTokens(), 5);
    QCOMPARE(provider.outputTokens(), 1);
    QVERIFY(provider.cancellationCount() >= 1);
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
    QTRY_VERIFY_WITH_TIMEOUT(!provider.isLoading(), 3000);
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
