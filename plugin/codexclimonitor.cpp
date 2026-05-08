#include "codexclimonitor.h"
#include <QDir>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <KLocalizedString>

#include "browsercookieextractor.h"

CodexCliMonitor::CodexCliMonitor(QObject *parent)
    : LocalActivityMonitorBase(parent)
{
    const QString configDir = codexConfigDir();
    setInstallExecutableNames({QStringLiteral("codex")});
    setInstallPaths({configDir});
    setWatchedPaths({configDir});
}

QString CodexCliMonitor::codexConfigDir() const
{
    // Codex CLI stores config in ~/.codex/
    return QDir::homePath() + QStringLiteral("/.codex");
}

QStringList CodexCliMonitor::availablePlans() const
{
    return catalogPlanLabels();
}

int CodexCliMonitor::defaultLimitForPlan(const QString &plan) const
{
    return catalogDefaultLimitForPlan(plan);
}

int CodexCliMonitor::defaultSecondaryLimitForPlan(const QString &plan) const
{
    return catalogDefaultSecondaryLimitForPlan(plan);
}

double CodexCliMonitor::subscriptionCost() const
{
    return defaultCostForPlan(planTier());
}

double CodexCliMonitor::defaultCostForPlan(const QString &plan) const
{
    return catalogDefaultCostForPlan(plan);
}

// --- Browser Sync ---

void CodexCliMonitor::syncFromBrowser(const QString &cookieHeader, int browserType)
{
    Q_UNUSED(browserType);
    if (isSyncing()) return;
    setSyncing(true);
    setSyncStatus(QStringLiteral("Syncing..."));

    if (cookieHeader.isEmpty()) {
        setSyncing(false);
        setSyncStatus(i18n("Not logged in"));
        const QString message = i18n("Not logged in — open chatgpt.com in the selected browser first");
        Q_EMIT syncDiagnostic(toolName(), QStringLiteral("not_logged_in"), message);
        Q_EMIT syncCompleted(false, message);
        return;
    }

    fetchAccountCheck(cookieHeader);
}

void CodexCliMonitor::fetchAccountCheck(const QString &cookieHeader)
{
    // ChatGPT internal API for account/usage info
    QString demoUrl = QString::fromLocal8Bit(qgetenv("PLASMA_AI_MONITOR_DEMO_BASE_URL")).trimmed();
    if (demoUrl.isEmpty()) {
        demoUrl = QStringLiteral("http://localhost:8080");
    }
    while (demoUrl.endsWith(QLatin1Char('/'))) {
        demoUrl.chop(1);
    }
    QUrl url = qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO")
        ? QUrl(QStringLiteral("%1/chatgpt/backend-api/accounts/check/v4-2023-04-27").arg(demoUrl))
        : QUrl(QStringLiteral("https://chatgpt.com/backend-api/accounts/check/v4-2023-04-27"));

    QNetworkRequest request(url);
    request.setRawHeader("Cookie", cookieHeader.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");
    // Force HTTP/1.1 — Qt's HTTP/2 implementation triggers 401 on ChatGPT backend API
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(30000); // 30 second timeout

    QNetworkReply *reply = networkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qWarning() << "CodexCliMonitor: Account check failed:" << reply->errorString() << "HTTP" << httpStatus;
            setSyncing(false);
            if (httpStatus == 401 || httpStatus == 403) {
                setSyncStatus(i18n("Session expired"));
                const QString message = i18n("Session expired — please log in to chatgpt.com in Firefox again");
                Q_EMIT syncDiagnostic(toolName(), QStringLiteral("session_expired"), message);
                Q_EMIT syncCompleted(false, message);
            } else {
                setSyncStatus(i18n("Sync failed"));
                const QString message = reply->errorString();
                Q_EMIT syncDiagnostic(toolName(), QStringLiteral("network_error"), message);
                Q_EMIT syncCompleted(false, message);
            }
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            setSyncing(false);
            setSyncStatus(i18n("Invalid response"));
            const QString message = i18n("Unexpected response from ChatGPT");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("invalid_response"), message);
            Q_EMIT syncCompleted(false, message);
            return;
        }

        QJsonObject root = doc.object();

        // The response contains accounts → account_id → rate_limits and usage
        // Navigate to the first account
        QJsonObject accounts = root.value(QStringLiteral("accounts")).toObject();
        QJsonObject accountData;
        for (auto it = accounts.begin(); it != accounts.end(); ++it) {
            accountData = it.value().toObject();
            break;
        }

        if (accountData.isEmpty()) {
            // Try alternate structure
            accountData = root;
        }

        // Parse rate limits
        QJsonObject rateLimits = accountData.value(QStringLiteral("rate_limits")).toObject();
        if (rateLimits.isEmpty()) {
            qWarning() << "CodexCliMonitor: No rate_limits found in response";
        }
        bool foundLiveQuota = false;
        if (!rateLimits.isEmpty()) {
            // Primary: 5-hour usage limit
            QJsonObject fiveHour = rateLimits.value(QStringLiteral("message_cap")).toObject();
            if (fiveHour.isEmpty()) {
                // Try alternate key
                for (auto it = rateLimits.begin(); it != rateLimits.end(); ++it) {
                    QJsonObject rl = it.value().toObject();
                    QString type = rl.value(QStringLiteral("type")).toString();
                    if (type.contains(QStringLiteral("5h")) || type.contains(QStringLiteral("five_hour"))) {
                        fiveHour = rl;
                        break;
                    }
                }
            }

            if (!fiveHour.isEmpty()) {
                int remaining = fiveHour.value(QStringLiteral("remaining")).toInt(-1);
                int limit = fiveHour.value(QStringLiteral("limit")).toInt(0);
                if (limit > 0) {
                    setUsageLimit(limit);
                    foundLiveQuota = true;
                    if (remaining >= 0) setUsageCount(limit - remaining);
                }
                // Parse resets_at for primary period
                QString resetsAt = fiveHour.value(QStringLiteral("resets_at")).toString();
                if (!resetsAt.isEmpty()) {
                    QDateTime resetTime = QDateTime::fromString(resetsAt, Qt::ISODate);
                    if (resetTime.isValid()) {
                        // Period start = reset time minus 5 hours
                        setPeriodStart(resetTime.addSecs(-5 * 3600));
                    }
                }
            }

            // Weekly usage limit
            QJsonObject weekly;
            for (auto it = rateLimits.begin(); it != rateLimits.end(); ++it) {
                QJsonObject rl = it.value().toObject();
                QString type = rl.value(QStringLiteral("type")).toString();
                if (type.contains(QStringLiteral("week"))) {
                    weekly = rl;
                    break;
                }
            }
            if (!weekly.isEmpty()) {
                int remaining = weekly.value(QStringLiteral("remaining")).toInt(-1);
                int limit = weekly.value(QStringLiteral("limit")).toInt(0);
                if (limit > 0) {
                    setSecondaryUsageLimit(limit);
                    foundLiveQuota = true;
                    if (remaining >= 0) setSecondaryUsageCount(limit - remaining);
                }
                // Parse resets_at for secondary period
                QString resetsAt = weekly.value(QStringLiteral("resets_at")).toString();
                if (!resetsAt.isEmpty()) {
                    QDateTime resetTime = QDateTime::fromString(resetsAt, Qt::ISODate);
                    if (resetTime.isValid()) {
                        // Period start = reset time minus 7 days
                        setSecondaryPeriodStart(resetTime.addDays(-7));
                    }
                }
            }

            // Code review (tertiary)
            QJsonObject codeReview;
            for (auto it = rateLimits.begin(); it != rateLimits.end(); ++it) {
                QJsonObject rl = it.value().toObject();
                QString type = rl.value(QStringLiteral("type")).toString();
                if (type.contains(QStringLiteral("code_review")) || type.contains(QStringLiteral("review"))) {
                    codeReview = rl;
                    break;
                }
            }
            if (!codeReview.isEmpty()) {
                int remaining = codeReview.value(QStringLiteral("remaining")).toInt(-1);
                int limit = codeReview.value(QStringLiteral("limit")).toInt(0);
                if (limit > 0 && remaining >= 0) {
                    foundLiveQuota = true;
                    double pctRemaining = (static_cast<double>(remaining) / limit) * 100.0;
                    setTertiaryPercentRemaining(pctRemaining);
                    m_hasTertiary = true;

                    QString resetsAt = codeReview.value(QStringLiteral("resets_at")).toString();
                    if (!resetsAt.isEmpty()) {
                        setTertiaryResetDate(QDateTime::fromString(resetsAt, Qt::ISODate));
                    }
                }
            }
        }

        // Parse credits/remaining
        double credits = accountData.value(QStringLiteral("remaining_credits")).toDouble(-1);
        if (credits < 0) {
            // Try alternate path
            QJsonObject billing = accountData.value(QStringLiteral("billing")).toObject();
            credits = billing.value(QStringLiteral("remaining_credits")).toDouble(-1);
        }
        if (credits >= 0) {
            setRemainingCredits(credits);
            m_hasCreditsData = true;
            foundLiveQuota = true;
        }

        // Detect plan from entitlement
        const QString entitlement = accountData.value(QStringLiteral("entitlement")).toString().toLower();
        const QString currentPlan = planIdForLabel(planTier()).toLower();
        if (entitlement.contains(QStringLiteral("pro"))) {
            setPlanTier(QStringLiteral("pro"));
        } else if (entitlement.contains(QStringLiteral("plus"))
                   && (currentPlan.isEmpty() || currentPlan == QLatin1String("free") || currentPlan == QLatin1String("go") || currentPlan == QLatin1String("plus"))) {
            setPlanTier(QStringLiteral("plus"));
        }

        // Sync complete
        setSyncing(false);
        setLastSyncTime(QDateTime::currentDateTimeUtc());
        if (foundLiveQuota) {
            setSyncStatus(i18n("Synced"));
            Q_EMIT syncCompleted(true, i18n("Codex usage data synced successfully"));
        } else {
            setSyncStatus(i18n("Plan presets"));
            const QString message = i18n("ChatGPT did not return live quota fields; showing configured plan presets.");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("no_live_quota"), message);
            Q_EMIT syncCompleted(true, message);
        }
        Q_EMIT usageUpdated();
    });
}
