#include <QtTest>
#include <QSignalSpy>
#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "updatechecker.h"

class UpdateStubServer : public QObject
{
    Q_OBJECT

public:
    struct Response {
        int status = 200;
        QByteArray body = "{}";
        QList<QPair<QByteArray, QByteArray>> headers;
    };

    explicit UpdateStubServer(QObject *parent = nullptr)
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
                    const QString path = QString::fromUtf8(firstLine.at(1));
                    const QString key = method + QStringLiteral(" ") + path;
                    m_hitCount[path] = m_hitCount.value(path, 0) + 1;

                    const Response response = m_routes.value(key, Response{});

                    QByteArray payload;
                    payload += "HTTP/1.1 " + QByteArray::number(response.status) + " OK\r\n";
                    payload += "Content-Type: application/json\r\n";
                    for (const auto &header : response.headers) {
                        payload += header.first + ": " + header.second + "\r\n";
                    }
                    payload += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
                    payload += "Connection: close\r\n\r\n";
                    payload += response.body;

                    socket->write(payload);
                    socket->disconnectFromHost();
                });

                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    QString urlFor(const QString &path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path);
    }

    void setResponse(const QString &method, const QString &path, int status, const QByteArray &body)
    {
        m_routes.insert(method + QStringLiteral(" ") + path, Response{status, body, {}});
    }

    int hitCount(const QString &path) const
    {
        return m_hitCount.value(path, 0);
    }

private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QString, Response> m_routes;
    QHash<QString, int> m_hitCount;
};

class UpdateCheckerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSetCurrentVersion();
    void testSetCheckIntervalHours();
    void testCheckIntervalClamping();
    void testInitialState();
    void testDefaultReleaseApiUrl();
    void testSetReleaseApiUrl();
    void testEmitsUpdateForNewerRelease();
    void testDoesNotEmitUpdateForSameVersion();
    void testMissingCurrentVersionDoesNotRequest();
    void testDisabledCheckerDoesNotRequest();
    void testReEnablingAllowsSingleRequest();
};

void UpdateCheckerTest::testSetCurrentVersion()
{
    UpdateChecker checker;
    QSignalSpy versionSpy(&checker, &UpdateChecker::currentVersionChanged);

    checker.setCurrentVersion(QStringLiteral("2.8.1"));
    QCOMPARE(checker.currentVersion(), QStringLiteral("2.8.1"));
    QCOMPARE(versionSpy.count(), 1);

    // Setting same value — no signal
    checker.setCurrentVersion(QStringLiteral("2.8.1"));
    QCOMPARE(versionSpy.count(), 1);

    // Setting different value — signal
    checker.setCurrentVersion(QStringLiteral("3.0.0"));
    QCOMPARE(versionSpy.count(), 2);
}

void UpdateCheckerTest::testSetCheckIntervalHours()
{
    UpdateChecker checker;
    QSignalSpy intervalSpy(&checker, &UpdateChecker::checkIntervalHoursChanged);

    QCOMPARE(checker.checkIntervalHours(), 12); // default

    checker.setCheckIntervalHours(24);
    QCOMPARE(checker.checkIntervalHours(), 24);
    QCOMPARE(intervalSpy.count(), 1);

    // Setting same value — no signal
    checker.setCheckIntervalHours(24);
    QCOMPARE(intervalSpy.count(), 1);
}

void UpdateCheckerTest::testCheckIntervalClamping()
{
    UpdateChecker checker;

    // Values < 1 should be clamped to 1
    checker.setCheckIntervalHours(0);
    QCOMPARE(checker.checkIntervalHours(), 1);

    checker.setCheckIntervalHours(-5);
    QCOMPARE(checker.checkIntervalHours(), 1);
}

void UpdateCheckerTest::testInitialState()
{
    UpdateChecker checker;
    QVERIFY(checker.currentVersion().isEmpty());
    QVERIFY(checker.latestVersion().isEmpty());
    QVERIFY(!checker.checking());
    QVERIFY(checker.automaticChecksEnabled());
    QCOMPARE(checker.checkIntervalHours(), 12);
}

void UpdateCheckerTest::testDefaultReleaseApiUrl()
{
    UpdateChecker checker;
    QCOMPARE(checker.releaseApiUrl(),
             QStringLiteral("https://api.github.com/repos/loofiboss-bit/plasma-ai-usage-monitor/releases/latest"));
}

void UpdateCheckerTest::testSetReleaseApiUrl()
{
    UpdateChecker checker;
    QSignalSpy urlSpy(&checker, &UpdateChecker::releaseApiUrlChanged);

    checker.setReleaseApiUrl(QStringLiteral("http://127.0.0.1:9000/releases/latest"));
    QCOMPARE(checker.releaseApiUrl(), QStringLiteral("http://127.0.0.1:9000/releases/latest"));
    QCOMPARE(urlSpy.count(), 1);

    checker.setReleaseApiUrl(QStringLiteral("http://127.0.0.1:9000/releases/latest"));
    QCOMPARE(urlSpy.count(), 1);
}

void UpdateCheckerTest::testEmitsUpdateForNewerRelease()
{
    UpdateStubServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("GET"),
                       QStringLiteral("/releases/latest"),
                       200,
                       QByteArrayLiteral(R"JSON({
                           "tag_name": "v3.9.0-beta1",
                           "html_url": "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/releases/tag/v3.9.0"
                       })JSON"));

    UpdateChecker checker;
    checker.setCurrentVersion(QStringLiteral("3.8.1"));
    checker.setReleaseApiUrl(server.urlFor(QStringLiteral("/releases/latest")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy latestSpy(&checker, &UpdateChecker::latestVersionChanged);

    checker.checkForUpdate();

    QTRY_VERIFY_WITH_TIMEOUT(updateSpy.count() >= 1, 3000);
    QVERIFY(latestSpy.count() >= 1);
    QCOMPARE(checker.latestVersion(), QStringLiteral("3.9.0"));
    QCOMPARE(server.hitCount(QStringLiteral("/releases/latest")), 1);

    const QList<QVariant> args = updateSpy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("3.9.0"));
    QCOMPARE(args.at(1).toString(),
             QStringLiteral("https://github.com/loofiboss-bit/plasma-ai-usage-monitor/releases/tag/v3.9.0"));
}

void UpdateCheckerTest::testDoesNotEmitUpdateForSameVersion()
{
    UpdateStubServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("GET"),
                       QStringLiteral("/releases/latest"),
                       200,
                       QByteArrayLiteral(R"JSON({
                           "tag_name": "v3.8.1",
                           "html_url": "https://github.com/loofiboss-bit/plasma-ai-usage-monitor/releases/tag/v3.8.1"
                       })JSON"));

    UpdateChecker checker;
    checker.setCurrentVersion(QStringLiteral("3.8.1"));
    checker.setReleaseApiUrl(server.urlFor(QStringLiteral("/releases/latest")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy latestSpy(&checker, &UpdateChecker::latestVersionChanged);

    checker.checkForUpdate();

    QTRY_VERIFY_WITH_TIMEOUT(latestSpy.count() >= 1, 3000);
    QCOMPARE(checker.latestVersion(), QStringLiteral("3.8.1"));
    QCOMPARE(updateSpy.count(), 0);
}

void UpdateCheckerTest::testMissingCurrentVersionDoesNotRequest()
{
    UpdateStubServer server;
    QVERIFY(server.listen());

    UpdateChecker checker;
    checker.setReleaseApiUrl(server.urlFor(QStringLiteral("/releases/latest")));

    checker.checkForUpdate();

    QTest::qWait(100);
    QCOMPARE(server.hitCount(QStringLiteral("/releases/latest")), 0);
    QVERIFY(!checker.checking());
}

void UpdateCheckerTest::testDisabledCheckerDoesNotRequest()
{
    UpdateStubServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("GET"),
                       QStringLiteral("/releases/latest"),
                       200,
                       QByteArrayLiteral(R"JSON({
                           "tag_name": "v3.9.0",
                           "html_url": "https://example.test/releases/v3.9.0"
                       })JSON"));

    UpdateChecker checker;
    checker.setReleaseApiUrl(server.urlFor(QStringLiteral("/releases/latest")));
    checker.setCurrentVersion(QStringLiteral("3.8.1"));

    QSignalSpy enabledSpy(&checker, &UpdateChecker::automaticChecksEnabledChanged);
    QSignalSpy updateSpy(&checker, &UpdateChecker::updateAvailable);

    checker.setAutomaticChecksEnabled(false);
    checker.checkForUpdate();

    QTest::qWait(100);
    QCOMPARE(server.hitCount(QStringLiteral("/releases/latest")), 0);
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(enabledSpy.count(), 1);
    QVERIFY(!checker.automaticChecksEnabled());
    QVERIFY(!checker.checking());
}

void UpdateCheckerTest::testReEnablingAllowsSingleRequest()
{
    UpdateStubServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("GET"),
                       QStringLiteral("/releases/latest"),
                       200,
                       QByteArrayLiteral(R"JSON({
                           "tag_name": "v3.8.1",
                           "html_url": "https://example.test/releases/v3.8.1"
                       })JSON"));

    UpdateChecker checker;
    checker.setReleaseApiUrl(server.urlFor(QStringLiteral("/releases/latest")));
    checker.setAutomaticChecksEnabled(false);
    checker.setCurrentVersion(QStringLiteral("3.8.1"));

    checker.setAutomaticChecksEnabled(true);
    QTest::qWait(100);
    QCOMPARE(server.hitCount(QStringLiteral("/releases/latest")), 0);

    checker.checkForUpdate();

    QTRY_COMPARE_WITH_TIMEOUT(server.hitCount(QStringLiteral("/releases/latest")), 1, 1000);
    QTest::qWait(200);
    QCOMPARE(server.hitCount(QStringLiteral("/releases/latest")), 1);
}

QTEST_MAIN(UpdateCheckerTest)
#include "test_updatechecker.moc"
