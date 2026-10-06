#include "claudecodemonitor.h"
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

ClaudeCodeMonitor::ClaudeCodeMonitor(QObject *parent)
    : LocalActivityMonitorBase(parent)
{
    const QString configDir = claudeConfigDir();
    setInstallExecutableNames({QStringLiteral("claude")});
    setInstallPaths({
        configDir,
        QDir::homePath() + QStringLiteral("/.claude.json")
    });
    setWatchedPaths({
        configDir,
        configDir + QStringLiteral("/projects"),
        QDir::homePath() + QStringLiteral("/.claude.json")
    });
    setIgnoredPathSuffixes({
        QStringLiteral("/settings.json"),
        QStringLiteral("/.claude.json")
    });
}

QString ClaudeCodeMonitor::claudeConfigDir() const
{
    // Claude Code stores config in ~/.claude/ by default
    // Can be overridden with CLAUDE_CONFIG_DIR env var
    QString envDir = QString::fromLocal8Bit(qgetenv("CLAUDE_CONFIG_DIR"));
    if (!envDir.isEmpty()) return envDir;
    return QDir::homePath() + QStringLiteral("/.claude");
}

QStringList ClaudeCodeMonitor::availablePlans() const
{
    return catalogPlanLabels();
}

int ClaudeCodeMonitor::defaultLimitForPlan(const QString &plan) const
{
    return catalogDefaultLimitForPlan(plan);
}

int ClaudeCodeMonitor::defaultSecondaryLimitForPlan(const QString &plan) const
{
    return catalogDefaultSecondaryLimitForPlan(plan);
}

double ClaudeCodeMonitor::subscriptionCost() const
{
    return defaultCostForPlan(planTier());
}

double ClaudeCodeMonitor::defaultCostForPlan(const QString &plan) const
{
    return catalogDefaultCostForPlan(plan);
}

// --- Browser Sync ---

void ClaudeCodeMonitor::syncFromBrowser(const QString &cookieHeader, int browserType)
{
    Q_UNUSED(browserType);
    if (!isEnabled() || isSyncing()) return;
    if (beginSyncGeneration() == 0) return;
    setSyncing(true);
    setSyncStatus(QStringLiteral("Syncing..."));

    if (cookieHeader.isEmpty()) {
        setSyncing(false);
        setSyncStatus(i18n("Not logged in"));
        const QString message = i18n("Not logged in — open claude.ai in the selected browser first");
        Q_EMIT syncDiagnostic(toolName(), QStringLiteral("not_logged_in"), message);
        Q_EMIT syncCompleted(false, message);
        return;
    }

    fetchAccountInfo(cookieHeader);
}

void ClaudeCodeMonitor::fetchAccountInfo(const QString &cookieHeader)
{
    // Use /api/bootstrap to get account info and organization UUID
    QString demoUrl = QString::fromLocal8Bit(qgetenv("PLASMA_AI_MONITOR_DEMO_BASE_URL")).trimmed();
    if (demoUrl.isEmpty()) {
        demoUrl = QStringLiteral("http://localhost:8080");
    }
    while (demoUrl.endsWith(QLatin1Char('/'))) {
        demoUrl.chop(1);
    }
    QUrl url = qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO") 
        ? QUrl(QStringLiteral("%1/claude/api/bootstrap").arg(demoUrl))
        : QUrl(QStringLiteral("https://claude.ai/api/bootstrap"));

    QNetworkRequest request(url);
    request.setRawHeader("Cookie", cookieHeader.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(30000); // 30 second timeout

    QNetworkReply *reply = networkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, cookieHeader]() {
        if (!syncReplyIsCurrent(reply)) { reply->deleteLater(); return; }
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
          recordSyncHttpFailure(
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                  .toInt(),
              reply->rawHeader("Retry-After"));
          int httpStatus =
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                  .toInt();
          qWarning() << "ClaudeCodeMonitor: Bootstrap fetch failed:"
                     << reply->errorString() << "HTTP" << httpStatus;
          setSyncing(false);
          if (httpStatus == 401 || httpStatus == 403) {
            setSyncStatus(i18n("Session expired"));
            const QString message = i18n("Session expired — please log in to "
                                         "claude.ai in Firefox again");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("session_expired"),
                                  message);
            Q_EMIT syncCompleted(false, message);
          } else {
            setSyncStatus(i18n("Sync failed"));
            const QString message = reply->errorString();
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("network_error"),
                                  message);
            Q_EMIT syncCompleted(false, message);
          }
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            setSyncing(false);
            setSyncStatus(i18n("Invalid response"));
            const QString message = i18n("Unexpected response from Claude.ai");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("invalid_response"), message);
            Q_EMIT syncCompleted(false, message);
            return;
        }

        QJsonObject root = doc.object();

        // Extract organization UUID from bootstrap response
        // Structure: { account: { memberships: [ { organization: { uuid: "..." } } ] } }
        QString orgUuid;
        QJsonObject account = root.value(QStringLiteral("account")).toObject();
        if (account.isEmpty()) {
            setSyncing(false);
            setSyncStatus(i18n("Invalid response"));
            const QString message = i18n("API response format may have changed — missing account data");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("format_changed"), message);
            Q_EMIT syncCompleted(false, message);
            return;
        }
        QJsonArray memberships = account.value(QStringLiteral("memberships")).toArray();
        QJsonObject org; // Declared in outer scope for plan detection below
        if (!memberships.isEmpty()) {
            QJsonObject firstMembership = memberships.first().toObject();
            org = firstMembership.value(QStringLiteral("organization")).toObject();
            orgUuid = org.value(QStringLiteral("uuid")).toString();
        }

        // Fallback: try top-level uuid
        if (orgUuid.isEmpty()) {
            orgUuid = account.value(QStringLiteral("uuid")).toString();
        }

        if (orgUuid.isEmpty()) {
            setSyncing(false);
            setSyncStatus(i18n("No organization"));
            const QString message = i18n("Could not find your Claude organization");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("organization_missing"), message);
            Q_EMIT syncCompleted(false, message);
            return;
        }

        m_orgUuid = orgUuid;

        // Auto-detect plan from organization subscription data
        QJsonObject subscription = org.value(QStringLiteral("subscription")).toObject();
        if (!subscription.isEmpty()) {
            QString planType = subscription.value(QStringLiteral("type")).toString();
            // Also check for rate_limit_tier as fallback
            if (planType.isEmpty()) {
                planType = org.value(QStringLiteral("rate_limit_tier")).toString();
            }

            QString detectedPlan;
            if (planType.contains(QStringLiteral("max_20x"), Qt::CaseInsensitive)
                || planType.contains(QStringLiteral("max20x"), Qt::CaseInsensitive)
                || planType == QStringLiteral("scale_max_20x")) {
                detectedPlan = QStringLiteral("Max 20x");
            } else if (planType.contains(QStringLiteral("max_5x"), Qt::CaseInsensitive)
                       || planType.contains(QStringLiteral("max5x"), Qt::CaseInsensitive)
                       || planType == QStringLiteral("scale_max_5x")) {
                detectedPlan = QStringLiteral("Max 5x");
            } else if (planType.contains(QStringLiteral("pro"), Qt::CaseInsensitive)
                       || planType == QStringLiteral("professional")) {
                detectedPlan = QStringLiteral("Pro");
            }

            if (!detectedPlan.isEmpty() && detectedPlan != planTier()) {
                qDebug() << "ClaudeCodeMonitor: Auto-detected plan:" << detectedPlan << "(raw:" << planType << ")";
                setPlanTier(detectedPlan);
                setUsageLimit(defaultLimitForPlan(detectedPlan));
                setSecondaryUsageLimit(defaultSecondaryLimitForPlan(detectedPlan));
            }
        }

        fetchUsageData(orgUuid, cookieHeader);
    });
}

void ClaudeCodeMonitor::fetchUsageData(const QString &orgUuid, const QString &cookieHeader)
{
    QString demoUrl = QString::fromLocal8Bit(qgetenv("PLASMA_AI_MONITOR_DEMO_BASE_URL")).trimmed();
    if (demoUrl.isEmpty()) {
        demoUrl = QStringLiteral("http://localhost:8080");
    }
    while (demoUrl.endsWith(QLatin1Char('/'))) {
        demoUrl.chop(1);
    }
    QUrl url = qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO")
        ? QUrl(QStringLiteral("%1/claude/api/organizations/%2/usage").arg(demoUrl, orgUuid))
        : QUrl(QStringLiteral("https://claude.ai/api/organizations/%1/usage").arg(orgUuid));

    QNetworkRequest request(url);
    request.setRawHeader("Cookie", cookieHeader.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(30000); // 30 second timeout

    QNetworkReply *reply = networkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!syncReplyIsCurrent(reply)) { reply->deleteLater(); return; }
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
          recordSyncHttpFailure(
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                  .toInt(),
              reply->rawHeader("Retry-After"));
          int httpStatus =
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                  .toInt();
          qWarning() << "ClaudeCodeMonitor: Usage fetch failed:"
                     << reply->errorString() << "HTTP" << httpStatus;
          setSyncing(false);
          if (httpStatus == 401 || httpStatus == 403) {
            setSyncStatus(i18n("Session expired"));
            const QString message = i18n("Session expired — please log in to "
                                         "claude.ai in Firefox again");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("session_expired"),
                                  message);
            Q_EMIT syncCompleted(false, message);
          } else {
            setSyncStatus(i18n("Sync failed"));
            const QString message = reply->errorString();
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("network_error"),
                                  message);
            Q_EMIT syncCompleted(false, message);
          }
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            setSyncing(false);
            setSyncStatus(i18n("Invalid response"));
            const QString message = i18n("Unexpected response from Claude.ai");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("invalid_response"), message);
            Q_EMIT syncCompleted(false, message);
            return;
        }

        QJsonObject root = doc.object();

        // Validate expected fields exist
        if (!root.contains(QStringLiteral("five_hour")) && !root.contains(QStringLiteral("seven_day"))) {
            setSyncing(false);
            setSyncStatus(i18n("Invalid response"));
            const QString message = i18n("API response format may have changed — no usage data found");
            Q_EMIT syncDiagnostic(toolName(), QStringLiteral("format_changed"), message);
            Q_EMIT syncCompleted(false, message);
            return;
        }

        QVariantList windows;
        for (const QString &key :
             {QStringLiteral("five_hour"), QStringLiteral("seven_day")}) {
          const auto window = root.value(key).toObject();
          const auto utilization = window.value(QStringLiteral("utilization"));
          if (!utilization.isDouble() || utilization.toDouble() < 0 ||
              utilization.toDouble() > 100)
            continue;
          QVariantMap row{
              {QStringLiteral("kind"), key},
              {QStringLiteral("window"), key},
              {QStringLiteral("source"), QStringLiteral("browser_sync")},
              {QStringLiteral("precision"),
               QStringLiteral("browser_sync_actual")},
              {QStringLiteral("percentUsed"), utilization.toDouble()}};
          const QDateTime reset = QDateTime::fromString(
              window.value(QStringLiteral("resets_at")).toString(),
              Qt::ISODate);
          if (reset.isValid())
            row.insert(QStringLiteral("resetAt"), reset);
          windows.append(row);
        }
        setSyncedQuotaWindows(windows);
        // Parse 5-hour session usage
        QJsonObject fiveHour = root.value(QStringLiteral("five_hour")).toObject();
        if (fiveHour.value(QStringLiteral("utilization")).isDouble() &&
            fiveHour.value(QStringLiteral("utilization")).toDouble() >= 0 &&
            fiveHour.value(QStringLiteral("utilization")).toDouble() <= 100) {
          double utilization =
              fiveHour.value(QStringLiteral("utilization")).toDouble(0.0);
          setHasSessionInfo(false);

          // Convert percentage to count based on configured limit
          int limit = usageLimit();
          if (limit > 0) {
            int used = static_cast<int>((utilization / 100.0) * limit);
            setUsageCount(used);
          }

          // Update period reset time
          QString resetsAt =
              fiveHour.value(QStringLiteral("resets_at")).toString();
          if (!resetsAt.isEmpty()) {
            QDateTime resetTime = QDateTime::fromString(resetsAt, Qt::ISODate);
            if (resetTime.isValid()) {
              // Calculate period start from reset time (reset = start + 5h)
              setPeriodStart(resetTime.addSecs(-5 * 3600));
            }
          }
        }

        // Parse 7-day (weekly) usage
        QJsonObject sevenDay = root.value(QStringLiteral("seven_day")).toObject();
        if (sevenDay.value(QStringLiteral("utilization")).isDouble() &&
            sevenDay.value(QStringLiteral("utilization")).toDouble() >= 0 &&
            sevenDay.value(QStringLiteral("utilization")).toDouble() <= 100) {
          double utilization =
              sevenDay.value(QStringLiteral("utilization")).toDouble(0.0);
          int secLimit = secondaryUsageLimit();
          if (secLimit > 0) {
            int used = static_cast<int>((utilization / 100.0) * secLimit);
            setSecondaryUsageCount(used);
          }
        }

        // Parse extra_usage (metered spending)
        QJsonValue extraVal = root.value(QStringLiteral("extra_usage"));
        if (!extraVal.isNull() && extraVal.isObject()) {
            QJsonObject extra = extraVal.toObject();
            setHasExtraUsage(true);
            double spentCents = extra.value(QStringLiteral("spent_cents")).toDouble(0);
            setExtraUsageSpent(spentCents / 100.0);
            double limitCents = extra.value(QStringLiteral("monthly_limit_cents")).toDouble(0);
            setExtraUsageLimit(limitCents / 100.0);
            QString resetsAt = extra.value(QStringLiteral("resets_at")).toString();
            if (!resetsAt.isEmpty()) {
                setExtraUsageResetDate(QDateTime::fromString(resetsAt, Qt::ISODate));
            }
        }

        // Sync complete
        setSyncing(false);
        setLastSyncTime(QDateTime::currentDateTimeUtc());
        setSyncStatus(i18n("Synced"));
        Q_EMIT syncCompleted(true, i18n("Claude usage data synced successfully"));
        Q_EMIT usageUpdated();
    });
}
