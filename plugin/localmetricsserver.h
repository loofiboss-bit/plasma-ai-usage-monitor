#ifndef LOCALMETRICSSERVER_H
#define LOCALMETRICSSERVER_H

#include <QObject>
#include <QTimer>
#include <QtQmlIntegration/qqmlintegration.h>

class QTcpServer;

class LocalMetricsServer : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool listenOnAllInterfaces READ listenOnAllInterfaces WRITE setListenOnAllInterfaces NOTIFY
            listenOnAllInterfacesChanged)
    Q_PROPERTY(QString payload READ payload WRITE setPayload NOTIFY payloadChanged)
    Q_PROPERTY(bool listening READ isListening NOTIFY listeningChanged)
    Q_PROPERTY(QString listeningAddress READ listeningAddress NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY runtimeStatusChanged)

public:
    explicit LocalMetricsServer(QObject *parent = nullptr);
    ~LocalMetricsServer() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    int port() const;
    void setPort(int port);

    bool listenOnAllInterfaces() const;
    void setListenOnAllInterfaces(bool enabled);
    QString listeningAddress() const;
    QString errorCode() const;

    QString payload() const;
    void setPayload(const QString &payload);

    bool isListening() const;

Q_SIGNALS:
    void enabledChanged();
    void portChanged();
    void listenOnAllInterfacesChanged();
    void payloadRequested();
    void payloadChanged();
    void listeningChanged();
    void runtimeStatusChanged();
    void error(const QString &message);

private:
    void restartServer();

    QTcpServer *m_server = nullptr;
    QTimer m_retryTimer;
    bool m_enabled = false;
    int m_port = 9464;
    bool m_listenOnAllInterfaces = false;
    QString m_errorCode;
    QString m_payload;
};

#endif // LOCALMETRICSSERVER_H
