#include "antigravitymonitor.h"

#include "antigravity_local.pb.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <utility>

namespace
{
using ProtoResponse = exa::language_server_pb::GetUserStatusResponse;
using ProtoPlanInfo = exa::language_server_pb::PlanInfo;

QString fromProto(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString slug(const QString &value)
{
    QString result = value.toLower().trimmed();
    result.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    result.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return result.isEmpty() ? QStringLiteral("unknown") : result;
}

QString modelFamily(const QString &label, int provider)
{
    const QString normalized = label.toLower();
    if (provider == 4 || normalized.startsWith(QLatin1String("gemini")))
        return QStringLiteral("google");
    if (provider == 3 || normalized.startsWith(QLatin1String("claude")))
        return QStringLiteral("anthropic");
    if (provider == 2 || normalized.startsWith(QLatin1String("gpt")) || normalized.startsWith(QLatin1String("openai")))
    {
        return QStringLiteral("openai");
    }
    return QStringLiteral("antigravity");
}

int familyRank(const QString &family)
{
    return family == QLatin1String("google") ? 0 : 1;
}

QString untilReset(const QDateTime &resetAt)
{
    if (!resetAt.isValid())
        return {};
    const qint64 seconds = QDateTime::currentDateTimeUtc().secsTo(resetAt);
    if (seconds <= 0)
        return QStringLiteral("now");
    const qint64 days = seconds / 86400;
    const qint64 hours = (seconds % 86400) / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (days > 0)
        return QStringLiteral("%1d %2h").arg(days).arg(hours);
    if (hours > 0)
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    return QStringLiteral("%1m").arg(minutes);
}

const ProtoPlanInfo *reportedPlan(const ProtoResponse &response)
{
    if (response.has_user_status())
    {
        const auto &status = response.user_status();
        if (status.has_plan_status() && status.plan_status().has_plan_info() &&
            !status.plan_status().plan_info().plan_name().empty())
        {
            return &status.plan_status().plan_info();
        }
        if (status.has_plan_info() && !status.plan_info().plan_name().empty())
            return &status.plan_info();
    }
    if (response.has_plan_info())
        return &response.plan_info();
    return nullptr;
}

QString safeNetworkMessage(QNetworkReply::NetworkError error)
{
    switch (error)
    {
    case QNetworkReply::TimeoutError:
        return i18n("Antigravity did not respond before the timeout.");
    case QNetworkReply::SslHandshakeFailedError:
        return i18n("Antigravity's local TLS connection could not be verified.");
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
        return i18n("The Antigravity daemon stopped before quota could be read.");
    default:
        return i18n("The local Antigravity quota request failed.");
    }
}
} // namespace

AntigravityMonitor::AntigravityMonitor(QObject *parent) : SubscriptionToolBackend(parent)
{
    setSyncEnabled(true);
    setSyncStatus(QStringLiteral("idle"));
}

QStringList AntigravityMonitor::availablePlans() const
{
    return catalogPlanLabels();
}

int AntigravityMonitor::defaultLimitForPlan(const QString &) const
{
    return 0;
}

double AntigravityMonitor::percentUsed() const
{
    return m_maximumPercentUsed;
}

bool AntigravityMonitor::isLimitReached() const
{
    return m_maximumPercentUsed >= 100.0;
}

void AntigravityMonitor::checkToolInstalled()
{
    const bool installed = !QStandardPaths::findExecutable(QStringLiteral("antigravity")).isEmpty() ||
                           !QStandardPaths::findExecutable(QStringLiteral("agy")).isEmpty() ||
                           QFileInfo::exists(QStringLiteral("/usr/share/antigravity/resources/app/extensions/"
                                                            "antigravity/bin/language_server_linux_x64"));
    setInstalled(installed);
    if (!installed)
        setConnectionState(QStringLiteral("unavailable"), QStringLiteral("not_installed"));
}

void AntigravityMonitor::detectActivity()
{
    refreshQuota();
}

void AntigravityMonitor::syncFromBrowser(const QString &cookieHeader, int browserType)
{
    Q_UNUSED(cookieHeader);
    Q_UNUSED(browserType);
    refreshQuota();
}

QByteArray AntigravityMonitor::grpcFrame(const QByteArray &payload)
{
    QByteArray frame(5, '\0');
    const quint32 size = static_cast<quint32>(payload.size());
    frame[1] = static_cast<char>((size >> 24) & 0xff);
    frame[2] = static_cast<char>((size >> 16) & 0xff);
    frame[3] = static_cast<char>((size >> 8) & 0xff);
    frame[4] = static_cast<char>(size & 0xff);
    frame.append(payload);
    return frame;
}

QByteArray AntigravityMonitor::firstGrpcMessage(const QByteArray &body, QString *error)
{
    if (error)
        error->clear();
    if (body.size() < 5)
    {
        if (error)
            *error = QStringLiteral("missing_frame");
        return {};
    }
    if (static_cast<unsigned char>(body.at(0)) != 0)
    {
        if (error)
            *error = QStringLiteral("compressed_frame");
        return {};
    }
    const quint32 size = (static_cast<quint32>(static_cast<unsigned char>(body.at(1))) << 24) |
                         (static_cast<quint32>(static_cast<unsigned char>(body.at(2))) << 16) |
                         (static_cast<quint32>(static_cast<unsigned char>(body.at(3))) << 8) |
                         static_cast<quint32>(static_cast<unsigned char>(body.at(4)));
    if (size > 16 * 1024 * 1024 || body.size() < 5 + static_cast<qsizetype>(size))
    {
        if (error)
            *error = QStringLiteral("invalid_frame_size");
        return {};
    }
    return body.mid(5, static_cast<qsizetype>(size));
}

QString AntigravityMonitor::normalizedPlanId(const QString &planName, const QString &tierId, bool enterprise)
{
    const QString text = (tierId + QLatin1Char(' ') + planName).toLower();
    if (enterprise || text.contains(QLatin1String("enterprise")))
        return QStringLiteral("enterprise");
    if (text.contains(QLatin1String("ultra")) && text.contains(QLatin1String("20")))
        return QStringLiteral("ultra_20x");
    if (text.contains(QLatin1String("ultra")) && text.contains(QLatin1String("5")))
        return QStringLiteral("ultra_5x");
    if (text.contains(QLatin1String("ultra")))
        return QStringLiteral("ultra");
    if (text.contains(QLatin1String("pro")))
        return QStringLiteral("pro");
    if (text.contains(QLatin1String("standard")) || text.contains(QLatin1String("free")) ||
        text.contains(QLatin1String("individual")))
    {
        return QStringLiteral("standard");
    }
    return QStringLiteral("unknown");
}

QVariantMap AntigravityMonitor::parseUserStatusPayload(const QByteArray &payload)
{
    QVariantMap result;
    ProtoResponse response;
    if (!response.ParseFromArray(payload.constData(), static_cast<int>(payload.size())))
    {
        result.insert(QStringLiteral("error"), QStringLiteral("invalid_response"));
        return result;
    }
    if (!response.has_user_status())
    {
        result.insert(QStringLiteral("error"), QStringLiteral("not_signed_in"));
        return result;
    }

    const auto &status = response.user_status();
    QString tierId;
    QString tierName;
    if (status.has_user_tier())
    {
        tierId = fromProto(status.user_tier().id());
        tierName = fromProto(status.user_tier().name());
    }
    const ProtoPlanInfo *plan = reportedPlan(response);
    const QString planName = plan ? fromProto(plan->plan_name()) : QString();
    const bool enterprise = plan && plan->is_enterprise();
    const QString planId = normalizedPlanId(planName, tierId, enterprise);
    QString planLabel = tierName.isEmpty() ? planName : tierName;
    if (planLabel.compare(QLatin1String("Pro"), Qt::CaseInsensitive) == 0)
        planLabel = QStringLiteral("Google AI Pro");
    if (planLabel.compare(QLatin1String("Ultra"), Qt::CaseInsensitive) == 0)
        planLabel = QStringLiteral("Google AI Ultra");
    if (planId == QLatin1String("standard") &&
        (planLabel.compare(QLatin1String("Standard"), Qt::CaseInsensitive) == 0 ||
         planLabel.compare(QLatin1String("Free"), Qt::CaseInsensitive) == 0))
        planLabel = QStringLiteral("Google AI Standard");
    if (planId == QLatin1String("ultra_5x"))
        planLabel = QStringLiteral("Google AI Ultra 5x");
    if (planId == QLatin1String("ultra_20x"))
        planLabel = QStringLiteral("Google AI Ultra 20x");
    if (planLabel.isEmpty())
        planLabel = QStringLiteral("Unknown plan");

    QList<QVariantMap> normalizedRows;
    if (status.has_cascade_model_config_data())
    {
        const auto &configs = status.cascade_model_config_data().client_model_configs();
        normalizedRows.reserve(configs.size());
        QSet<QString> seen;
        for (const auto &config : configs)
        {
            const QString label = fromProto(config.label()).trimmed();
            if (label.isEmpty())
                continue;

            int model = 0;
            int alias = 0;
            if (config.has_model_or_alias())
            {
                model = config.model_or_alias().model();
                alias = config.model_or_alias().alias();
            }
            const QString identity = model > 0   ? QStringLiteral("model:%1").arg(model)
                                     : alias > 0 ? QStringLiteral("alias:%1").arg(alias)
                                                 : QStringLiteral("label:%1").arg(slug(label));
            const QString deduplicationKey = identity + QLatin1Char('|') + label;
            if (seen.contains(deduplicationKey))
                continue;
            seen.insert(deduplicationKey);

            const QString family = modelFamily(label, config.provider());
            QVariantMap row;
            row.insert(QStringLiteral("kind"), QStringLiteral("model_quota"));
            row.insert(QStringLiteral("label"), label);
            row.insert(QStringLiteral("modelId"), identity);
            row.insert(QStringLiteral("modelFamily"), family);
            row.insert(QStringLiteral("availability"),
                       config.disabled() ? QStringLiteral("disabled") : QStringLiteral("available"));
            row.insert(QStringLiteral("unit"), QStringLiteral("percent_remaining"));
            row.insert(QStringLiteral("source"), QStringLiteral("antigravity_local"));
            row.insert(QStringLiteral("visibleByDefault"), true);
            if (config.disabled())
            {
                const QString reason = config.description().empty() ? fromProto(config.tag_description())
                                                                    : fromProto(config.description());
                row.insert(QStringLiteral("availabilityReason"), reason);
                row.insert(QStringLiteral("badge"), QStringLiteral("Unavailable"));
            }

            bool hasExactFraction = false;
            if (config.has_quota_info())
            {
                const auto &quota = config.quota_info();
                if (quota.has_remaining_fraction() && std::isfinite(quota.remaining_fraction()) &&
                    quota.remaining_fraction() >= 0.0f && quota.remaining_fraction() <= 1.0f)
                {
                    const double remaining = static_cast<double>(quota.remaining_fraction()) * 100.0;
                    row.insert(QStringLiteral("percentRemaining"), remaining);
                    row.insert(QStringLiteral("percentUsed"), 100.0 - remaining);
                    hasExactFraction = true;
                }
                if (quota.has_reset_time() && quota.reset_time().seconds() > 0)
                {
                    const QDateTime resetAt =
                        QDateTime::fromSecsSinceEpoch(quota.reset_time().seconds(), QTimeZone::UTC);
                    row.insert(QStringLiteral("resetAt"), resetAt.toString(Qt::ISODate));
                    row.insert(QStringLiteral("timeUntilReset"), untilReset(resetAt));
                }
            }
            row.insert(QStringLiteral("precision"),
                       hasExactFraction ? QStringLiteral("local_daemon_actual") : QStringLiteral("availability_only"));
            if (!row.contains(QStringLiteral("badge")))
            {
                row.insert(QStringLiteral("badge"),
                           hasExactFraction ? QStringLiteral("Live") : QStringLiteral("Available"));
            }
            normalizedRows.append(row);
        }
    }

    if (normalizedRows.isEmpty())
    {
        result.insert(QStringLiteral("error"), tierId.isEmpty() && planName.isEmpty()
                                                   ? QStringLiteral("not_signed_in")
                                                   : QStringLiteral("unsupported_version"));
        return result;
    }

    std::stable_sort(normalizedRows.begin(), normalizedRows.end(),
                     [](const QVariantMap &left, const QVariantMap &right) {
                         const int leftRank = familyRank(left.value(QStringLiteral("modelFamily")).toString());
                         const int rightRank = familyRank(right.value(QStringLiteral("modelFamily")).toString());
                         if (leftRank != rightRank)
                             return leftRank < rightRank;
                         return QString::localeAwareCompare(left.value(QStringLiteral("label")).toString(),
                                                            right.value(QStringLiteral("label")).toString()) < 0;
                     });

    QVariantList rows;
    double maximumUsed = 0.0;
    for (const QVariantMap &row : std::as_const(normalizedRows))
    {
        rows.append(row);
        if (row.contains(QStringLiteral("percentUsed")))
            maximumUsed = qMax(maximumUsed, row.value(QStringLiteral("percentUsed")).toDouble());
    }
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("planId"), planId);
    result.insert(QStringLiteral("planLabel"), planLabel);
    result.insert(QStringLiteral("rows"), rows);
    result.insert(QStringLiteral("maximumPercentUsed"), maximumUsed);
    return result;
}

bool AntigravityMonitor::isLoopbackHost(const QString &host)
{
    const QString normalized = host.trimmed().toLower();
    if (normalized == QLatin1String("localhost"))
        return true;
    const QHostAddress address(normalized);
    return !address.isNull() && address.isLoopback();
}

bool AntigravityMonitor::validateDiscoveryFile(const QString &path, quint32 expectedOwner, QVariantMap *document,
                                               QString *error)
{
    if (error)
        error->clear();
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isSymLink())
    {
        if (error)
            *error = QStringLiteral("not_regular_file");
        return false;
    }
    if (info.ownerId() != expectedOwner)
    {
        if (error)
            *error = QStringLiteral("wrong_owner");
        return false;
    }
    const QFileDevice::Permissions permissions = info.permissions();
    if (permissions.testFlag(QFileDevice::WriteGroup) || permissions.testFlag(QFileDevice::WriteOther))
    {
        if (error)
            *error = QStringLiteral("unsafe_permissions");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024)
    {
        if (error)
            *error = QStringLiteral("unreadable");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject())
    {
        if (error)
            *error = QStringLiteral("invalid_json");
        return false;
    }
    const QVariantMap object = json.object().toVariantMap();
    const QString host = object.value(QStringLiteral("host"), QStringLiteral("127.0.0.1")).toString();
    const qint64 pid = object.value(QStringLiteral("pid")).toLongLong();
    const int port = object.value(QStringLiteral("httpsPort")).toInt();
    const QString token = object.value(QStringLiteral("csrfToken")).toString();
    if (!isLoopbackHost(host) || pid <= 0 || port <= 0 || port > 65535 || token.isEmpty())
    {
        if (error)
            *error = QStringLiteral("invalid_endpoint");
        return false;
    }
    if (document)
        *document = object;
    return true;
}

QString AntigravityMonitor::argumentValue(const QList<QByteArray> &arguments, const QByteArray &name)
{
    for (qsizetype index = 0; index + 1 < arguments.size(); ++index)
    {
        if (arguments.at(index) == name)
            return QString::fromUtf8(arguments.at(index + 1));
    }
    return {};
}

QString AntigravityMonitor::validatedLanguageServerExecutable(qint64 pid, quint32 expectedOwner)
{
    const QFileInfo processInfo(QStringLiteral("/proc/%1").arg(pid));
    if (!processInfo.exists() || processInfo.ownerId() != expectedOwner || ::kill(static_cast<pid_t>(pid), 0) != 0)
        return {};
    const QString executable = QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
    if (!executable.contains(QLatin1String("/extensions/antigravity/bin/language_server_")))
        return {};
    return QFileInfo(executable).canonicalFilePath();
}

QString AntigravityMonitor::certificateForExecutable(const QString &executable)
{
    if (executable.isEmpty())
        return {};
    const QDir binDirectory = QFileInfo(executable).absoluteDir();
    const QString certificate =
        QFileInfo(binDirectory.filePath(QStringLiteral("../dist/languageServer/cert.pem"))).canonicalFilePath();
    return QFileInfo(certificate).isFile() ? certificate : QString();
}

QList<quint16> AntigravityMonitor::listeningLoopbackPorts(qint64 pid)
{
    QSet<quint64> socketInodes;
    const QDir fdDirectory(QStringLiteral("/proc/%1/fd").arg(pid));
    const QStringList descriptors = fdDirectory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
    // QFileInfo expands relative procfs link targets to an absolute-looking
    // path (for example /proc/123/fd/socket:[456]), so match the stable suffix.
    const QRegularExpression socketPattern(QStringLiteral("socket:\\[(\\d+)\\]$"));
    for (const QString &descriptor : descriptors)
    {
        const QByteArray encodedPath = QFile::encodeName(fdDirectory.filePath(descriptor));
        QByteArray linkTarget(256, '\0');
        const ssize_t length =
            ::readlink(encodedPath.constData(), linkTarget.data(), static_cast<size_t>(linkTarget.size()));
        if (length <= 0)
            continue;
        linkTarget.truncate(static_cast<qsizetype>(length));
        const QString target = QString::fromLatin1(linkTarget);
        const QRegularExpressionMatch match = socketPattern.match(target);
        if (match.hasMatch())
            socketInodes.insert(match.captured(1).toULongLong());
    }
    QSet<quint16> ports;
    const auto readTable = [&socketInodes, &ports](const QString &path, bool ipv6) {
        QFile table(path);
        if (!table.open(QIODevice::ReadOnly))
            return;
        // procfs reports a zero file size, so atEnd() is true before the first
        // read. Read until readLine() itself returns empty instead.
        while (true)
        {
            const QByteArray line = table.readLine();
            if (line.isEmpty())
                break;
            const QList<QByteArray> fields = line.simplified().split(' ');
            bool inodeOk = false;
            const quint64 inode = fields.size() >= 10 ? fields.at(9).toULongLong(&inodeOk) : 0;
            if (fields.size() < 10 || fields.at(3) != "0A" || !inodeOk || !socketInodes.contains(inode))
                continue;
            const QList<QByteArray> local = fields.at(1).split(':');
            if (local.size() != 2)
                continue;
            const QByteArray address = local.at(0).toUpper();
            const bool loopback = ipv6 ? address == "00000000000000000000000001000000" : address == "0100007F";
            bool ok = false;
            const uint port = local.at(1).toUInt(&ok, 16);
            if (loopback && ok && port > 0 && port <= 65535)
                ports.insert(static_cast<quint16>(port));
        }
    };
    readTable(QStringLiteral("/proc/net/tcp"), false);
    readTable(QStringLiteral("/proc/net/tcp6"), true);
    QList<quint16> result = ports.values();
    std::sort(result.begin(), result.end());
    return result;
}

QList<AntigravityMonitor::Endpoint> AntigravityMonitor::endpointsFromDiscoveryFiles() const
{
    QList<Endpoint> endpoints;
    const quint32 owner = static_cast<quint32>(::geteuid());
    const QDir gemini(QDir(QDir::homePath()).filePath(QStringLiteral(".gemini")));
    const QStringList appDataDirectories = gemini.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &appDataDirectory : appDataDirectories)
    {
        const QDir daemon(gemini.filePath(appDataDirectory + QStringLiteral("/daemon")));
        const QFileInfoList files = daemon.entryInfoList({QStringLiteral("ls_*.json")}, QDir::Files, QDir::Time);
        for (const QFileInfo &file : files)
        {
            QVariantMap object;
            if (!validateDiscoveryFile(file.absoluteFilePath(), owner, &object))
                continue;
            const qint64 pid = object.value(QStringLiteral("pid")).toLongLong();
            const QString executable = validatedLanguageServerExecutable(pid, owner);
            const QString certificate = certificateForExecutable(executable);
            const quint16 port = static_cast<quint16>(object.value(QStringLiteral("httpsPort")).toUInt());
            if (certificate.isEmpty() || !listeningLoopbackPorts(pid).contains(port))
                continue;
            Endpoint endpoint;
            endpoint.pid = pid;
            endpoint.host = object.value(QStringLiteral("host"), QStringLiteral("127.0.0.1")).toString();
            endpoint.port = port;
            endpoint.csrfToken = object.value(QStringLiteral("csrfToken")).toString();
            endpoint.certificatePath = certificate;
            endpoint.serverVersion = object.value(QStringLiteral("lsVersion")).toString();
            endpoints.append(endpoint);
        }
    }
    return endpoints;
}

QList<AntigravityMonitor::Endpoint> AntigravityMonitor::endpointsFromProcesses() const
{
    QList<Endpoint> endpoints;
    const quint32 owner = static_cast<quint32>(::geteuid());
    const QDir proc(QStringLiteral("/proc"));
    const QStringList processes = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : processes)
    {
        bool numeric = false;
        const qint64 pid = name.toLongLong(&numeric);
        if (!numeric || pid <= 0)
            continue;
        const QString executable = validatedLanguageServerExecutable(pid, owner);
        if (executable.isEmpty())
            continue;

        QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(pid));
        if (!cmdline.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> arguments = cmdline.readAll().split('\0');
        const QString csrfToken = argumentValue(arguments, QByteArrayLiteral("--csrf_token"));
        const QString certificate = certificateForExecutable(executable);
        if (csrfToken.isEmpty() || certificate.isEmpty())
            continue;
        for (quint16 port : listeningLoopbackPorts(pid))
        {
            Endpoint endpoint;
            endpoint.pid = pid;
            endpoint.port = port;
            endpoint.csrfToken = csrfToken;
            endpoint.certificatePath = certificate;
            endpoints.append(endpoint);
        }
    }
    return endpoints;
}

QList<AntigravityMonitor::Endpoint> AntigravityMonitor::discoverEndpoints(QString *errorCode) const
{
    if (errorCode)
        errorCode->clear();
    QList<Endpoint> endpoints = endpointsFromDiscoveryFiles();
    const QList<Endpoint> processEndpoints = endpointsFromProcesses();
    QSet<QString> seen;
    QList<Endpoint> unique;
    for (const Endpoint &endpoint : endpoints + processEndpoints)
    {
        const QString key = QStringLiteral("%1:%2").arg(endpoint.host).arg(endpoint.port);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        unique.append(endpoint);
    }
    if (unique.isEmpty() && errorCode)
        *errorCode = QStringLiteral("daemon_not_running");
    return unique;
}

void AntigravityMonitor::refreshQuota()
{
    if (isSyncing())
        return;
    checkToolInstalled();
    if (!isInstalled())
    {
        finishFailure(QStringLiteral("not_installed"), i18n("Google Antigravity is not installed."));
        return;
    }

    setSyncing(true);
    setSyncStatus(i18n("Reading Antigravity quota…"));
    setConnectionState(QStringLiteral("connecting"), QString());
    m_pendingEndpoints.clear();
    m_lastAttemptCode.clear();
    m_lastAttemptMessage.clear();
    QString discoveryError;
    const QList<Endpoint> endpoints = discoverEndpoints(&discoveryError);
    for (const Endpoint &endpoint : endpoints)
        m_pendingEndpoints.enqueue(endpoint);
    if (m_pendingEndpoints.isEmpty())
    {
        finishFailure(discoveryError.isEmpty() ? QStringLiteral("daemon_not_running") : discoveryError,
                      i18n("Start Antigravity and sign in, then retry."));
        return;
    }
    tryNextEndpoint();
}

void AntigravityMonitor::tryNextEndpoint()
{
    if (m_pendingEndpoints.isEmpty())
    {
        finishFailure(m_lastAttemptCode.isEmpty() ? QStringLiteral("invalid_response") : m_lastAttemptCode,
                      m_lastAttemptMessage.isEmpty() ? i18n("Antigravity did not return usable quota data.")
                                                     : m_lastAttemptMessage);
        return;
    }
    requestEndpoint(m_pendingEndpoints.dequeue());
}

void AntigravityMonitor::requestEndpoint(const Endpoint &endpoint)
{
    QFile certificateFile(endpoint.certificatePath);
    if (!certificateFile.open(QIODevice::ReadOnly))
    {
        m_lastAttemptCode = QStringLiteral("tls_error");
        m_lastAttemptMessage = i18n("Antigravity's local certificate could not be read.");
        tryNextEndpoint();
        return;
    }
    const QSslCertificate certificate(certificateFile.readAll(), QSsl::Pem);
    if (certificate.isNull())
    {
        m_lastAttemptCode = QStringLiteral("tls_error");
        m_lastAttemptMessage = i18n("Antigravity's local certificate is invalid.");
        tryNextEndpoint();
        return;
    }

    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates({certificate});
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    ssl.setProtocol(QSsl::TlsV1_2OrLater);

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(endpoint.host);
    url.setPort(endpoint.port);
    url.setPath(QStringLiteral("/exa.language_server_pb.LanguageServerService/GetUserStatus"));
    QNetworkRequest request(url);
    request.setSslConfiguration(ssl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/grpc+proto"));
    request.setRawHeader("Accept", "application/grpc+proto");
    request.setRawHeader("TE", "trailers");
    request.setRawHeader("grpc-accept-encoding", "identity");
    request.setRawHeader("x-codeium-csrf-token", endpoint.csrfToken.toUtf8());
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(5000);

    QNetworkReply *reply = networkManager()->post(request, grpcFrame({}));
    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError> &) {
        // Never ignore certificate failures; remember only the error class.
        reply->setProperty("antigravityTlsError", true);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            const bool tlsError = reply->property("antigravityTlsError").toBool() ||
                                  reply->error() == QNetworkReply::SslHandshakeFailedError;
            m_lastAttemptCode = tlsError                                        ? QStringLiteral("tls_error")
                                : reply->error() == QNetworkReply::TimeoutError ? QStringLiteral("timeout")
                                                                                : QStringLiteral("daemon_not_running");
            m_lastAttemptMessage = safeNetworkMessage(reply->error());
            tryNextEndpoint();
            return;
        }

        QString frameError;
        const QByteArray payload = firstGrpcMessage(reply->readAll(), &frameError);
        if (!frameError.isEmpty())
        {
            m_lastAttemptCode = QStringLiteral("invalid_response");
            m_lastAttemptMessage = i18n("Antigravity returned an unsupported local protocol response.");
            tryNextEndpoint();
            return;
        }
        const QVariantMap parsed = parseUserStatusPayload(payload);
        if (!parsed.value(QStringLiteral("ok")).toBool())
        {
            m_lastAttemptCode = parsed.value(QStringLiteral("error"), QStringLiteral("invalid_response")).toString();
            m_lastAttemptMessage = m_lastAttemptCode == QLatin1String("not_signed_in")
                                       ? i18n("Sign in to Antigravity, then retry.")
                                   : m_lastAttemptCode == QLatin1String("unsupported_version")
                                       ? i18n("This Antigravity version does not expose compatible "
                                              "model quota data.")
                                       : i18n("Antigravity returned invalid quota data.");
            tryNextEndpoint();
            return;
        }
        finishSuccess(parsed);
    });
}

void AntigravityMonitor::finishSuccess(const QVariantMap &parsed)
{
    const QVariantList rows = parsed.value(QStringLiteral("rows")).toList();
    setSyncedQuotaWindows(rows);
    setPlanTier(parsed.value(QStringLiteral("planId"), QStringLiteral("unknown")).toString());
    m_detectedPlanLabel = parsed.value(QStringLiteral("planLabel"), QStringLiteral("Unknown plan")).toString();
    m_maximumPercentUsed = parsed.value(QStringLiteral("maximumPercentUsed")).toDouble();
    setLastSyncTime(QDateTime::currentDateTimeUtc());
    setLastActivity(lastSyncTime());
    setSyncing(false);
    setSyncStatus(i18n("Synced %1 models", rows.size()));
    setConnectionState(QStringLiteral("connected"), QString());
    updateAggregateWarning(m_maximumPercentUsed);
    Q_EMIT antigravityStatusChanged();
    Q_EMIT usageUpdated();
    Q_EMIT syncCompleted(true, i18n("Antigravity quota synced successfully."));
}

void AntigravityMonitor::finishFailure(const QString &code, const QString &message)
{
    setSyncing(false);
    const bool stale = lastSyncTime().isValid();
    setSyncStatus(stale ? i18n("Stale — %1", message) : message);
    setConnectionState(stale ? QStringLiteral("stale") : QStringLiteral("failed"), code);
    Q_EMIT syncDiagnostic(toolName(), code, message);
    Q_EMIT syncCompleted(false, message);
}

void AntigravityMonitor::updateAggregateWarning(double maximumPercentUsed)
{
    const int nextBand = maximumPercentUsed >= criticalThreshold()  ? 2
                         : maximumPercentUsed >= warningThreshold() ? 1
                                                                    : 0;
    if (nextBand > m_warningBand)
    {
        Q_EMIT limitWarning(toolName(), qRound(maximumPercentUsed));
        if (maximumPercentUsed >= 100.0)
            Q_EMIT usageLimitReached(toolName());
    }
    m_warningBand = nextBand;
}

void AntigravityMonitor::setConnectionState(const QString &state, const QString &readinessCode)
{
    if (m_connectionState == state && m_readinessCode == readinessCode)
        return;
    m_connectionState = state;
    m_readinessCode = readinessCode;
    Q_EMIT antigravityStatusChanged();
}
