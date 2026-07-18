#include "appinfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QUrl>
#include <QUuid>

#include <KLocalizedString>

#ifdef Q_OS_UNIX
#include <dlfcn.h>
#endif

#ifndef AIUSAGE_MONITOR_VERSION
#define AIUSAGE_MONITOR_VERSION "0.0.0"
#endif

namespace
{
constexpr auto PlasmoidId = "com.github.loofi.aiusagemonitor";

void pluginLocationAnchor() {}

QString metadataVersion(const QString &dataRoot)
{
    const QString path = QDir(dataRoot).filePath(
        QStringLiteral("plasma/plasmoids/%1/metadata.json").arg(QString::fromLatin1(PlasmoidId)));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return {};
    const QJsonObject root = document.object();
    const QJsonObject plugin = root.value(QStringLiteral("KPlugin")).toObject();
    return plugin.value(QStringLiteral("Version"))
        .toString(root.value(QStringLiteral("Version")).toString())
        .trimmed();
}

QString firstSystemVersion(const QStringList &roots)
{
    for (const QString &root : roots)
    {
        const QString version = metadataVersion(root);
        if (!version.isEmpty())
            return version;
    }
    return {};
}

QString nativeLibraryPath()
{
#ifdef Q_OS_UNIX
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&pluginLocationAnchor), &info) != 0 && info.dli_fname)
    {
        const QFileInfo file(QString::fromLocal8Bit(info.dli_fname));
        const QString canonical = file.canonicalFilePath();
        return canonical.isEmpty() ? file.absoluteFilePath() : canonical;
    }
#endif
    return QCoreApplication::applicationFilePath();
}

QString redactHome(QString path)
{
    path = QDir::cleanPath(path);
    const QString home = QDir::cleanPath(QDir::homePath());
    if (!home.isEmpty() && (path == home || path.startsWith(home + QLatin1Char('/'))))
    {
        path.replace(0, home.size(), QStringLiteral("$HOME"));
    }
    return path;
}

QString commandVersion(const QString &executableName)
{
    const QString executable = QStandardPaths::findExecutable(executableName);
    if (executable.isEmpty())
        return QStringLiteral("unavailable");

    QProcess process;
    process.start(executable, {QStringLiteral("--version")}, QIODevice::ReadOnly);
    if (!process.waitForStarted(500) || !process.waitForFinished(1500))
    {
        process.kill();
        process.waitForFinished(200);
        return QStringLiteral("unavailable");
    }
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    if (output.isEmpty())
        output = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    return output.section(QLatin1Char('\n'), 0, 0).trimmed();
}

QString safeScalar(const QVariant &value, const QString &fallback = QStringLiteral("unknown"))
{
    QString text = value.toString().trimmed();
    text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    if (text.isEmpty())
        return fallback;
    return text.left(160);
}

QString knownValue(const QVariant &value, const QSet<QString> &allowed,
                   const QString &fallback = QStringLiteral("unknown"))
{
    const QString candidate = value.toString().trimmed();
    return allowed.contains(candidate) ? candidate : fallback;
}

const QSet<QString> &sourceIds()
{
    static const QSet<QString> values = {QStringLiteral("openai"),
                                         QStringLiteral("anthropic"),
                                         QStringLiteral("google"),
                                         QStringLiteral("mistral"),
                                         QStringLiteral("deepseek"),
                                         QStringLiteral("groq"),
                                         QStringLiteral("xai"),
                                         QStringLiteral("ollama"),
                                         QStringLiteral("openrouter"),
                                         QStringLiteral("together"),
                                         QStringLiteral("cohere"),
                                         QStringLiteral("googleveo"),
                                         QStringLiteral("azure"),
                                         QStringLiteral("bedrock"),
                                         QStringLiteral("litellm"),
                                         QStringLiteral("cerebras"),
                                         QStringLiteral("fireworks"),
                                         QStringLiteral("perplexity"),
                                         QStringLiteral("claude-code"),
                                         QStringLiteral("codex-cli"),
                                         QStringLiteral("github-copilot"),
                                         QStringLiteral("cursor"),
                                         QStringLiteral("windsurf"),
                                         QStringLiteral("jetbrains-ai")};
    return values;
}

const QSet<QString> &readinessStates()
{
    static const QSet<QString> values = {QStringLiteral("disabled"),
                                         QStringLiteral("unavailable_locally"),
                                         QStringLiteral("needs_configuration"),
                                         QStringLiteral("ready_to_verify"),
                                         QStringLiteral("verifying"),
                                         QStringLiteral("connected_connectivity_only"),
                                         QStringLiteral("reporting_estimate"),
                                         QStringLiteral("reporting_actual"),
                                         QStringLiteral("degraded"),
                                         QStringLiteral("failed")};
    return values;
}

const QSet<QString> &errorKinds()
{
    static const QSet<QString> values = {QString(),
                                         QStringLiteral("backend_unavailable"),
                                         QStringLiteral("configuration"),
                                         QStringLiteral("authentication"),
                                         QStringLiteral("permission"),
                                         QStringLiteral("permission_denied"),
                                         QStringLiteral("unsupported_metric"),
                                         QStringLiteral("not_supported"),
                                         QStringLiteral("schema"),
                                         QStringLiteral("network"),
                                         QStringLiteral("network_error"),
                                         QStringLiteral("timeout"),
                                         QStringLiteral("rate_limit"),
                                         QStringLiteral("server"),
                                         QStringLiteral("cancelled"),
                                         QStringLiteral("stale"),
                                         QStringLiteral("degraded"),
                                         QStringLiteral("failed"),
                                         QStringLiteral("not_logged_in"),
                                         QStringLiteral("session_expired"),
                                         QStringLiteral("invalid_response"),
                                         QStringLiteral("format_changed"),
                                         QStringLiteral("organization_missing"),
                                         QStringLiteral("no_live_quota")};
    return values;
}

const QSet<QString> &nextActions()
{
    static const QSet<QString> values = {QStringLiteral("none"),
                                         QStringLiteral("enable_source"),
                                         QStringLiteral("install_local_source"),
                                         QStringLiteral("add_credentials"),
                                         QStringLiteral("complete_configuration"),
                                         QStringLiteral("verify_source"),
                                         QStringLiteral("wait_for_verification"),
                                         QStringLiteral("sign_in"),
                                         QStringLiteral("replace_credentials"),
                                         QStringLiteral("grant_read_only_permission"),
                                         QStringLiteral("review_unsupported_metric"),
                                         QStringLiteral("refresh_stale_data"),
                                         QStringLiteral("check_network"),
                                         QStringLiteral("retry_later")};
    return values;
}
} // namespace

AppInfo::AppInfo(QObject *parent) : QObject(parent) {}

QString AppInfo::version() const { return QStringLiteral(AIUSAGE_MONITOR_VERSION); }

QString AppInfo::pluginPath() const { return nativeLibraryPath(); }

bool AppInfo::demoMode() const { return qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO"); }

QString AppInfo::smokeView() const
{
    return qEnvironmentVariable("PLASMA_AI_MONITOR_SMOKE_VIEW").trimmed().toLower();
}

QVariantMap AppInfo::inspectInstallation(const QString &frontendVersion,
                                         const QString &userDataRoot,
                                         const QStringList &systemDataRoots,
                                         const QString &nativePluginPath,
                                         const QString &nativePluginVersion)
{
    const QString userVersion = metadataVersion(userDataRoot);
    const QString systemVersion = firstSystemVersion(systemDataRoots);
    const bool hasUserFrontend = !userVersion.isEmpty();
    const bool hasSystemFrontend = !systemVersion.isEmpty();

    QString frontendLayer = QStringLiteral("unknown");
    if (hasUserFrontend)
        frontendLayer = QStringLiteral("user-local");
    else if (hasSystemFrontend)
        frontendLayer = QStringLiteral("system");

    const QString cleanPluginPath = QDir::cleanPath(nativePluginPath);
    QString pluginLayer = QStringLiteral("unknown");
    const QString cleanUserRoot = QDir::cleanPath(userDataRoot);
    if (!cleanUserRoot.isEmpty() &&
        cleanPluginPath.startsWith(QFileInfo(cleanUserRoot).absolutePath()))
    {
        pluginLayer = QStringLiteral("user-local");
    }
    else if (cleanPluginPath.contains(QStringLiteral("/build/")))
    {
        pluginLayer = QStringLiteral("build-tree");
    }
    else if (cleanPluginPath.startsWith(QStringLiteral("/usr/")))
    {
        pluginLayer = QStringLiteral("system");
    }

    const bool nativeAvailable =
        !nativePluginVersion.trimmed().isEmpty() && !nativePluginPath.trimmed().isEmpty();
    const bool versionMatch = nativeAvailable && !frontendVersion.trimmed().isEmpty() &&
                              frontendVersion.trimmed() == nativePluginVersion.trimmed();
    const bool shadowing = hasUserFrontend && hasSystemFrontend;

    QString nativeStatus;
    QString nextStep;
    QString repairCommand;
    if (!nativeAvailable)
    {
        nativeStatus = QStringLiteral("missing");
        nextStep = i18n("Install the complete Fedora package, then restart Plasma.");
        repairCommand = QStringLiteral("sudo dnf install plasma-ai-usage-monitor");
    }
    else if (!versionMatch)
    {
        nativeStatus = QStringLiteral("version_mismatch");
        nextStep = i18n("Update the frontend and native plugin together, then restart Plasma.");
        repairCommand = QStringLiteral("sudo dnf upgrade --refresh plasma-ai-usage-monitor");
    }
    else if (shadowing)
    {
        nativeStatus = QStringLiteral("shadowed");
        nextStep = i18n("Remove the user-local widget so the system package is "
                        "selected, then restart Plasma.");
        repairCommand = QStringLiteral("kpackagetool6 --type Plasma/Applet "
                                       "--remove com.github.loofi.aiusagemonitor");
    }
    else
    {
        nativeStatus = QStringLiteral("ready");
        nextStep = i18n("No installation repair is required.");
    }

    return {{QStringLiteral("frontendVersion"), frontendVersion.trimmed()},
            {QStringLiteral("nativePluginVersion"), nativePluginVersion.trimmed()},
            {QStringLiteral("nativePluginPath"), redactHome(nativePluginPath)},
            {QStringLiteral("frontendLayer"), frontendLayer},
            {QStringLiteral("pluginLayer"), pluginLayer},
            {QStringLiteral("userFrontendVersion"), userVersion},
            {QStringLiteral("systemFrontendVersion"), systemVersion},
            {QStringLiteral("nativeAvailable"), nativeAvailable},
            {QStringLiteral("versionMatch"), versionMatch},
            {QStringLiteral("shadowing"), shadowing},
            {QStringLiteral("nativeStatus"), nativeStatus},
            {QStringLiteral("nextStep"), nextStep},
            {QStringLiteral("repairCommand"), repairCommand}};
}

QVariantMap AppInfo::systemDiagnostics(const QString &frontendVersion) const
{
    const QString userRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QStringList systemRoots =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    systemRoots.removeAll(userRoot);
    QVariantMap result =
        inspectInstallation(frontendVersion, userRoot, systemRoots, pluginPath(), version());
    result.insert(QStringLiteral("plasmaVersion"), commandVersion(QStringLiteral("plasmashell")));
    result.insert(QStringLiteral("distribution"), QSysInfo::prettyProductName());
    return result;
}

QVariantMap AppInfo::inspectDatabase(const QString &databasePath)
{
    const QFileInfo database(databasePath);
    qint64 totalSize = database.exists() ? database.size() : 0;
    totalSize += QFileInfo(databasePath + QStringLiteral("-wal")).size();
    totalSize += QFileInfo(databasePath + QStringLiteral("-shm")).size();

    QVariantMap result{{QStringLiteral("path"), redactHome(databasePath)},
                       {QStringLiteral("sizeBytes"), totalSize},
                       {QStringLiteral("status"), QStringLiteral("not_created")}};
    if (!database.exists())
        return result;
    if (!database.isReadable())
    {
        result.insert(QStringLiteral("status"), QStringLiteral("unreadable"));
        return result;
    }

    const QString connection = QStringLiteral("aiusagemonitor_diagnostics_%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString status = QStringLiteral("open_failed");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000"));
        db.setDatabaseName(databasePath);
        if (db.open())
        {
            QSqlQuery query(db);
            status = query.exec(QStringLiteral("PRAGMA quick_check(1)")) && query.next() &&
                             query.value(0).toString() == QLatin1String("ok")
                         ? QStringLiteral("ok")
                         : QStringLiteral("integrity_failed");
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    result.insert(QStringLiteral("status"), status);
    return result;
}

QVariantMap AppInfo::databaseDiagnostics() const
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                             .filePath(QStringLiteral("plasma-ai-usage-monitor/usage_history.db"));
    return inspectDatabase(path);
}

QString AppInfo::formatSupportReport(const QVariantMap &systemContext,
                                     const QVariantMap &runtimeContext)
{
    const QVariantMap database = runtimeContext.value(QStringLiteral("database")).toMap();
    QStringList lines{
        QStringLiteral("Plasma AI Usage Monitor Support Report"),
        QStringLiteral("Frontend version: %1")
            .arg(safeScalar(systemContext.value(QStringLiteral("frontendVersion")))),
        QStringLiteral("Native plugin: %1 (%2)")
            .arg(safeScalar(systemContext.value(QStringLiteral("nativePluginVersion"))),
                 knownValue(systemContext.value(QStringLiteral("nativeStatus")),
                            {QStringLiteral("ready"), QStringLiteral("missing"),
                             QStringLiteral("version_mismatch"), QStringLiteral("shadowed")})),
        QStringLiteral("Native plugin path: %1")
            .arg(safeScalar(systemContext.value(QStringLiteral("nativePluginPath")))),
        QStringLiteral("Install layers: frontend=%1 plugin=%2 shadowing=%3")
            .arg(knownValue(systemContext.value(QStringLiteral("frontendLayer")),
                            {QStringLiteral("user-local"), QStringLiteral("system"),
                             QStringLiteral("unknown")}),
                 knownValue(systemContext.value(QStringLiteral("pluginLayer")),
                            {QStringLiteral("user-local"), QStringLiteral("system"),
                             QStringLiteral("build-tree"), QStringLiteral("unknown")}),
                 systemContext.value(QStringLiteral("shadowing")).toBool() ? QStringLiteral("yes")
                                                                           : QStringLiteral("no")),
        QStringLiteral("Plasma: %1")
            .arg(safeScalar(systemContext.value(QStringLiteral("plasmaVersion")))),
        QStringLiteral("Distribution: %1")
            .arg(safeScalar(systemContext.value(QStringLiteral("distribution")))),
        QStringLiteral("KWallet open: %1")
            .arg(runtimeContext.value(QStringLiteral("walletOpen")).toBool()
                     ? QStringLiteral("yes")
                     : QStringLiteral("no")),
        QStringLiteral("Database: %1 size_bytes=%2")
            .arg(knownValue(database.value(QStringLiteral("status")),
                            {QStringLiteral("ok"), QStringLiteral("not_created"),
                             QStringLiteral("unreadable"), QStringLiteral("open_failed"),
                             QStringLiteral("integrity_failed")}),
                 QString::number(
                     qMax<qint64>(0, database.value(QStringLiteral("sizeBytes")).toLongLong()))),
        QStringLiteral("Provider catalog: %1")
            .arg(safeScalar(runtimeContext.value(QStringLiteral("providerCatalogVersion")))),
        QStringLiteral("Subscription catalog: %1")
            .arg(safeScalar(runtimeContext.value(QStringLiteral("subscriptionCatalogVersion")))),
        QStringLiteral("Source readiness:")};

    const QVariantList sources = runtimeContext.value(QStringLiteral("sources")).toList();
    for (const QVariant &value : sources)
    {
        const QVariantMap source = value.toMap();
        const QString id = knownValue(source.value(QStringLiteral("stableId")), sourceIds());
        if (id == QLatin1String("unknown"))
            continue;
        const QString state =
            knownValue(source.value(QStringLiteral("readinessStateKey")), readinessStates());
        const QString rawError = source.value(QStringLiteral("errorCode")).toString().trimmed();
        const QString error =
            rawError.isEmpty() ? QStringLiteral("none") : knownValue(rawError, errorKinds());
        const QString action =
            knownValue(source.value(QStringLiteral("nextActionKey")), nextActions());
        lines.append(
            QStringLiteral("- %1 enabled=%2 installed=%3 state=%4 error=%5 next=%6 "
                           "verified=%7")
                .arg(id,
                     source.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("yes")
                                                                      : QStringLiteral("no"),
                     source.value(QStringLiteral("installed")).toBool() ? QStringLiteral("yes")
                                                                        : QStringLiteral("no"),
                     state, error, action,
                     source.value(QStringLiteral("lastVerifiedPresent")).toBool()
                         ? QStringLiteral("yes")
                         : QStringLiteral("no")));
    }
    if (sources.isEmpty())
        lines.append(QStringLiteral("- unavailable"));

    lines.append(QStringLiteral("Recovery: %1")
                     .arg(safeScalar(systemContext.value(QStringLiteral("nextStep")))));
    const QString repairCommand =
        safeScalar(systemContext.value(QStringLiteral("repairCommand")), QString());
    if (!repairCommand.isEmpty())
        lines.append(QStringLiteral("Repair command: %1").arg(repairCommand));
    lines.append(QStringLiteral("Sensitive endpoints, identifiers, credentials, cookies, "
                                "webhooks, and wallet contents are omitted."));
    return lines.join(QLatin1Char('\n'));
}

QString AppInfo::buildSupportReport(const QString &frontendVersion,
                                    const QVariantMap &runtimeContext) const
{
    QVariantMap context = runtimeContext;
    context.insert(QStringLiteral("database"), databaseDiagnostics());
    return formatSupportReport(systemDiagnostics(frontendVersion), context);
}

bool AppInfo::exportConfig(const QString &jsonConfig, const QString &filePath) const
{
    QString localPath = filePath;
    if (localPath.startsWith("file://"))
    {
        localPath = QUrl(filePath).toLocalFile();
    }
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << jsonConfig;
    return true;
}

QString AppInfo::importConfig(const QString &filePath) const
{
    QString localPath = filePath;
    if (localPath.startsWith("file://"))
    {
        localPath = QUrl(filePath).toLocalFile();
    }
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    return QString(file.readAll());
}
