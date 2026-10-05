#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "awssigv4signer.h"
#include "localmetricsserver.h"
#include "usagedatabase.h"
#include "webhooknotifier.h"

class TestWebhookReply : public QNetworkReply {
public:
  TestWebhookReply(int status, QObject *parent) : QNetworkReply(parent) {
    open(QIODevice::ReadOnly);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
    if (status == 0)
      setError(QNetworkReply::RemoteHostClosedError,
               QStringLiteral("connection closed"));
    if (status < 0)
      setError(QNetworkReply::TimeoutError, QStringLiteral("request timed out"));
    if (status == 429)
      setRawHeader("Retry-After", "1200");
    QTimer::singleShot(0, this, [this]() {
      setFinished(true);
      Q_EMIT finished();
    });
  }
  void abort() override {}

protected:
  qint64 readData(char *, qint64) override { return -1; }
};
class TestWebhookManager : public QNetworkAccessManager {
public:
  QList<int> responses{429, 503, 204, 400, 0};
  QList<QByteArray> payloads;

protected:
  QNetworkReply *createRequest(Operation, const QNetworkRequest &,
                               QIODevice *data) override {
    payloads.append(data->readAll());
    return new TestWebhookReply(responses.takeFirst(), this);
  }
};

class LocalIntegrationsTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void webhookPolicyResultsAreCorrelatedAndBounded();
  void metricsServerResponds();
  void metricsServerStaysDisabledUntilEnabled();
  void metricsServerBindingIsExplicit();
  void metricsServerRecoversAfterBindFailure();
  void usageDatabaseExportsFiles();
  void webhookNotifierRejectsInsecureEndpoints();
  void webhookNotifierSanitizesGuardrailPayload();
  void webhookNotifierUsesStrictBudgetPolicyAllowlist();
  void webhookRuntimeResultsAreTypedAndRedacted();
  void awsSigV4SignerShapesHeaders();
};

void LocalIntegrationsTest::webhookPolicyResultsAreCorrelatedAndBounded() {
  TestWebhookManager network;
  WebhookNotifier notifier(nullptr, &network);
  notifier.setSlackEnabled(true);
  notifier.setSlackWebhookUrl(
      QStringLiteral("https://hooks.example.test/secret"));
  QSignalSpy result(&notifier, &WebhookNotifier::policyChannelResult);
  const QVariantMap event{
      {QStringLiteral("type"), QStringLiteral("guardrail")},
      {QStringLiteral("contractVersion"), QStringLiteral("budget-pacing-v2")},
      {QStringLiteral("eventKey"),
       QStringLiteral("budget-policy-transition-warning")},
      {QStringLiteral("transition"), QStringLiteral("warning")},
      {QStringLiteral("risk"), QStringLiteral("warning")},
      {QStringLiteral("percentClass"), QStringLiteral("warning")},
      {QStringLiteral("period"), QStringLiteral("calendar_month")},
      {QStringLiteral("providerDisplayName"), QStringLiteral("OpenAI")},
      {QStringLiteral("linkText"), QStringLiteral("Open Budget Control")},
      {QStringLiteral("policyId"), QStringLiteral("secret-internal-policy")}};
  notifier.sendPolicyChannel(11, QStringLiteral("slack"), event);
  notifier.sendPolicyChannel(12, QStringLiteral("slack"), event);
  notifier.sendPolicyChannel(13, QStringLiteral("slack"), event);
  notifier.sendPolicyChannel(14, QStringLiteral("slack"), event);
  notifier.sendPolicyChannel(15, QStringLiteral("slack"), event);
  QTRY_COMPARE(result.count(), 5);
  QCOMPARE(result.at(0).at(0).toLongLong(), 11);
  QVERIFY(!result.at(0).at(2).toBool());
  QVERIFY(result.at(0).at(3).toBool());
  QCOMPARE(result.at(0).at(5).toInt(), 1200);
  QVERIFY(result.at(1).at(3).toBool());
  QVERIFY(result.at(2).at(2).toBool());
  QVERIFY(!result.at(3).at(2).toBool());
  QVERIFY(!result.at(3).at(3).toBool());
  QVERIFY(result.at(4).at(3).toBool());
  for (const auto &payload : network.payloads) {
    QVERIFY(!payload.contains("secret-internal-policy"));
    QVERIFY(!payload.contains("eventId"));
  }
}

void LocalIntegrationsTest::metricsServerResponds() {
  LocalMetricsServer server;
  server.setPayload(QStringLiteral("stale_metric 1\n"));
  QSignalSpy payloadSpy(&server, &LocalMetricsServer::payloadRequested);
  connect(&server, &LocalMetricsServer::payloadRequested, &server, [&server]() {
    server.setPayload(QStringLiteral("fresh_metric 1\n"));
  });
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
  QVERIFY(response.contains("fresh_metric 1"));
  QVERIFY(!response.contains("stale_metric 1"));
  QCOMPARE(payloadSpy.count(), 1);

  auto request = [](const QByteArray &payload) {
    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, 19464);
    if (!client.waitForConnected())
      return QByteArray();
    client.write(payload);
    client.waitForBytesWritten();
    for (int i = 0; i < 30 && client.bytesAvailable() == 0; ++i) {
      QTest::qWait(10);
    }
    return client.readAll();
  };
  const QByteArray head =
      request("HEAD /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n");
  QVERIFY(head.contains("HTTP/1.1 200 OK"));
  QVERIFY(!head.contains("fresh_metric 1"));
  QVERIFY(request("GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n")
              .contains("404 Not Found"));
  QVERIFY(request("POST /metrics HTTP/1.1\r\nHost: "
                  "localhost\r\nContent-Length: 0\r\n\r\n")
              .contains("405 Method Not Allowed"));
}

void LocalIntegrationsTest::metricsServerStaysDisabledUntilEnabled() {
  QTcpServer portProbe;
  QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
  const quint16 port = portProbe.serverPort();
  portProbe.close();

  LocalMetricsServer server;
  QSignalSpy listeningSpy(&server, &LocalMetricsServer::listeningChanged);
  QVERIFY(!server.isEnabled());
  QVERIFY(!server.isListening());
  QVERIFY(!server.listenOnAllInterfaces());

  server.setPort(port);
  server.setListenOnAllInterfaces(true);
  QVERIFY(!server.isListening());
  QCOMPARE(listeningSpy.count(), 0);

  server.setEnabled(true);
  QVERIFY(server.isListening());
  QCOMPARE(server.listeningAddress(),
           QHostAddress(QHostAddress::AnyIPv4).toString());
  QCOMPARE(listeningSpy.count(), 1);

  server.setEnabled(false);
  QVERIFY(!server.isListening());
  QVERIFY(server.listeningAddress().isEmpty());
  QCOMPARE(listeningSpy.count(), 2);
}

void LocalIntegrationsTest::metricsServerBindingIsExplicit() {
  QTcpServer portProbe;
  QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
  const quint16 port = portProbe.serverPort();
  portProbe.close();

  LocalMetricsServer server;
  server.setPort(port);
  server.setEnabled(true);
  QVERIFY(server.isListening());
  QVERIFY(!server.listenOnAllInterfaces());
  QCOMPARE(server.listeningAddress(),
           QHostAddress(QHostAddress::LocalHost).toString());

  QSignalSpy bindingSpy(&server,
                        &LocalMetricsServer::listenOnAllInterfacesChanged);
  server.setListenOnAllInterfaces(true);
  QCOMPARE(bindingSpy.count(), 1);
  QVERIFY(server.isListening());
  QVERIFY(server.listenOnAllInterfaces());
  QCOMPARE(server.listeningAddress(),
           QHostAddress(QHostAddress::AnyIPv4).toString());

  server.setListenOnAllInterfaces(false);
  QCOMPARE(bindingSpy.count(), 2);
  QVERIFY(server.isListening());
  QCOMPARE(server.listeningAddress(),
           QHostAddress(QHostAddress::LocalHost).toString());
}

void LocalIntegrationsTest::metricsServerRecoversAfterBindFailure() {
  QTcpServer firstPortProbe;
  QVERIFY(firstPortProbe.listen(QHostAddress::LocalHost, 0));
  const quint16 firstPort = firstPortProbe.serverPort();
  firstPortProbe.close();

  QTcpServer blocker;
  QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));

  LocalMetricsServer server;
  server.setPort(firstPort);
  QSignalSpy listeningSpy(&server, &LocalMetricsServer::listeningChanged);
  QSignalSpy errorSpy(&server, &LocalMetricsServer::error);
  server.setEnabled(true);
  QVERIFY(server.isListening());
  QCOMPARE(listeningSpy.count(), 1);

  server.setPort(blocker.serverPort());
  QVERIFY(!server.isListening());
  QCOMPARE(server.errorCode(), QStringLiteral("address-in-use"));
  QCOMPARE(errorSpy.count(), 1);
  QCOMPARE(listeningSpy.count(), 2);

  blocker.close();
  QTRY_VERIFY_WITH_TIMEOUT(server.isListening(), 3000);
  QVERIFY(server.errorCode().isEmpty());
  QCOMPARE(server.listeningAddress(),
           QHostAddress(QHostAddress::LocalHost).toString());
  QCOMPARE(listeningSpy.count(), 3);
}

void LocalIntegrationsTest::webhookRuntimeResultsAreTypedAndRedacted() {
  TestWebhookManager network;
  network.responses = {204, 401, -1};
  WebhookNotifier notifier(nullptr, &network);
  notifier.setSlackEnabled(true);
  notifier.setSlackWebhookUrl(
      QStringLiteral("https://hooks.example.test/private-sentinel"));
  QList<QVariantMap> snapshots;
  connect(&notifier, &WebhookNotifier::runtimeStatusChanged, &notifier,
          [&]() { snapshots.append(notifier.lastDeliveryResults()); });

  notifier.sendAlert(QStringLiteral("first"), QStringLiteral("First"),
                     QStringLiteral("Test"));
  notifier.sendAlert(QStringLiteral("second"), QStringLiteral("Second"),
                     QStringLiteral("Test"));
  notifier.sendAlert(QStringLiteral("third"), QStringLiteral("Third"),
                     QStringLiteral("Test"));
  QTRY_COMPARE(snapshots.size(), 3);

  const QVariantMap accepted = snapshots.at(0).value(QStringLiteral("slack")).toMap();
  QCOMPARE(accepted.value(QStringLiteral("status")).toString(),
           QStringLiteral("delivered"));
  QCOMPARE(accepted.value(QStringLiteral("httpStatus")).toInt(), 204);

  const QVariantMap unauthorized = snapshots.at(1).value(QStringLiteral("slack")).toMap();
  QCOMPARE(unauthorized.value(QStringLiteral("reasonKey")).toString(),
           QStringLiteral("authentication-or-permission"));
  QCOMPARE(unauthorized.value(QStringLiteral("httpStatus")).toInt(), 401);

  const QVariantMap timeout = snapshots.at(2).value(QStringLiteral("slack")).toMap();
  QCOMPARE(timeout.value(QStringLiteral("reasonKey")).toString(),
           QStringLiteral("timeout"));
  const QByteArray safeSnapshot =
      QJsonDocument::fromVariant(snapshots.at(2)).toJson(QJsonDocument::Compact);
  QVERIFY(!safeSnapshot.contains("private-sentinel"));
  QVERIFY(!safeSnapshot.contains("hooks.example.test"));
}

void LocalIntegrationsTest::usageDatabaseExportsFiles() {
  UsageDatabase db;
  db.init();
  db.recordSnapshot(QStringLiteral("OpenAI"), 10, 5, 1, 0.25, 0.25, 0.25, 100,
                    99, 1000, 985, QStringLiteral("gpt-5.4-pro"), false);
  db.recordToolSnapshot(QStringLiteral("Cursor"), 3, 500,
                        QStringLiteral("Monthly"), QStringLiteral("Pro"),
                        false);

  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const auto result = db.exportAllToDirectory(
      dir.path(), {QStringLiteral("json"), QStringLiteral("csv")});
  QCOMPARE(result.value(QStringLiteral("status")).toString(),
           QStringLiteral("success"));
  const auto files = result.value(QStringLiteral("paths")).toStringList();
  QCOMPARE(files.size(), 6);
  for (const QString &path : files) {
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(QFileInfo(path).size() > 0);
  }
}

void LocalIntegrationsTest::webhookNotifierRejectsInsecureEndpoints() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  const quint16 port = server.serverPort();

  WebhookNotifier notifier;
  notifier.setSlackEnabled(true);
  notifier.setDiscordEnabled(true);
  notifier.setSlackWebhookUrl(
      QStringLiteral("http://127.0.0.1:%1/slack").arg(port));
  notifier.setDiscordWebhookUrl(
      QStringLiteral("http://127.0.0.1:%1/discord").arg(port));
  notifier.setCooldownMinutes(1);
  QSignalSpy failureSpy(&notifier, &WebhookNotifier::deliveryFailed);
  notifier.sendAlert(QStringLiteral("budget"), QStringLiteral("Budget warning"),
                     QStringLiteral("Threshold reached"), true);
  QCOMPARE(failureSpy.count(), 2);
  QVERIFY(!server.hasPendingConnections());
  QVERIFY(
      failureSpy.first().at(1).toString().contains(QStringLiteral("HTTPS")));
}

void LocalIntegrationsTest::webhookNotifierSanitizesGuardrailPayload() {
  WebhookNotifier notifier;
  QSignalSpy acceptedSpy(&notifier, &WebhookNotifier::guardrailEventAccepted);
  QSignalSpy failureSpy(&notifier, &WebhookNotifier::deliveryFailed);
  const QVariantMap event{
      {QStringLiteral("type"), QStringLiteral("guardrail")},
      {QStringLiteral("eventKey"),
       QStringLiteral("guardrail-local-key-warning")},
      {QStringLiteral("sourceId"), QStringLiteral("openai")},
      {QStringLiteral("kind"), QStringLiteral("quota_exhaustion")},
      {QStringLiteral("state"), QStringLiteral("warning")},
      {QStringLiteral("transition"), QStringLiteral("warning")},
      {QStringLiteral("title"), QStringLiteral("OpenAI guardrail warning")},
      {QStringLiteral("message"), QStringLiteral("Quota runway is warning.")},
      {QStringLiteral("critical"), false},
      {QStringLiteral("scope"), QStringLiteral("project:secret-project")},
      {QStringLiteral("modelScope"), QStringLiteral("secret-model")},
      {QStringLiteral("projectScope"), QStringLiteral("secret-project")},
      {QStringLiteral("workspaceScope"), QStringLiteral("secret-workspace")},
      {QStringLiteral("serviceTierScope"), QStringLiteral("secret-tier")},
      {QStringLiteral("lineItemScope"), QStringLiteral("secret-line-item")},
      {QStringLiteral("apiKeyId"), QStringLiteral("secret-key")},
  };

  notifier.sendGuardrailEvent(event);
  QCOMPARE(acceptedSpy.count(), 1);
  QCOMPARE(failureSpy.count(), 0);
  const QVariantMap sanitized = acceptedSpy.first().at(0).toMap();
  QCOMPARE(sanitized.value(QStringLiteral("sourceId")).toString(),
           QStringLiteral("openai"));
  QVERIFY(!sanitized.contains(QStringLiteral("scope")));
  QVERIFY(!sanitized.contains(QStringLiteral("modelScope")));
  QVERIFY(!sanitized.contains(QStringLiteral("projectScope")));
  QVERIFY(!sanitized.contains(QStringLiteral("workspaceScope")));
  QVERIFY(!sanitized.contains(QStringLiteral("serviceTierScope")));
  QVERIFY(!sanitized.contains(QStringLiteral("lineItemScope")));
  QVERIFY(!sanitized.contains(QStringLiteral("apiKeyId")));

  QVariantMap invalid = event;
  invalid.insert(QStringLiteral("sourceId"),
                 QStringLiteral("project:secret-project"));
  notifier.sendGuardrailEvent(invalid);
  QCOMPARE(acceptedSpy.count(), 1);
  QCOMPARE(failureSpy.count(), 1);
}

void LocalIntegrationsTest::webhookNotifierUsesStrictBudgetPolicyAllowlist() {
  WebhookNotifier notifier;
  QSignalSpy acceptedSpy(&notifier, &WebhookNotifier::guardrailEventAccepted);
  QSignalSpy failureSpy(&notifier, &WebhookNotifier::deliveryFailed);
  const QVariantMap event{
      {QStringLiteral("type"), QStringLiteral("guardrail")},
      {QStringLiteral("contractVersion"), QStringLiteral("budget-pacing-v2")},
      {QStringLiteral("eventKey"),
       QStringLiteral("budget-policy-transition-exceeded")},
      {QStringLiteral("policyId"), QStringLiteral("secret-policy-id")},
      {QStringLiteral("scopeIdentity"), QStringLiteral("secret-project")},
      {QStringLiteral("providerDisplayName"), QStringLiteral("OpenAI")},
      {QStringLiteral("risk"), QStringLiteral("exceeded")},
      {QStringLiteral("percentClass"), QStringLiteral("exceeded")},
      {QStringLiteral("period"), QStringLiteral("calendar_month")},
      {QStringLiteral("linkText"), QStringLiteral("Open Budget Control")},
      {QStringLiteral("transition"), QStringLiteral("exceeded")},
      {QStringLiteral("title"), QStringLiteral("secret-policy-id")},
      {QStringLiteral("message"), QStringLiteral("secret-project")},
  };

  notifier.sendGuardrailEvent(event);
  QCOMPARE(acceptedSpy.count(), 1);
  QCOMPARE(failureSpy.count(), 0);
  const QVariantMap sanitized = acceptedSpy.first().at(0).toMap();
  QCOMPARE(sanitized.size(), 5);
  QVERIFY(sanitized.contains(QStringLiteral("providerDisplayName")));
  QVERIFY(sanitized.contains(QStringLiteral("risk")));
  QVERIFY(sanitized.contains(QStringLiteral("percentClass")));
  QVERIFY(sanitized.contains(QStringLiteral("period")));
  QVERIFY(sanitized.contains(QStringLiteral("linkText")));
  QVERIFY(!sanitized.contains(QStringLiteral("policyId")));
  QVERIFY(!sanitized.contains(QStringLiteral("scopeIdentity")));

  QVariantMap invalid = event;
  invalid.insert(QStringLiteral("eventKey"),
                 QStringLiteral("secret-policy-id"));
  notifier.sendGuardrailEvent(invalid);
  QCOMPARE(acceptedSpy.count(), 1);
  QCOMPARE(failureSpy.count(), 1);
}

void LocalIntegrationsTest::awsSigV4SignerShapesHeaders() {
  const auto signedHeaders = AwsSigV4Signer::sign(
      QStringLiteral("AKIDEXAMPLE"),
      QStringLiteral("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"), QString(),
      QStringLiteral("us-east-1"), QStringLiteral("bedrock"),
      QStringLiteral("GET"), QStringLiteral("/foundation-models"),
      QStringLiteral("byOutputModality=TEXT"),
      {{QByteArrayLiteral("accept"), QByteArrayLiteral("application/json")},
       {QByteArrayLiteral("host"),
        QByteArrayLiteral("bedrock.us-east-1.amazonaws.com")}},
      QByteArray(),
      QDateTime(QDate(2026, 4, 10), QTime(12, 0), QTimeZone::utc()));

  QVERIFY(signedHeaders.authorizationHeader.startsWith(
      "AWS4-HMAC-SHA256 "
      "Credential=AKIDEXAMPLE/20260410/us-east-1/bedrock/aws4_request"));
  QVERIFY(signedHeaders.authorizationHeader.contains(
      "SignedHeaders=accept;host;x-amz-content-sha256;x-amz-date"));
  QCOMPARE(signedHeaders.amzDate, QByteArray("20260410T120000Z"));
  QCOMPARE(signedHeaders.payloadHash.size(), 64);
}

QTEST_MAIN(LocalIntegrationsTest)
#include "test_local_integrations.moc"
