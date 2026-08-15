#ifndef CATALOGUPDATER_H
#define CATALOGUPDATER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QtQmlIntegration/qqmlintegration.h>

class QUrl;
class QNetworkReply;

class CatalogUpdateManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool networkUpdatesEnabled READ networkUpdatesEnabled WRITE setNetworkUpdatesEnabled NOTIFY configurationChanged)
    Q_PROPERTY(QString feedUrl READ feedUrl WRITE setFeedUrl NOTIFY configurationChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY statusChanged)
    Q_PROPERTY(QDateTime lastChecked READ lastChecked NOTIFY statusChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY statusChanged)

public:
    explicit CatalogUpdateManager(QString fileName = QStringLiteral("providers-v4.json"),
                                  QObject *parent = nullptr);

    static QByteArray pinnedPublicKey();
    static bool verifyEd25519(const QByteArray &message,
                              const QByteArray &signature,
                              const QByteArray &publicKey = pinnedPublicKey());

    bool installSignedFeed(const QByteArray &envelope, QString *diagnostic = nullptr);
    QString cachePath() const;
    bool networkUpdatesEnabled() const;
    void setNetworkUpdatesEnabled(bool enabled);
    QString feedUrl() const;
    void setFeedUrl(const QString &url);
    QString status() const;
    QString diagnostic() const;
    QDateTime lastChecked() const;
    bool checking() const;
    Q_INVOKABLE void checkForUpdate();

Q_SIGNALS:
    void configurationChanged();
    void statusChanged();

private:
    void finishNetworkCheck(const QByteArray &body, const QString &etag,
                            const QString &lastModified, int httpStatus);
    bool allowedFeedUrl(const QUrl &url) const;
    void persistCheckState(const QString &etag, const QString &lastModified);

    QString m_fileName;
    QNetworkAccessManager *m_networkManager = nullptr;
    QPointer<QNetworkReply> m_reply;
    bool m_networkUpdatesEnabled = true;
    QString m_feedUrl;
    QString m_status = QStringLiteral("not-configured");
    QString m_diagnostic;
    QDateTime m_lastChecked;
    bool m_checking = false;
};

#endif // CATALOGUPDATER_H
