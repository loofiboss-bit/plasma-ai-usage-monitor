#include "codexclimonitor.h"
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <KLocalizedString>

#include "browsercookieextractor.h"

namespace {
double numericField(const QJsonObject &object, const QString &snakeName, const QString &camelName)
{
    const QJsonValue snake = object.value(snakeName);
    if (snake.isDouble()) return snake.toDouble();
    const QJsonValue camel = object.value(camelName);
    return camel.isDouble() ? camel.toDouble() : -1.0;
}

QVariantMap codexQuotaWindow(const QJsonObject &window)
{
    const double usedPercent = numericField(window, QStringLiteral("used_percent"), QStringLiteral("usedPercent"));
    const double windowSeconds = numericField(window, QStringLiteral("limit_window_seconds"), QStringLiteral("windowSeconds"));
    const double resetAt = numericField(window, QStringLiteral("reset_at"), QStringLiteral("resetsAt"));
    if (usedPercent < 0.0) return {};

    const bool weekly = windowSeconds >= 6.0 * 24.0 * 60.0 * 60.0;
    QVariantMap row;
    row.insert(QStringLiteral("kind"), weekly ? QStringLiteral("rolling_weekly") : QStringLiteral("rolling_5h"));
    row.insert(QStringLiteral("label"), weekly ? QStringLiteral("Weekly Codex limit") : QStringLiteral("5-hour Codex limit"));
    row.insert(QStringLiteral("unit"), QStringLiteral("percent_remaining"));
    row.insert(QStringLiteral("percentUsed"), qBound(0.0, usedPercent, 100.0));
    row.insert(QStringLiteral("percentRemaining"), qBound(0.0, 100.0 - usedPercent, 100.0));
    row.insert(QStringLiteral("source"), QStringLiteral("browser_sync"));
    row.insert(QStringLiteral("precision"), QStringLiteral("browser_sync_actual"));
    row.insert(QStringLiteral("visibleByDefault"), true);
    if (resetAt > 0.0) {
        const QDateTime reset = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(resetAt), QTimeZone::UTC);
        row.insert(QStringLiteral("resetAt"), reset.toString(Qt::ISODate));
        qint64 remaining = QDateTime::currentDateTimeUtc().secsTo(reset);
        if (remaining > 0) {
            const qint64 days = remaining / 86400;
            const qint64 hours = (remaining % 86400) / 3600;
            row.insert(QStringLiteral("timeUntilReset"), days > 0
                ? QStringLiteral("%1d %2h").arg(days).arg(hours)
                : QStringLiteral("%1h %2m").arg(hours).arg((remaining % 3600) / 60));
        }
    }
    return row;
}
}

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

    if (!qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO") && fetchCodexUsage(cookieHeader)) {
        return;
    }

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

bool CodexCliMonitor::fetchCodexUsage(const QString &cookieHeader)
{
    QFile authFile(codexConfigDir() + QStringLiteral("/auth.json"));
    if (!authFile.open(QIODevice::ReadOnly)) return false;

    const QJsonObject auth = QJsonDocument::fromJson(authFile.readAll()).object();
    const QJsonObject tokens = auth.value(QStringLiteral("tokens")).toObject();
    const QString accessToken = tokens.value(QStringLiteral("access_token")).toString();
    const QString accountId = tokens.value(QStringLiteral("account_id")).toString();
    if (accessToken.isEmpty()) return false;

    QNetworkRequest request(QUrl(QStringLiteral("https://chatgpt.com/backend-api/wham/usage")));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("OpenAI-Beta", "codex-1");
    request.setRawHeader("originator", "Codex CLI");
    request.setRawHeader("User-Agent", "codex-cli");
    if (!accountId.isEmpty()) request.setRawHeader("ChatGPT-Account-ID", accountId.toUtf8());
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setTransferTimeout(30000);

    QNetworkReply *reply = networkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, cookieHeader]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if ((status == 401 || status == 403) && !cookieHeader.isEmpty()) {
                qWarning() << "CodexCliMonitor: Codex usage request rejected; falling back to browser account check, HTTP" << status;
                fetchAccountCheck(cookieHeader);
                return;
            }
            setSyncing(false);
            setSyncStatus(i18n("Sync failed"));
            Q_EMIT syncCompleted(false, reply->errorString());
            return;
        }

        const QByteArray payload = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(payload).object();
        const QVariantList windows = quotaWindowsFromUsagePayload(payload);
        setSyncedQuotaWindows(windows);

        const QString reportedPlan = root.value(QStringLiteral("plan_type")).toString().toLower();
        if (!reportedPlan.isEmpty()) setPlanTier(reportedPlan);

        const QJsonObject credits = root.value(QStringLiteral("credits")).toObject();
        if (credits.value(QStringLiteral("has_credits")).toBool()) {
            bool ok = false;
            const double balance = credits.value(QStringLiteral("balance")).toString().toDouble(&ok);
            if (ok) {
                setRemainingCredits(qMax(0, qRound(balance)));
                m_hasCreditsData = true;
            }
        }

        setSyncing(false);
        setLastSyncTime(QDateTime::currentDateTimeUtc());
        if (windows.isEmpty()) {
            setSyncStatus(i18n("Plan presets"));
            Q_EMIT syncCompleted(true, i18n("Codex did not return live quota fields; showing configured plan presets."));
        } else {
            setSyncStatus(i18n("Synced"));
            Q_EMIT syncCompleted(true, i18n("Codex limits synced successfully"));
        }
        Q_EMIT usageUpdated();
    });
    return true;
}

QVariantList CodexCliMonitor::quotaWindowsFromUsagePayload(const QByteArray &payload)
{
    const QJsonObject root = QJsonDocument::fromJson(payload).object();
    const QJsonObject rateLimit = root.value(QStringLiteral("rate_limit")).toObject();
    QVariantList windows;
    const QVariantMap primary = codexQuotaWindow(rateLimit.value(QStringLiteral("primary_window")).toObject());
    const QVariantMap secondary = codexQuotaWindow(rateLimit.value(QStringLiteral("secondary_window")).toObject());
    if (!primary.isEmpty()) windows << primary;
    if (!secondary.isEmpty()) windows << secondary;
    return windows;
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
