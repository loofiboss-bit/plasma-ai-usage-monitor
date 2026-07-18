#include "sourcereadinessmodel.h"

#include "providerbackend.h"
#include "providerpricingcatalog.h"
#include "subscriptionplancatalog.h"
#include "subscriptiontoolbackend.h"

#include <KLocalizedString>
#include <QSet>
#include <algorithm>

namespace {
bool isActualMetricSource(const QString &source)
{
    static const QSet<QString> actualSources{
        QStringLiteral("billing_api"),
        QStringLiteral("usage_api"),
        QStringLiteral("actual_api"),
        QStringLiteral("metrics_api"),
        QStringLiteral("browser_sync")
    };
    return actualSources.contains(source);
}

bool isEstimatedMetricSource(const QString &source)
{
    static const QSet<QString> estimatedSources{
        QStringLiteral("estimated_pricing"),
        QStringLiteral("estimated_from_usage"),
        QStringLiteral("local_observation"),
        QStringLiteral("self_tracked")
    };
    return estimatedSources.contains(source);
}

QDateTime latest(const QDateTime &first, const QDateTime &second)
{
    if (!first.isValid()) return second;
    if (!second.isValid()) return first;
    return first > second ? first : second;
}
}

SourceReadinessModel::SourceReadinessModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QVariantList providers = ProviderPricingCatalog::instance()->providers();
    for (const QVariant &value : providers) {
        const QVariantMap descriptor = value.toMap();
        SourceEntry entry;
        entry.stableId = descriptor.value(QStringLiteral("stableId")).toString();
        entry.displayName = descriptor.value(QStringLiteral("displayName")).toString();
        entry.kind = SourceKind::Provider;
        entry.monitoringLevel = descriptor.value(QStringLiteral("monitoringLevel")).toString();

        const QVariantMap auth = descriptor.value(QStringLiteral("auth")).toMap();
        const QStringList credentialSlots = auth.value(QStringLiteral("credentialSlots")).toStringList();
        for (const QString &slot : credentialSlots) {
            // AWS session tokens are optional for long-lived credentials.
            if (!slot.endsWith(QLatin1String("_session_token"))) entry.requiredCredentialSlots.append(slot);
        }

        const QVariantMap safeRefresh = descriptor.value(QStringLiteral("safeRefresh")).toMap();
        const QString method = safeRefresh.value(QStringLiteral("method")).toString().toUpper();
        entry.safeVerification = safeRefresh.value(QStringLiteral("readOnly")).toBool()
            && (method == QLatin1String("GET") || method == QLatin1String("HEAD"));
        entry.customEndpointRequired = descriptor.value(QStringLiteral("endpoint")).toMap()
            .value(QStringLiteral("customPolicy")).toString() == QLatin1String("required");
        if (!entry.stableId.isEmpty()) m_sources.append(entry);
    }

    const QVariantList tools = SubscriptionPlanCatalog::instance()->tools();
    for (const QVariant &value : tools) {
        const QVariantMap descriptor = value.toMap();
        SourceEntry entry;
        entry.stableId = descriptor.value(QStringLiteral("key")).toString();
        entry.displayName = descriptor.value(QStringLiteral("label")).toString();
        entry.kind = SourceKind::LocalTool;
        entry.monitoringLevel = QStringLiteral("local_activity_estimate");
        entry.safeVerification = true;
        if (!entry.stableId.isEmpty()) m_sources.append(entry);
    }
}

int SourceReadinessModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_sources.size();
}

QVariant SourceReadinessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sources.size()) return {};
    const SourceEntry &entry = m_sources.at(index.row());
    const Snapshot snapshot = snapshotFor(entry);

    switch (role) {
    case StableIdRole: return entry.stableId;
    case DisplayNameRole: return entry.displayName;
    case SourceKindRole: return QVariant::fromValue(entry.kind);
    case SourceKindKeyRole: return sourceKindKey(entry.kind);
    case MonitoringLevelRole: return entry.monitoringLevel;
    case RequiredCredentialSlotsRole: return entry.requiredCredentialSlots;
    case InstalledRole: return snapshot.installed;
    case EnabledRole: return snapshot.enabled;
    case LastVerifiedRole: return snapshot.lastVerified;
    case SafeVerificationRole: return entry.safeVerification;
    case CustomEndpointRequiredRole: return entry.customEndpointRequired;
    case ReadinessStateRole: return QVariant::fromValue(snapshot.state);
    case ReadinessStateKeyRole: return stateKey(snapshot.state);
    case NextActionRole: return QVariant::fromValue(snapshot.nextAction);
    case NextActionKeyRole: return nextActionKey(snapshot.nextAction);
    case NextActionTextRole: return nextActionText(snapshot.nextAction);
    case ErrorCodeRole: return snapshot.errorCode;
    case SetupRankRole: return snapshot.setupRank;
    case LastVerifiedPresentRole: return snapshot.lastVerified.isValid();
    default: return {};
    }
}

QHash<int, QByteArray> SourceReadinessModel::roleNames() const
{
    return {
        {StableIdRole, "stableId"},
        {DisplayNameRole, "displayName"},
        {SourceKindRole, "sourceKind"},
        {SourceKindKeyRole, "sourceKindKey"},
        {MonitoringLevelRole, "monitoringLevel"},
        {RequiredCredentialSlotsRole, "requiredCredentialSlots"},
        {InstalledRole, "installed"},
        {EnabledRole, "enabled"},
        {LastVerifiedRole, "lastVerified"},
        {SafeVerificationRole, "safeVerification"},
        {CustomEndpointRequiredRole, "customEndpointRequired"},
        {ReadinessStateRole, "readinessState"},
        {ReadinessStateKeyRole, "readinessStateKey"},
        {NextActionRole, "nextAction"},
        {NextActionKeyRole, "nextActionKey"},
        {NextActionTextRole, "nextActionText"},
        {ErrorCodeRole, "errorCode"},
        {SetupRankRole, "setupRank"},
        {LastVerifiedPresentRole, "lastVerifiedPresent"}
    };
}

QVariantMap SourceReadinessModel::source(const QString &stableId) const
{
    return mapForRow(rowForId(stableId));
}

QStringList SourceReadinessModel::rankedSourceIds() const
{
    QList<int> rows;
    rows.reserve(m_sources.size());
    for (int row = 0; row < m_sources.size(); ++row) rows.append(row);
    std::stable_sort(rows.begin(), rows.end(), [this](int left, int right) {
        const Snapshot leftSnapshot = snapshotFor(m_sources.at(left));
        const Snapshot rightSnapshot = snapshotFor(m_sources.at(right));
        if (leftSnapshot.setupRank != rightSnapshot.setupRank)
            return leftSnapshot.setupRank < rightSnapshot.setupRank;
        return left < right;
    });

    QStringList result;
    result.reserve(rows.size());
    for (int row : std::as_const(rows)) result.append(m_sources.at(row).stableId);
    return result;
}

void SourceReadinessModel::registerProviderBackend(const QString &stableId, QObject *backendObject)
{
    const int row = rowForId(stableId);
    auto *backend = qobject_cast<ProviderBackend *>(backendObject);
    if (row < 0 || !backend || m_sources.at(row).kind != SourceKind::Provider) return;
    if (m_sources.at(row).backend == backend) return;
    if (m_sources.at(row).backend) disconnect(m_sources.at(row).backend, nullptr, this, nullptr);
    m_sources[row].backend = backend;
    connectProvider(row, backend);
    updateRow(row);
}

void SourceReadinessModel::registerLocalTool(const QString &stableId, QObject *backendObject)
{
    const int row = rowForId(stableId);
    auto *backend = qobject_cast<SubscriptionToolBackend *>(backendObject);
    if (row < 0 || !backend || m_sources.at(row).kind != SourceKind::LocalTool) return;
    if (m_sources.at(row).backend == backend) return;
    if (m_sources.at(row).backend) disconnect(m_sources.at(row).backend, nullptr, this, nullptr);
    m_sources[row].backend = backend;
    m_sources[row].enabled = backend->isEnabled();
    connectLocalTool(row, backend);
    updateRow(row);
}

void SourceReadinessModel::setSourceEnabled(const QString &stableId, bool enabled)
{
    const int row = rowForId(stableId);
    if (row < 0 || m_sources.at(row).enabled == enabled) return;
    m_sources[row].enabled = enabled;
    updateRow(row);
}

bool SourceReadinessModel::verifySource(const QString &stableId)
{
    const int row = rowForId(stableId);
    if (row < 0) return false;

    SourceEntry &entry = m_sources[row];
    if (!entry.safeVerification || !entry.enabled) return false;

    if (entry.kind == SourceKind::Provider) {
        auto *backend = qobject_cast<ProviderBackend *>(entry.backend.data());
        if (!backend || !providerCredentialsConfigured(entry, backend)
            || (entry.customEndpointRequired && backend->customBaseUrl().isEmpty()
                && !qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO"))) {
            updateRow(row);
            return false;
        }
        return backend->requestRefresh(ProviderBackend::RefreshReason::Manual);
    }

    auto *tool = qobject_cast<SubscriptionToolBackend *>(entry.backend.data());
    if (!tool) return false;
    entry.localDiagnosticCode.clear();
    tool->checkToolInstalled();
    if (!tool->isInstalled()) {
        updateRow(row);
        return false;
    }
    tool->detectActivity();
    entry.localVerification = QDateTime::currentDateTimeUtc();
    updateRow(row);
    return true;
}

int SourceReadinessModel::rowForId(const QString &stableId) const
{
    for (int row = 0; row < m_sources.size(); ++row) {
        if (m_sources.at(row).stableId == stableId) return row;
    }
    return -1;
}

SourceReadinessModel::Snapshot SourceReadinessModel::snapshotFor(const SourceEntry &entry) const
{
    Snapshot result;
    result.enabled = entry.enabled;

    if (entry.kind == SourceKind::LocalTool) {
        const auto *tool = qobject_cast<SubscriptionToolBackend *>(entry.backend.data());
        result.installed = tool && tool->isInstalled();
        result.enabled = tool ? tool->isEnabled() : entry.enabled;
        result.setupRank = result.installed ? 0 : 300;
        if (tool) result.lastVerified = latest(latest(tool->lastSyncTime(), tool->lastActivity()),
                                               entry.localVerification);

        if (!result.enabled) {
            result.state = SourceState::Disabled;
            result.nextAction = NextAction::EnableSource;
        } else if (!result.installed) {
            result.state = SourceState::UnavailableLocally;
            result.nextAction = NextAction::InstallLocalSource;
        } else if (tool && tool->isSyncing()) {
            result.state = SourceState::Verifying;
            result.nextAction = NextAction::WaitForVerification;
        } else if (!entry.localDiagnosticCode.isEmpty()) {
            result.errorCode = entry.localDiagnosticCode;
            if (entry.localDiagnosticCode == QLatin1String("not_logged_in")) {
                result.state = SourceState::Failed;
                result.nextAction = NextAction::SignIn;
            } else if (entry.localDiagnosticCode == QLatin1String("permission_denied")) {
                result.state = SourceState::Failed;
                result.nextAction = NextAction::GrantReadOnlyPermission;
            } else if (entry.localDiagnosticCode == QLatin1String("not_supported")
                       || entry.localDiagnosticCode == QLatin1String("unsupported_metric")) {
                result.state = SourceState::Failed;
                result.nextAction = NextAction::ReviewUnsupportedMetric;
            } else if (entry.localDiagnosticCode == QLatin1String("network_error")) {
                result.state = result.lastVerified.isValid() ? SourceState::Degraded : SourceState::Failed;
                result.nextAction = NextAction::CheckNetwork;
            } else {
                result.state = SourceState::Failed;
                result.nextAction = NextAction::CompleteConfiguration;
            }
        } else if (tool && tool->lastSyncTime().isValid()) {
            result.state = SourceState::ReportingActual;
            result.nextAction = NextAction::None;
        } else if (tool && (tool->lastActivity().isValid() || tool->usageCount() > 0
                            || entry.localVerification.isValid())) {
            result.state = SourceState::ReportingEstimate;
            result.nextAction = NextAction::None;
        } else {
            result.state = SourceState::ReadyToVerify;
            result.nextAction = NextAction::VerifySource;
        }
        return result;
    }

    const auto *backend = qobject_cast<ProviderBackend *>(entry.backend.data());
    result.installed = true;
    if (entry.monitoringLevel == QLatin1String("actual_usage_spend")
        || entry.monitoringLevel == QLatin1String("actual_key_usage")
        || entry.monitoringLevel == QLatin1String("gateway_aggregate")) {
        result.setupRank = 100;
    } else {
        result.setupRank = 200;
    }

    if (!result.enabled) {
        result.state = SourceState::Disabled;
        result.nextAction = NextAction::EnableSource;
        return result;
    }

    if (!backend) {
        result.state = SourceState::Failed;
        result.errorCode = QStringLiteral("backend_unavailable");
        result.nextAction = NextAction::RetryLater;
        return result;
    }

    result.lastVerified = backend->lastSuccess();
    if (backend->isLoading() || backend->providerState() == ProviderBackend::ProviderState::Refreshing) {
        result.state = SourceState::Verifying;
        result.nextAction = NextAction::WaitForVerification;
        return result;
    }

    const ProviderBackend::ProviderErrorKind errorKind = backend->errorKind();
    if (errorKind != ProviderBackend::ProviderErrorKind::None) {
        switch (errorKind) {
        case ProviderBackend::ProviderErrorKind::Configuration:
            result.state = SourceState::NeedsConfiguration;
            result.errorCode = QStringLiteral("configuration");
            result.nextAction = NextAction::CompleteConfiguration;
            return result;
        case ProviderBackend::ProviderErrorKind::Authentication:
            result.state = SourceState::Failed;
            result.errorCode = QStringLiteral("authentication");
            result.nextAction = NextAction::ReplaceCredentials;
            return result;
        case ProviderBackend::ProviderErrorKind::Permission:
            result.state = SourceState::Failed;
            result.errorCode = QStringLiteral("permission");
            result.nextAction = NextAction::GrantReadOnlyPermission;
            return result;
        case ProviderBackend::ProviderErrorKind::Unsupported:
        case ProviderBackend::ProviderErrorKind::Schema:
            result.state = result.lastVerified.isValid() ? SourceState::Degraded : SourceState::Failed;
            result.errorCode = errorKind == ProviderBackend::ProviderErrorKind::Unsupported
                ? QStringLiteral("unsupported_metric") : QStringLiteral("schema");
            result.nextAction = NextAction::ReviewUnsupportedMetric;
            return result;
        case ProviderBackend::ProviderErrorKind::Network:
        case ProviderBackend::ProviderErrorKind::Timeout:
            result.state = result.lastVerified.isValid() ? SourceState::Degraded : SourceState::Failed;
            result.errorCode = errorKind == ProviderBackend::ProviderErrorKind::Network
                ? QStringLiteral("network") : QStringLiteral("timeout");
            result.nextAction = NextAction::CheckNetwork;
            return result;
        case ProviderBackend::ProviderErrorKind::RateLimit:
        case ProviderBackend::ProviderErrorKind::Server:
        case ProviderBackend::ProviderErrorKind::Cancelled:
            result.state = result.lastVerified.isValid() ? SourceState::Degraded : SourceState::Failed;
            result.errorCode = errorKind == ProviderBackend::ProviderErrorKind::RateLimit
                ? QStringLiteral("rate_limit")
                : errorKind == ProviderBackend::ProviderErrorKind::Server
                    ? QStringLiteral("server") : QStringLiteral("cancelled");
            result.nextAction = NextAction::RetryLater;
            return result;
        case ProviderBackend::ProviderErrorKind::None:
            break;
        }
    }

    if (backend->providerState() == ProviderBackend::ProviderState::Stale
        || backend->freshness() == ProviderBackend::Freshness::Stale) {
        result.state = SourceState::Degraded;
        result.errorCode = QStringLiteral("stale");
        result.nextAction = NextAction::RefreshStaleData;
        return result;
    }
    if (backend->providerState() == ProviderBackend::ProviderState::Degraded) {
        result.state = SourceState::Degraded;
        result.errorCode = QStringLiteral("degraded");
        result.nextAction = NextAction::RetryLater;
        return result;
    }
    if (backend->providerState() == ProviderBackend::ProviderState::Failed) {
        result.state = SourceState::Failed;
        result.errorCode = QStringLiteral("failed");
        result.nextAction = NextAction::RetryLater;
        return result;
    }

    if (!providerCredentialsConfigured(entry, backend)) {
        result.state = SourceState::NeedsConfiguration;
        result.nextAction = NextAction::AddCredentials;
        return result;
    }
    if (entry.customEndpointRequired && backend->customBaseUrl().isEmpty()
        && !qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO")) {
        result.state = SourceState::NeedsConfiguration;
        result.nextAction = NextAction::CompleteConfiguration;
        return result;
    }

    if (!backend->isConnected()) {
        result.state = SourceState::ReadyToVerify;
        result.nextAction = NextAction::VerifySource;
        return result;
    }

    bool hasActual = false;
    bool hasEstimate = false;
    for (const QVariant &value : backend->metrics()) {
        const QVariantMap metric = value.toMap();
        if (!metric.value(QStringLiteral("available")).toBool()) continue;
        const QString source = metric.value(QStringLiteral("source")).toString();
        hasActual = hasActual || isActualMetricSource(source);
        hasEstimate = hasEstimate || isEstimatedMetricSource(source);
    }
    hasActual = hasActual || isActualMetricSource(backend->usageSource())
        || isActualMetricSource(backend->costSource());
    hasEstimate = hasEstimate || backend->isEstimatedCost()
        || isEstimatedMetricSource(backend->usageSource())
        || isEstimatedMetricSource(backend->costSource());

    result.state = hasActual ? SourceState::ReportingActual
        : hasEstimate ? SourceState::ReportingEstimate
                      : SourceState::ConnectedConnectivityOnly;
    result.nextAction = NextAction::None;
    return result;
}

QVariantMap SourceReadinessModel::mapForRow(int row) const
{
    if (row < 0 || row >= m_sources.size()) return {};
    QVariantMap result;
    const QHash<int, QByteArray> names = roleNames();
    const QModelIndex modelIndex = index(row);
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        result.insert(QString::fromUtf8(it.value()), data(modelIndex, it.key()));
    return result;
}

bool SourceReadinessModel::providerCredentialsConfigured(const SourceEntry &entry,
                                                         const ProviderBackend *backend) const
{
    if (entry.requiredCredentialSlots.isEmpty()) return true;
    if (!backend->hasApiKey()) return false;
    if (entry.stableId == QLatin1String("bedrock"))
        return !backend->property("secretAccessKey").toString().isEmpty();
    return true;
}

void SourceReadinessModel::updateRow(int row)
{
    if (row < 0 || row >= m_sources.size()) return;
    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT sourceChanged(m_sources.at(row).stableId);
}

void SourceReadinessModel::connectProvider(int row, ProviderBackend *backend)
{
    const auto update = [this, row]() { updateRow(row); };
    connect(backend, &ProviderBackend::connectedChanged, this, update);
    connect(backend, &ProviderBackend::loadingChanged, this, update);
    connect(backend, &ProviderBackend::errorChanged, this, update);
    connect(backend, &ProviderBackend::stateChanged, this, update);
    connect(backend, &ProviderBackend::dataUpdated, this, update);
    connect(backend, &ProviderBackend::metricsChanged, this, update);
    connect(backend, &ProviderBackend::customBaseUrlChanged, this, update);
    if (backend->metaObject()->indexOfSignal("credentialsChanged()") >= 0)
        connect(backend, SIGNAL(credentialsChanged()), this, SLOT(backendChanged()));
    connect(backend, &QObject::destroyed, this, update);
}

void SourceReadinessModel::backendChanged()
{
    QObject *changedBackend = sender();
    for (int row = 0; row < m_sources.size(); ++row) {
        if (m_sources.at(row).backend == changedBackend) {
            updateRow(row);
            return;
        }
    }
}

void SourceReadinessModel::connectLocalTool(int row, SubscriptionToolBackend *backend)
{
    const auto update = [this, row]() {
        if (row >= 0 && row < m_sources.size()) {
            if (auto *tool = qobject_cast<SubscriptionToolBackend *>(m_sources.at(row).backend.data()))
                m_sources[row].enabled = tool->isEnabled();
        }
        updateRow(row);
    };
    connect(backend, &SubscriptionToolBackend::enabledChanged, this, update);
    connect(backend, &SubscriptionToolBackend::installedChanged, this, update);
    connect(backend, &SubscriptionToolBackend::usageUpdated, this, update);
    connect(backend, &SubscriptionToolBackend::syncStatusChanged, this, update);
    connect(backend, &SubscriptionToolBackend::syncDiagnostic, this,
            [this, row](const QString &, const QString &code, const QString &) {
                if (row < 0 || row >= m_sources.size()) return;
                m_sources[row].localDiagnosticCode = code;
                updateRow(row);
            });
    connect(backend, &SubscriptionToolBackend::syncCompleted, this,
            [this, row](bool success, const QString &) {
                if (row < 0 || row >= m_sources.size()) return;
                if (success) m_sources[row].localDiagnosticCode.clear();
                updateRow(row);
            });
    connect(backend, &QObject::destroyed, this, update);
}

QString SourceReadinessModel::sourceKindKey(SourceKind kind)
{
    return kind == SourceKind::Provider ? QStringLiteral("provider") : QStringLiteral("local_tool");
}

QString SourceReadinessModel::stateKey(SourceState state)
{
    switch (state) {
    case SourceState::Disabled: return QStringLiteral("disabled");
    case SourceState::UnavailableLocally: return QStringLiteral("unavailable_locally");
    case SourceState::NeedsConfiguration: return QStringLiteral("needs_configuration");
    case SourceState::ReadyToVerify: return QStringLiteral("ready_to_verify");
    case SourceState::Verifying: return QStringLiteral("verifying");
    case SourceState::ConnectedConnectivityOnly: return QStringLiteral("connected_connectivity_only");
    case SourceState::ReportingEstimate: return QStringLiteral("reporting_estimate");
    case SourceState::ReportingActual: return QStringLiteral("reporting_actual");
    case SourceState::Degraded: return QStringLiteral("degraded");
    case SourceState::Failed: return QStringLiteral("failed");
    }
    return {};
}

QString SourceReadinessModel::nextActionKey(NextAction action)
{
    switch (action) {
    case NextAction::None: return QStringLiteral("none");
    case NextAction::EnableSource: return QStringLiteral("enable_source");
    case NextAction::InstallLocalSource: return QStringLiteral("install_local_source");
    case NextAction::AddCredentials: return QStringLiteral("add_credentials");
    case NextAction::CompleteConfiguration: return QStringLiteral("complete_configuration");
    case NextAction::VerifySource: return QStringLiteral("verify_source");
    case NextAction::WaitForVerification: return QStringLiteral("wait_for_verification");
    case NextAction::SignIn: return QStringLiteral("sign_in");
    case NextAction::ReplaceCredentials: return QStringLiteral("replace_credentials");
    case NextAction::GrantReadOnlyPermission: return QStringLiteral("grant_read_only_permission");
    case NextAction::ReviewUnsupportedMetric: return QStringLiteral("review_unsupported_metric");
    case NextAction::RefreshStaleData: return QStringLiteral("refresh_stale_data");
    case NextAction::CheckNetwork: return QStringLiteral("check_network");
    case NextAction::RetryLater: return QStringLiteral("retry_later");
    }
    return {};
}

QString SourceReadinessModel::nextActionText(NextAction action)
{
    switch (action) {
    case NextAction::None: return i18n("No action required");
    case NextAction::EnableSource: return i18n("Enable this source");
    case NextAction::InstallLocalSource: return i18n("Install the local tool, then check again");
    case NextAction::AddCredentials: return i18n("Add the required credential");
    case NextAction::CompleteConfiguration: return i18n("Complete the required configuration");
    case NextAction::VerifySource: return i18n("Run the safe read-only verification");
    case NextAction::WaitForVerification: return i18n("Wait for verification to finish");
    case NextAction::SignIn: return i18n("Sign in to this source");
    case NextAction::ReplaceCredentials: return i18n("Replace or verify the credential");
    case NextAction::GrantReadOnlyPermission: return i18n("Grant the required read-only permission");
    case NextAction::ReviewUnsupportedMetric: return i18n("Review which metrics this source supports");
    case NextAction::RefreshStaleData: return i18n("Refresh the stale data");
    case NextAction::CheckNetwork: return i18n("Check the network connection and retry");
    case NextAction::RetryLater: return i18n("Retry the verification later");
    }
    return {};
}
