#include "localmetricsserver.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>

LocalMetricsServer::LocalMetricsServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (m_server->hasPendingConnections()) {
            QTcpSocket *socket = m_server->nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray request = socket->property("requestBuffer").toByteArray();
                request += socket->readAll();
                if (request.size() > 8192) {
                    socket->write("HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                if (!request.contains("\r\n\r\n")) {
                    socket->setProperty("requestBuffer", request);
                    return;
                }
                const QList<QByteArray> requestLine = request.left(request.indexOf("\r\n")).split(' ');
                if (requestLine.size() != 3) {
                    socket->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                const QByteArray method = requestLine.at(0);
                const QByteArray path = requestLine.at(1);
                if (path != "/metrics") {
                    socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                if (method != "GET" && method != "HEAD") {
                    socket->write("HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                const QByteArray body = m_payload.toUtf8();
                QByteArray response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                    "Cache-Control: no-store\r\n"
                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                    "Connection: close\r\n\r\n";
                if (method == "GET") {
                    response += body;
                }
                socket->write(response);
                socket->disconnectFromHost();
            });
            QTimer::singleShot(5000, socket, [socket]() {
                if (socket->state() != QAbstractSocket::UnconnectedState) {
                    socket->disconnectFromHost();
                }
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

LocalMetricsServer::~LocalMetricsServer() = default;

bool LocalMetricsServer::isEnabled() const
{
    return m_enabled;
}

void LocalMetricsServer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        restartServer();
        Q_EMIT enabledChanged();
    }
}

int LocalMetricsServer::port() const
{
    return m_port;
}

void LocalMetricsServer::setPort(int port)
{
    const int clampedPort = qBound(1024, port, 65535);
    if (m_port != clampedPort) {
        m_port = clampedPort;
        restartServer();
        Q_EMIT portChanged();
    }
}

QString LocalMetricsServer::payload() const
{
    return m_payload;
}

void LocalMetricsServer::setPayload(const QString &payload)
{
    if (m_payload != payload) {
        m_payload = payload;
        Q_EMIT payloadChanged();
    }
}

bool LocalMetricsServer::isListening() const
{
    return m_server->isListening();
}

void LocalMetricsServer::restartServer()
{
    const bool wasListening = m_server->isListening();
    if (wasListening) {
        m_server->close();
    }

    if (m_enabled) {
        if (!m_server->listen(QHostAddress::LocalHost, static_cast<quint16>(m_port))) {
            Q_EMIT error(m_server->errorString());
        }
    }

    if (wasListening != m_server->isListening()) {
        Q_EMIT listeningChanged();
    }
}
