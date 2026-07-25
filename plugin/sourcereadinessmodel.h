#ifndef SOURCEREADINESSMODEL_H
#define SOURCEREADINESSMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QPointer>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class ProviderBackend;
class SubscriptionToolBackend;

class SourceReadinessModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount CONSTANT)

public:
    enum class SourceKind { Provider, LocalTool };
    Q_ENUM(SourceKind)

    enum class SourceState {
        Disabled,
        UnavailableLocally,
        NeedsConfiguration,
        ReadyToVerify,
        Verifying,
        ConnectedConnectivityOnly,
        ReportingEstimate,
        ReportingActual,
        Degraded,
        Failed
    };
    Q_ENUM(SourceState)

    enum class NextAction {
        None,
        EnableSource,
        InstallLocalSource,
        AddCredentials,
        CompleteConfiguration,
        VerifySource,
        WaitForVerification,
        SignIn,
        ReplaceCredentials,
        GrantReadOnlyPermission,
        ReviewUnsupportedMetric,
        RefreshStaleData,
        CheckNetwork,
        RetryLater
    };
    Q_ENUM(NextAction)

    enum Role {
        StableIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        SourceKindRole,
        SourceKindKeyRole,
        MonitoringLevelRole,
        RequiredCredentialSlotsRole,
        InstalledRole,
        EnabledRole,
        LastVerifiedRole,
        SafeVerificationRole,
        CustomEndpointRequiredRole,
        ReadinessStateRole,
        ReadinessStateKeyRole,
        NextActionRole,
        NextActionKeyRole,
        NextActionTextRole,
        ErrorCodeRole,
        SetupRankRole,
        LastVerifiedPresentRole
    };
    Q_ENUM(Role)

    explicit SourceReadinessModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap source(const QString &stableId) const;
    Q_INVOKABLE QStringList rankedSourceIds() const;
    Q_INVOKABLE void registerProviderBackend(const QString &stableId, QObject *backend);
    Q_INVOKABLE void registerLocalTool(const QString &stableId, QObject *backend);
    Q_INVOKABLE void setSourceEnabled(const QString &stableId, bool enabled);
    Q_INVOKABLE bool verifySource(const QString &stableId);

Q_SIGNALS:
    void sourceChanged(const QString &stableId);

private Q_SLOTS:
    void backendChanged();

private:
    struct SourceEntry {
        QString stableId;
        QString displayName;
        SourceKind kind = SourceKind::Provider;
        QString monitoringLevel;
        QStringList requiredCredentialSlots;
        bool safeVerification = false;
        bool customEndpointRequired = false;
        bool enabled = false;
        QString localDiagnosticCode;
        QDateTime localVerification;
        QPointer<QObject> backend;
    };

    struct Snapshot {
        bool installed = true;
        bool enabled = false;
        QDateTime lastVerified;
        SourceState state = SourceState::Disabled;
        NextAction nextAction = NextAction::None;
        QString errorCode;
        int setupRank = 0;
    };

    int rowForId(const QString &stableId) const;
    Snapshot snapshotFor(const SourceEntry &entry) const;
    QVariantMap mapForRow(int row) const;
    bool providerCredentialsConfigured(const SourceEntry &entry, const ProviderBackend *backend) const;
    void updateRow(int row);
    void connectProvider(int row, ProviderBackend *backend);
    void connectLocalTool(int row, SubscriptionToolBackend *backend);

    static QString sourceKindKey(SourceKind kind);
    static QString stateKey(SourceState state);
    static QString nextActionKey(NextAction action);
    static QString nextActionText(NextAction action);

    QList<SourceEntry> m_sources;
};

#endif // SOURCEREADINESSMODEL_H
