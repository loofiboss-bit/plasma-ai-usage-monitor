#include "updatechecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDebug>
#include <QRegularExpression>

static QString normalizeVersionString(QString version)
{
    version = version.trimmed();

    if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V')))
        version.remove(0, 1);

    // Keep only the numeric semantic part (e.g. "2.8.0" from "2.8.0-beta1").
    static const QRegularExpression re(QStringLiteral("^(\\d+\\.\\d+\\.\\d+)"));
    const QRegularExpressionMatch m = re.match(version);
    return m.hasMatch() ? m.captured(1) : QString();
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &UpdateChecker::checkForUpdate);
    QTimer::singleShot(5000, this, &UpdateChecker::checkForUpdate);
}

// ── Properties ──

QString UpdateChecker::currentVersion() const { return m_currentVersion; }
void UpdateChecker::setCurrentVersion(const QString &v)
{
    if (m_currentVersion != v) {
        m_currentVersion = v;
        Q_EMIT currentVersionChanged();
        startTimerIfReady();
    }
}

int UpdateChecker::checkIntervalHours() const { return m_intervalHours; }
void UpdateChecker::setCheckIntervalHours(int h)
{
    if (h < 1) h = 1;
    if (m_intervalHours != h) {
        m_intervalHours = h;
        Q_EMIT checkIntervalHoursChanged();
        startTimerIfReady();
    }
}

QString UpdateChecker::releaseApiUrl() const { return m_releaseApiUrl; }
void UpdateChecker::setReleaseApiUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty() || m_releaseApiUrl == trimmed) {
        return;
    }

    m_releaseApiUrl = trimmed;
    Q_EMIT releaseApiUrlChanged();
}

bool UpdateChecker::automaticChecksEnabled() const { return m_automaticChecksEnabled; }
void UpdateChecker::setAutomaticChecksEnabled(bool enabled)
{
    if (m_automaticChecksEnabled == enabled) {
        return;
    }

    m_automaticChecksEnabled = enabled;

    if (enabled) {
        startTimerIfReady();
    } else {
        m_timer->stop();
    }

    Q_EMIT automaticChecksEnabledChanged();
}

bool UpdateChecker::checking() const { return m_checking; }
QString UpdateChecker::latestVersion() const { return m_latestVersion; }

// ── Update Check ──

void UpdateChecker::checkForUpdate()
{
    if (!m_automaticChecksEnabled || m_currentVersion.isEmpty() || m_checking) return;

    m_checking = true;
    Q_EMIT checkingChanged();

    QNetworkRequest req{QUrl(m_releaseApiUrl)};
    req.setTransferTimeout(30000);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "plasma-ai-usage-monitor/" + m_currentVersion.toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        Q_EMIT checkingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "UpdateChecker: network error:" << reply->errorString();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;

        const QJsonObject obj = doc.object();
        QString tagName = obj.value(QStringLiteral("tag_name")).toString();
        const QString htmlUrl = obj.value(QStringLiteral("html_url")).toString();

        const QString normalizedRemote = normalizeVersionString(tagName);
        const QString normalizedLocal = normalizeVersionString(m_currentVersion);
        const QVersionNumber remote = QVersionNumber::fromString(normalizedRemote);
        const QVersionNumber local  = QVersionNumber::fromString(normalizedLocal);

        if (remote.isNull() || local.isNull()) return;

        m_latestVersion = normalizedRemote;
        Q_EMIT latestVersionChanged();

        if (remote > local) {
            Q_EMIT updateAvailable(normalizedRemote, htmlUrl);
        }
    });
}

// ── Private ──

void UpdateChecker::startTimerIfReady()
{
    if (!m_automaticChecksEnabled || m_currentVersion.isEmpty()) return;

    m_timer->setInterval(m_intervalHours * 3600 * 1000);
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}
