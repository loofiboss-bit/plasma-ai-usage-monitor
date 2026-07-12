#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "awssigv4signer.h"
#include "localmetricsserver.h"
#include "usagedatabase.h"
#include "webhooknotifier.h"

class LocalIntegrationsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void metricsServerResponds();
    void usageDatabaseExportsFiles();
    void webhookNotifierRejectsInsecureEndpoints();
    void awsSigV4SignerShapesHeaders();
};

void LocalIntegrationsTest::metricsServerResponds()
{
    LocalMetricsServer server;
    server.setPayload(QStringLiteral("test_metric 1\n"));
    server.setPort(19464);
    server.setEnabled(true);
    QVERIFY(server.isListening());

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, 19464);
    QVERIFY(socket.waitForConnected());
    socket.write("GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n");
    QVERIFY(socket.waitForBytesWritten());
    QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0, 3000);

    const QByteArray response = socket.readAll();
    QVERIFY(response.contains("HTTP/1.1 200 OK"));
    QVERIFY(response.contains("Content-Type: text/plain; version=0.0.4"));
    QVERIFY(response.contains("test_metric 1"));

    auto request = [](const QByteArray &payload) {
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, 19464);
        if (!client.waitForConnected()) return QByteArray();
        client.write(payload);
        client.waitForBytesWritten();
        for (int i = 0; i < 30 && client.bytesAvailable() == 0; ++i) {
            QTest::qWait(10);
        }
        return client.readAll();
    };
    const QByteArray head = request("HEAD /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n");
    QVERIFY(head.contains("HTTP/1.1 200 OK"));
    QVERIFY(!head.contains("test_metric 1"));
    QVERIFY(request("GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n").contains("404 Not Found"));
    QVERIFY(request("POST /metrics HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n").contains("405 Method Not Allowed"));
}

void LocalIntegrationsTest::usageDatabaseExportsFiles()
{
    UsageDatabase db;
    db.init();
    db.recordSnapshot(QStringLiteral("OpenAI"), 10, 5, 1, 0.25, 0.25, 0.25, 100, 99, 1000, 985, QStringLiteral("gpt-5.4-pro"), false);
    db.recordToolSnapshot(QStringLiteral("Cursor"), 3, 500, QStringLiteral("Monthly"), QStringLiteral("Pro"), false);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QStringList files = db.exportAllToDirectory(dir.path(), {QStringLiteral("json"), QStringLiteral("csv")});
    QCOMPARE(files.size(), 3);
    for (const QString &path : files) {
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(QFileInfo(path).size() > 0);
    }
}

void LocalIntegrationsTest::webhookNotifierRejectsInsecureEndpoints()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const quint16 port = server.serverPort();

    WebhookNotifier notifier;
    notifier.setSlackEnabled(true);
    notifier.setDiscordEnabled(true);
    notifier.setSlackWebhookUrl(QStringLiteral("http://127.0.0.1:%1/slack").arg(port));
    notifier.setDiscordWebhookUrl(QStringLiteral("http://127.0.0.1:%1/discord").arg(port));
    notifier.setCooldownMinutes(1);
    QSignalSpy failureSpy(&notifier, &WebhookNotifier::deliveryFailed);
    notifier.sendAlert(QStringLiteral("budget"), QStringLiteral("Budget warning"), QStringLiteral("Threshold reached"), true);
    QCOMPARE(failureSpy.count(), 2);
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(failureSpy.first().at(1).toString().contains(QStringLiteral("HTTPS")));
}

void LocalIntegrationsTest::awsSigV4SignerShapesHeaders()
{
    const auto signedHeaders = AwsSigV4Signer::sign(
        QStringLiteral("AKIDEXAMPLE"),
        QStringLiteral("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"),
        QString(),
        QStringLiteral("us-east-1"),
        QStringLiteral("bedrock"),
        QStringLiteral("GET"),
        QStringLiteral("/foundation-models"),
        QStringLiteral("byOutputModality=TEXT"),
        {
            {QByteArrayLiteral("accept"), QByteArrayLiteral("application/json")},
            {QByteArrayLiteral("host"), QByteArrayLiteral("bedrock.us-east-1.amazonaws.com")}
        },
        QByteArray(),
        QDateTime(QDate(2026, 4, 10), QTime(12, 0), QTimeZone::utc()));

    QVERIFY(signedHeaders.authorizationHeader.startsWith("AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20260410/us-east-1/bedrock/aws4_request"));
    QVERIFY(signedHeaders.authorizationHeader.contains("SignedHeaders=accept;host;x-amz-content-sha256;x-amz-date"));
    QCOMPARE(signedHeaders.amzDate, QByteArray("20260410T120000Z"));
    QCOMPARE(signedHeaders.payloadHash.size(), 64);
}

QTEST_MAIN(LocalIntegrationsTest)
#include "test_local_integrations.moc"
