#ifndef WEBHOOKNOTIFIER_H
#define WEBHOOKNOTIFIER_H

#include <QObject>
#include <QDateTime>
#include <QHash>

class QNetworkAccessManager;

class WebhookNotifier : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool slackEnabled READ slackEnabled WRITE setSlackEnabled NOTIFY configChanged)
    Q_PROPERTY(bool discordEnabled READ discordEnabled WRITE setDiscordEnabled NOTIFY configChanged)
    Q_PROPERTY(QString slackWebhookUrl READ slackWebhookUrl WRITE setSlackWebhookUrl NOTIFY configChanged)
    Q_PROPERTY(QString discordWebhookUrl READ discordWebhookUrl WRITE setDiscordWebhookUrl NOTIFY configChanged)
    Q_PROPERTY(int cooldownMinutes READ cooldownMinutes WRITE setCooldownMinutes NOTIFY configChanged)

public:
    explicit WebhookNotifier(QObject *parent = nullptr);
    ~WebhookNotifier() override;

    bool slackEnabled() const;
    void setSlackEnabled(bool enabled);

    bool discordEnabled() const;
    void setDiscordEnabled(bool enabled);

    QString slackWebhookUrl() const;
    void setSlackWebhookUrl(const QString &url);

    QString discordWebhookUrl() const;
    void setDiscordWebhookUrl(const QString &url);

    int cooldownMinutes() const;
    void setCooldownMinutes(int minutes);

    Q_INVOKABLE void sendAlert(const QString &eventKey,
                               const QString &title,
                               const QString &message,
                               bool critical = false);

Q_SIGNALS:
    void configChanged();
    void deliveryFailed(const QString &channel, const QString &message);
    void delivered(const QString &channel, int httpStatus);

private:
    bool shouldSend(const QString &eventKey);
    void postSlack(const QString &title, const QString &message, bool critical);
    void postDiscord(const QString &title, const QString &message, bool critical);
    bool validateWebhookUrl(const QString &channel, const QString &url);

    QNetworkAccessManager *m_networkManager = nullptr;
    bool m_slackEnabled = false;
    bool m_discordEnabled = false;
    QString m_slackWebhookUrl;
    QString m_discordWebhookUrl;
    int m_cooldownMinutes = 15;
    QHash<QString, QDateTime> m_lastSent;
};

#endif // WEBHOOKNOTIFIER_H
