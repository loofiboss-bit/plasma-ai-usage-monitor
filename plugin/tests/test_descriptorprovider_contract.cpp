#include <QtTest>

#include <QHash>
#include <QHostAddress>
#include <QSignalSpy>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include "descriptorprovider.h"
#include "providerpricingcatalog.h"

class ContractHttpServer final : public QObject
{
    Q_OBJECT
public:
    struct Response { int status = 200; QByteArray body = "{}"; int delayMs = 0; };

    explicit ContractHttpServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (m_server.hasPendingConnections()) {
                QTcpSocket *socket = m_server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                    m_buffers[socket] += socket->readAll();
                    if (!m_buffers[socket].contains("\r\n\r\n")) return;
                    const QList<QByteArray> first = m_buffers[socket].split('\n').value(0).trimmed().split(' ');
                    if (first.size() < 2) return;
                    const QString method = QString::fromUtf8(first[0]);
                    const QString target = QString::fromUtf8(first[1]);
                    const QString path = QUrl(target).path();
                    m_methods.append(method);
                    m_targets.append(target);
                    m_requests.append(m_buffers.take(socket));
                    Response response;
                    QList<Response> &sequence = m_responses[path];
                    if (!sequence.isEmpty()) {
                        response = sequence.takeFirst();
                        if (sequence.isEmpty()) m_responses.remove(path);
                    }
                    QByteArray reason = response.status >= 400 ? " Error" : " OK";
                    QByteArray payload = "HTTP/1.1 " + QByteArray::number(response.status) + reason
                        + "\r\nContent-Type: application/json\r\nContent-Length: "
                        + QByteArray::number(response.body.size()) + "\r\nConnection: close\r\n\r\n"
                        + response.body;
                    auto send = [socket, payload]() {
                        if (socket->state() == QAbstractSocket::ConnectedState) {
                            socket->write(payload);
                            socket->disconnectFromHost();
                        }
                    };
                    if (response.delayMs > 0) QTimer::singleShot(response.delayMs, socket, send);
                    else send();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QString baseUrl() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    void respond(const QString &path, int status, const QByteArray &body, int delayMs = 0)
    { m_responses[path].append(Response{status, body, delayMs}); }
    QStringList methods() const { return m_methods; }
    QStringList targets() const { return m_targets; }
    QList<QByteArray> requests() const { return m_requests; }

private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QString, QList<Response>> m_responses;
    QStringList m_methods;
    QStringList m_targets;
    QList<QByteArray> m_requests;
};

class DescriptorProviderContractTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void launchProvidersUseOneReadOnlyRequest_data();
    void launchProvidersUseOneReadOnlyRequest();
    void statusAndSchemaFailuresAreTyped_data();
    void statusAndSchemaFailuresAreTyped();
    void credentialsAreRequiredWithoutNetworkTraffic();
    void modelDiscoveryDeduplicatesAndKeepsFallback();
    void gatewayPreservesMixedCurrenciesAndZero();
    void staleGenerationIsDiscarded();
};

static QVariantMap descriptor(const QString &key)
{
    return ProviderPricingCatalog::instance()->provider(key);
}

void DescriptorProviderContractTest::launchProvidersUseOneReadOnlyRequest_data()
{
    QTest::addColumn<QString>("provider");
    QTest::addColumn<QString>("path");
    QTest::addColumn<QByteArray>("body");
    QTest::newRow("litellm") << "litellm" << "/spend/logs"
        << QByteArray(R"([{"spend":1.5,"prompt_tokens":10,"completion_tokens":2,"currency":"USD"}])");
    QTest::newRow("cerebras") << "cerebras" << "/models" << QByteArray(R"({"data":[{"id":"live-model"}]})");
    QTest::newRow("fireworks") << "fireworks" << "/models" << QByteArray(R"({"models":[{"name":"accounts/a/models/live"}]})");
    QTest::newRow("perplexity") << "perplexity" << "/v1/models" << QByteArray(R"([{"id":"agent-model"}])");
}

void DescriptorProviderContractTest::launchProvidersUseOneReadOnlyRequest()
{
    QFETCH(QString, provider);
    QFETCH(QString, path);
    QFETCH(QByteArray, body);
    ContractHttpServer server;
    QVERIFY(server.listen());
    server.respond(path, 200, body);
    DescriptorProvider backend(descriptor(provider));
    backend.setCustomBaseUrl(server.baseUrl());
    backend.setApiKey(QStringLiteral("contract-secret"));
    QSignalSpy updated(&backend, &ProviderBackend::dataUpdated);
    backend.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 2000);
    QVERIFY(backend.isConnected());
    QCOMPARE(server.methods(), QStringList{QStringLiteral("GET")});
    QCOMPARE(server.targets().size(), 1);
    QVERIFY(!server.targets().first().contains(QStringLiteral("contract-secret")));
    QCOMPARE(server.requests().size(), 1);
    QCOMPARE(server.requests().first().count("contract-secret"), provider == QLatin1String("perplexity") ? 0 : 1);
}

void DescriptorProviderContractTest::statusAndSchemaFailuresAreTyped_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<ProviderBackend::ProviderErrorKind>("kind");
    QTest::newRow("401") << 401 << QByteArray("{}") << ProviderBackend::ProviderErrorKind::Authentication;
    QTest::newRow("403") << 403 << QByteArray("{}") << ProviderBackend::ProviderErrorKind::Permission;
    QTest::newRow("404") << 404 << QByteArray("{}") << ProviderBackend::ProviderErrorKind::Network;
    QTest::newRow("429") << 429 << QByteArray("{}") << ProviderBackend::ProviderErrorKind::RateLimit;
    QTest::newRow("500") << 500 << QByteArray("{}") << ProviderBackend::ProviderErrorKind::Server;
    QTest::newRow("schema") << 200 << QByteArray(R"({"unexpected":true})") << ProviderBackend::ProviderErrorKind::Schema;
}

void DescriptorProviderContractTest::statusAndSchemaFailuresAreTyped()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(ProviderBackend::ProviderErrorKind, kind);
    ContractHttpServer server;
    QVERIFY(server.listen());
    server.respond(QStringLiteral("/models"), status, body);
    DescriptorProvider backend(descriptor(QStringLiteral("cerebras")));
    backend.setCustomBaseUrl(server.baseUrl());
    backend.setApiKey(QStringLiteral("secret"));
    QSignalSpy updated(&backend, &ProviderBackend::dataUpdated);
    backend.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 2000);
    QCOMPARE(backend.errorKind(), kind);
    QVERIFY(!backend.isConnected());
}

void DescriptorProviderContractTest::credentialsAreRequiredWithoutNetworkTraffic()
{
    ContractHttpServer server;
    QVERIFY(server.listen());
    DescriptorProvider backend(descriptor(QStringLiteral("cerebras")));
    backend.setCustomBaseUrl(server.baseUrl());
    backend.refresh();
    QCOMPARE(backend.errorKind(), ProviderBackend::ProviderErrorKind::Configuration);
    QVERIFY(server.methods().isEmpty());
}

void DescriptorProviderContractTest::modelDiscoveryDeduplicatesAndKeepsFallback()
{
    ContractHttpServer server;
    QVERIFY(server.listen());
    server.respond(QStringLiteral("/models"), 200,
                   QByteArray(R"({"data":[{"id":"live"},{"id":"live"},{"id":"llama-4-scout-17b-16e-instruct"}]})"));
    DescriptorProvider backend(descriptor(QStringLiteral("cerebras")));
    backend.setCustomBaseUrl(server.baseUrl());
    backend.setApiKey(QStringLiteral("secret"));
    QSignalSpy updated(&backend, &ProviderBackend::dataUpdated);
    backend.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 2000);
    QSet<QString> ids;
    for (const QVariant &entry : backend.discoveredModels()) ids.insert(entry.toMap().value(QStringLiteral("id")).toString());
    QCOMPARE(ids.size(), backend.discoveredModels().size());
    QVERIFY(ids.contains(QStringLiteral("live")));
    QVERIFY(ids.contains(QStringLiteral("llama-4-scout-17b-16e-instruct")));
}

void DescriptorProviderContractTest::gatewayPreservesMixedCurrenciesAndZero()
{
    ContractHttpServer server;
    QVERIFY(server.listen());
    server.respond(QStringLiteral("/spend/logs"), 200,
                   QByteArray(R"([{"spend":0,"currency":"USD"},{"spend":2,"currency":"EUR"}])"));
    DescriptorProvider backend(descriptor(QStringLiteral("litellm")));
    backend.setCustomBaseUrl(server.baseUrl());
    backend.setApiKey(QStringLiteral("secret"));
    QSignalSpy updated(&backend, &ProviderBackend::dataUpdated);
    backend.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 2000);
    int costRows = 0;
    bool sawAvailableZero = false;
    for (const QVariant &entry : backend.metrics()) {
        const QVariantMap metric = entry.toMap();
        if (metric.value(QStringLiteral("kind")) != QLatin1String("cost")) continue;
        ++costRows;
        if (metric.value(QStringLiteral("currency")) == QLatin1String("USD")
            && metric.value(QStringLiteral("available")).toBool()
            && metric.value(QStringLiteral("value")).toDouble() == 0.0) sawAvailableZero = true;
    }
    QCOMPARE(costRows, 2);
    QVERIFY(sawAvailableZero);
    QCOMPARE(backend.dataQuality(), QStringLiteral("actual_mixed_currency"));
}

void DescriptorProviderContractTest::staleGenerationIsDiscarded()
{
    ContractHttpServer server;
    QVERIFY(server.listen());
    server.respond(QStringLiteral("/models"), 200, QByteArray(R"({"data":[{"id":"stale"}]})"), 250);
    server.respond(QStringLiteral("/models"), 200, QByteArray(R"({"data":[{"id":"fresh"}]})"));
    DescriptorProvider backend(descriptor(QStringLiteral("cerebras")));
    backend.setCustomBaseUrl(server.baseUrl());
    backend.setApiKey(QStringLiteral("secret"));
    backend.refresh();
    QTimer::singleShot(20, &backend, [&backend]() { backend.refresh(); });
    QTRY_VERIFY_WITH_TIMEOUT(backend.isConnected(), 2000);
    const QStringList ids = [&backend]() {
        QStringList result;
        for (const QVariant &entry : backend.discoveredModels()) result << entry.toMap().value(QStringLiteral("id")).toString();
        return result;
    }();
    QVERIFY(ids.contains(QStringLiteral("fresh")));
    QVERIFY(!ids.contains(QStringLiteral("stale")));
}

QTEST_MAIN(DescriptorProviderContractTest)
#include "test_descriptorprovider_contract.moc"
