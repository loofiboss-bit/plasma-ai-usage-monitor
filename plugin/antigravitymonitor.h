#ifndef ANTIGRAVITYMONITOR_H
#define ANTIGRAVITYMONITOR_H

#include "subscriptiontoolbackend.h"

#include <QByteArray>
#include <QQueue>
#include <QVariantMap>

class QNetworkReply;

/**
 * Read-only monitor for the local Google Antigravity subscription session.
 *
 * Antigravity does not expose a public quota API. A running Linux desktop
 * client does expose an authenticated, loopback-only language-server RPC.
 * This monitor calls only GetUserStatus and keeps the daemon CSRF token in
 * memory for the duration of the request.
 */
class AntigravityMonitor : public SubscriptionToolBackend
{
    Q_OBJECT
    Q_PROPERTY(QString detectedPlanLabel READ detectedPlanLabel NOTIFY antigravityStatusChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY antigravityStatusChanged)
    Q_PROPERTY(QString readinessCode READ readinessCode NOTIFY antigravityStatusChanged)
    Q_PROPERTY(QDateTime lastSuccessfulRefresh READ lastSuccessfulRefresh NOTIFY antigravityStatusChanged)
    Q_PROPERTY(QString syncSourceLabel READ syncSourceLabel CONSTANT)

  public:
    explicit AntigravityMonitor(QObject *parent = nullptr);

    QString toolName() const override
    {
        return QStringLiteral("Google Antigravity");
    }
    QString iconName() const override
    {
        return QStringLiteral("google-antigravity");
    }
    QString toolColor() const override
    {
        return QStringLiteral("#4285F4");
    }
    QString periodLabel() const override
    {
        return QStringLiteral("Model quota");
    }

    QString detectedPlanLabel() const
    {
        return m_detectedPlanLabel;
    }
    QString connectionState() const
    {
        return m_connectionState;
    }
    QString readinessCode() const
    {
        return m_readinessCode;
    }
    QDateTime lastSuccessfulRefresh() const
    {
        return lastSyncTime();
    }
    QString syncSourceLabel() const
    {
        return QStringLiteral("Antigravity local");
    }

    QStringList availablePlans() const override;
    int defaultLimitForPlan(const QString &plan) const override;
    double percentUsed() const override;
    bool isLimitReached() const override;

    Q_INVOKABLE void checkToolInstalled() override;
    Q_INVOKABLE void detectActivity() override;
    Q_INVOKABLE void refreshQuota();
    Q_INVOKABLE void syncFromBrowser(const QString &cookieHeader, int browserType) override;

    // Pure helpers used by deterministic protocol and security tests.
    static QByteArray grpcFrame(const QByteArray &payload);
    static QByteArray firstGrpcMessage(const QByteArray &body, QString *error = nullptr);
    static QVariantMap parseUserStatusPayload(const QByteArray &payload);
    static QVariantMap parseConnectUserStatusPayload(const QByteArray &payload);
    static QVariantMap parseQuotaSummaryPayload(const QByteArray &payload);
    static QString normalizedPlanId(const QString &planName, const QString &tierId, bool enterprise);
    static bool isLoopbackHost(const QString &host);
    static bool isSupportedLanguageServerPath(const QString &path);
    static bool validateDiscoveryFile(const QString &path, quint32 expectedOwner, QVariantMap *document = nullptr,
                                      QString *error = nullptr);

  Q_SIGNALS:
    void antigravityStatusChanged();

  protected:
    UsagePeriod primaryPeriodType() const override
    {
        return FiveHour;
    }
    QString catalogToolKey() const override
    {
        return QStringLiteral("google-antigravity");
    }
    void finishFailure(const QString &code, const QString &message);

  private:
    struct Endpoint
    {
        qint64 pid = 0;
        QString host = QStringLiteral("127.0.0.1");
        quint16 port = 0;
        QString csrfToken;
        QByteArray certificateData;
        QString serverVersion;
    };

    QList<Endpoint> discoverEndpoints(QString *errorCode) const;
    QList<Endpoint> endpointsFromDiscoveryFiles() const;
    QList<Endpoint> endpointsFromProcesses() const;
    static QString validatedLanguageServerExecutable(qint64 pid, quint32 expectedOwner);
    static QList<quint16> listeningLoopbackPorts(qint64 pid);
    static QByteArray certificateForExecutable(const QString &executable);
    static QString argumentValue(const QList<QByteArray> &arguments, const QByteArray &name);

    void tryNextEndpoint();
    void requestEndpoint(const Endpoint &endpoint);
    void requestQuotaSummary(const Endpoint &endpoint, const QVariantMap &userStatus);
    void finishSuccess(const QVariantMap &parsed);
    void updateAggregateWarning(double maximumPercentUsed);
    void setConnectionState(const QString &state, const QString &readinessCode);

    QString m_detectedPlanLabel;
    QString m_connectionState = QStringLiteral("idle");
    QString m_readinessCode;
    QQueue<Endpoint> m_pendingEndpoints;
    QString m_lastAttemptCode;
    QString m_lastAttemptMessage;
    double m_maximumPercentUsed = 0.0;
    int m_warningBand = 0;
};

#endif // ANTIGRAVITYMONITOR_H
