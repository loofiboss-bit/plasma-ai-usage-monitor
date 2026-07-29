#include "webhooknotifier.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {
constexpr int WEBHOOK_TIMEOUT_MS = 15000;
constexpr int MAX_TITLE_CHARS = 512;
constexpr int MAX_MESSAGE_CHARS = 4000;
}

WebhookNotifier::WebhookNotifier(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

WebhookNotifier::~WebhookNotifier() = default;

bool WebhookNotifier::slackEnabled() const { return m_slackEnabled; }
void WebhookNotifier::setSlackEnabled(bool enabled)
{
    if (m_slackEnabled != enabled) {
        m_slackEnabled = enabled;
        Q_EMIT configChanged();
    }
}

bool WebhookNotifier::discordEnabled() const { return m_discordEnabled; }
void WebhookNotifier::setDiscordEnabled(bool enabled)
{
    if (m_discordEnabled != enabled) {
        m_discordEnabled = enabled;
        Q_EMIT configChanged();
    }
}

QString WebhookNotifier::slackWebhookUrl() const { return m_slackWebhookUrl; }
void WebhookNotifier::setSlackWebhookUrl(const QString &url)
{
    if (m_slackWebhookUrl != url) {
        m_slackWebhookUrl = url.trimmed();
        Q_EMIT configChanged();
    }
}

QString WebhookNotifier::discordWebhookUrl() const { return m_discordWebhookUrl; }
void WebhookNotifier::setDiscordWebhookUrl(const QString &url)
{
    if (m_discordWebhookUrl != url) {
        m_discordWebhookUrl = url.trimmed();
        Q_EMIT configChanged();
    }
}

int WebhookNotifier::cooldownMinutes() const { return m_cooldownMinutes; }
void WebhookNotifier::setCooldownMinutes(int minutes)
{
    const int clamped = qBound(1, minutes, 1440);
    if (m_cooldownMinutes != clamped) {
        m_cooldownMinutes = clamped;
        Q_EMIT configChanged();
    }
}

void WebhookNotifier::sendAlert(const QString &eventKey, const QString &title, const QString &message, bool critical)
{
    if (!shouldSend(eventKey)) {
        return;
    }

    if (m_slackEnabled && validateWebhookUrl(QStringLiteral("slack"), m_slackWebhookUrl)) {
        postSlack(title, message, critical);
    }
    if (m_discordEnabled && validateWebhookUrl(QStringLiteral("discord"), m_discordWebhookUrl)) {
        postDiscord(title, message, critical);
    }
}

void WebhookNotifier::sendGuardrailEvent(const QVariantMap &event)
{
    const QString eventKey = event.value(QStringLiteral("eventKey")).toString();
    const QString title = event.value(QStringLiteral("title")).toString();
    const QString message = event.value(QStringLiteral("message")).toString();
    const QString transition = event.value(QStringLiteral("transition")).toString();
    const QString state = event.value(QStringLiteral("state")).toString();
    const QString sourceId = event.value(QStringLiteral("sourceId")).toString();
    const QString kind = event.value(QStringLiteral("kind")).toString();
    const QSet<QString> allowedTransitions {
        QStringLiteral("warning"),
        QStringLiteral("critical"),
        QStringLiteral("recovered"),
    };
    const QSet<QString> allowedStates {
        QStringLiteral("safe"),
        QStringLiteral("warning"),
        QStringLiteral("critical"),
        QStringLiteral("unavailable"),
    };
    const QRegularExpression lowCardinalityId(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    if (event.value(QStringLiteral("type")).toString() != QLatin1String("guardrail") || eventKey.isEmpty()
        || title.isEmpty() || message.isEmpty() || !allowedTransitions.contains(transition)
        || !allowedStates.contains(state) || !lowCardinalityId.match(sourceId).hasMatch()
        || !lowCardinalityId.match(kind).hasMatch()) {
        Q_EMIT deliveryFailed(QStringLiteral("guardrail"), QStringLiteral("Invalid guardrail event"));
        return;
    }

    // Only the allow-listed semantic payload is observable by integrations.
    // Raw scope identifiers and model/project/workspace dimensions stay local.
    QVariantMap sanitized {
        { QStringLiteral("type"), QStringLiteral("guardrail") },
        { QStringLiteral("eventKey"), eventKey },
        { QStringLiteral("sourceId"), sourceId },
        { QStringLiteral("kind"), kind },
        { QStringLiteral("state"), state },
        { QStringLiteral("transition"), transition },
        { QStringLiteral("predictedAt"), event.value(QStringLiteral("predictedAt")) },
        { QStringLiteral("periodEnd"), event.value(QStringLiteral("periodEnd")) },
        { QStringLiteral("evidenceGrade"), event.value(QStringLiteral("evidenceGrade")) },
        { QStringLiteral("methodId"), event.value(QStringLiteral("methodId")) },
        { QStringLiteral("valueClass"), event.value(QStringLiteral("valueClass")) },
        { QStringLiteral("title"), title.left(MAX_TITLE_CHARS) },
        { QStringLiteral("message"), message.left(MAX_MESSAGE_CHARS) },
        { QStringLiteral("critical"), event.value(QStringLiteral("critical")).toBool() },
    };
    Q_EMIT guardrailEventAccepted(sanitized);
    sendAlert(eventKey, sanitized.value(QStringLiteral("title")).toString(),
        sanitized.value(QStringLiteral("message")).toString(), sanitized.value(QStringLiteral("critical")).toBool());
}

bool WebhookNotifier::validateWebhookUrl(const QString &channel, const QString &url)
{
    const QUrl parsed(url);
    if (!parsed.isValid() || parsed.scheme() != QLatin1String("https") || parsed.host().isEmpty()) {
        Q_EMIT deliveryFailed(channel, QStringLiteral("Webhook URL must use HTTPS"));
        return false;
    }
    return true;
}

bool WebhookNotifier::shouldSend(const QString &eventKey)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime last = m_lastSent.value(eventKey);
    if (last.isValid() && last.secsTo(now) < (m_cooldownMinutes * 60)) {
        return false;
    }
    m_lastSent.insert(eventKey, now);
    return true;
}

void WebhookNotifier::postSlack(const QString &title, const QString &message, bool critical)
{
    QNetworkRequest request { QUrl(m_slackWebhookUrl) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(WEBHOOK_TIMEOUT_MS);

    QJsonObject payload;
    payload.insert(QStringLiteral("text"),
        QStringLiteral("%1 %2\n%3")
            .arg(critical ? QStringLiteral("[critical]") : QStringLiteral("[info]"), title.left(MAX_TITLE_CHARS),
                message.left(MAX_MESSAGE_CHARS)));

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            Q_EMIT deliveryFailed(QStringLiteral("slack"),
                reply->error() == QNetworkReply::NoError ? QStringLiteral("HTTP %1").arg(status)
                                                         : reply->errorString());
        } else {
            Q_EMIT delivered(QStringLiteral("slack"), status);
        }
        reply->deleteLater();
    });
}

void WebhookNotifier::postDiscord(const QString &title, const QString &message, bool critical)
{
    QNetworkRequest request { QUrl(m_discordWebhookUrl) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(WEBHOOK_TIMEOUT_MS);

    QJsonObject embed;
    embed.insert(QStringLiteral("title"), title.left(MAX_TITLE_CHARS));
    embed.insert(QStringLiteral("description"), message.left(MAX_MESSAGE_CHARS));
    embed.insert(QStringLiteral("color"), critical ? 15158332 : 3447003);

    QJsonObject payload;
    payload.insert(QStringLiteral("content"), QStringLiteral("AI Usage Monitor"));
    payload.insert(QStringLiteral("embeds"), QJsonArray { embed });

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            Q_EMIT deliveryFailed(QStringLiteral("discord"),
                reply->error() == QNetworkReply::NoError ? QStringLiteral("HTTP %1").arg(status)
                                                         : reply->errorString());
        } else {
            Q_EMIT delivered(QStringLiteral("discord"), status);
        }
        reply->deleteLater();
    });
}
