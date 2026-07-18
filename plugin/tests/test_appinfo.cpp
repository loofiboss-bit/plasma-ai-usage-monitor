#include <QtTest>

#include "appinfo.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace
{
void writeMetadata(const QString &dataRoot, const QString &version)
{
    const QString directory =
        QDir(dataRoot).filePath(QStringLiteral("plasma/plasmoids/com.github.loofi.aiusagemonitor"));
    QVERIFY(QDir().mkpath(directory));
    QFile file(QDir(directory).filePath(QStringLiteral("metadata.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QJsonObject metadata{
        {QStringLiteral("KPlugin"), QJsonObject{{QStringLiteral("Version"), version}}}};
    QCOMPARE(file.write(QJsonDocument(metadata).toJson()), QJsonDocument(metadata).toJson().size());
}
} // namespace

class AppInfoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void supportReportSnapshot();
    void supportReportRejectsSensitiveValues();
    void installationInspectionDetectsShadowingAndMismatch();
    void installationInspectionClassifiesMissingPlugin();
    void databaseInspectionIsReadOnlyAndTyped();
};

void AppInfoTest::supportReportSnapshot()
{
    const QVariantMap system{
        {QStringLiteral("frontendVersion"), QStringLiteral("14.0.0")},
        {QStringLiteral("nativePluginVersion"), QStringLiteral("13.0.0")},
        {QStringLiteral("nativePluginPath"),
         QStringLiteral("$HOME/.local/lib64/qt6/qml/libaiusagemonitorplugin.so")},
        {QStringLiteral("nativeStatus"), QStringLiteral("version_mismatch")},
        {QStringLiteral("frontendLayer"), QStringLiteral("user-local")},
        {QStringLiteral("pluginLayer"), QStringLiteral("system")},
        {QStringLiteral("shadowing"), true},
        {QStringLiteral("plasmaVersion"), QStringLiteral("plasmashell 6.4.5")},
        {QStringLiteral("distribution"),
         QStringLiteral("Fedora Linux 44 (KDE Plasma Desktop Edition)")},
        {QStringLiteral("nextStep"),
         QStringLiteral("Update the frontend and native plugin together, then "
                        "restart Plasma.")},
        {QStringLiteral("repairCommand"),
         QStringLiteral("sudo dnf upgrade --refresh plasma-ai-usage-monitor")}};
    const QVariantList sources{
        QVariantMap{{QStringLiteral("stableId"), QStringLiteral("openai")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("installed"), true},
                    {QStringLiteral("readinessStateKey"), QStringLiteral("failed")},
                    {QStringLiteral("errorCode"), QStringLiteral("authentication")},
                    {QStringLiteral("nextActionKey"), QStringLiteral("replace_credentials")},
                    {QStringLiteral("lastVerifiedPresent"), false}},
        QVariantMap{{QStringLiteral("stableId"), QStringLiteral("anthropic")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("installed"), true},
                    {QStringLiteral("readinessStateKey"), QStringLiteral("failed")},
                    {QStringLiteral("errorCode"), QStringLiteral("permission")},
                    {QStringLiteral("nextActionKey"), QStringLiteral("grant_read_only_permission")},
                    {QStringLiteral("lastVerifiedPresent"), true}},
        QVariantMap{{QStringLiteral("stableId"), QStringLiteral("google")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("installed"), true},
                    {QStringLiteral("readinessStateKey"), QStringLiteral("degraded")},
                    {QStringLiteral("errorCode"), QStringLiteral("unsupported_metric")},
                    {QStringLiteral("nextActionKey"), QStringLiteral("review_unsupported_metric")},
                    {QStringLiteral("lastVerifiedPresent"), true}},
        QVariantMap{{QStringLiteral("stableId"), QStringLiteral("xai")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("installed"), true},
                    {QStringLiteral("readinessStateKey"), QStringLiteral("degraded")},
                    {QStringLiteral("errorCode"), QStringLiteral("stale")},
                    {QStringLiteral("nextActionKey"), QStringLiteral("refresh_stale_data")},
                    {QStringLiteral("lastVerifiedPresent"), true}}};
    const QVariantMap runtime{
        {QStringLiteral("walletOpen"), false},
        {QStringLiteral("database"),
         QVariantMap{{QStringLiteral("status"), QStringLiteral("integrity_failed")},
                     {QStringLiteral("sizeBytes"), 4096}}},
        {QStringLiteral("providerCatalogVersion"), QStringLiteral("4")},
        {QStringLiteral("subscriptionCatalogVersion"), QStringLiteral("1")},
        {QStringLiteral("sources"), sources}};

    const QString expected =
        QStringLiteral("Plasma AI Usage Monitor Support Report\n"
                       "Frontend version: 14.0.0\n"
                       "Native plugin: 13.0.0 (version_mismatch)\n"
                       "Native plugin path: "
                       "$HOME/.local/lib64/qt6/qml/libaiusagemonitorplugin.so\n"
                       "Install layers: frontend=user-local plugin=system shadowing=yes\n"
                       "Plasma: plasmashell 6.4.5\n"
                       "Distribution: Fedora Linux 44 (KDE Plasma Desktop Edition)\n"
                       "KWallet open: no\n"
                       "Database: integrity_failed size_bytes=4096\n"
                       "Provider catalog: 4\n"
                       "Subscription catalog: 1\n"
                       "Source readiness:\n"
                       "- openai enabled=yes installed=yes state=failed error=authentication "
                       "next=replace_credentials verified=no\n"
                       "- anthropic enabled=yes installed=yes state=failed error=permission "
                       "next=grant_read_only_permission verified=yes\n"
                       "- google enabled=yes installed=yes state=degraded "
                       "error=unsupported_metric next=review_unsupported_metric verified=yes\n"
                       "- xai enabled=yes installed=yes state=degraded error=stale "
                       "next=refresh_stale_data verified=yes\n"
                       "Recovery: Update the frontend and native plugin together, then restart "
                       "Plasma.\n"
                       "Repair command: sudo dnf upgrade --refresh plasma-ai-usage-monitor\n"
                       "Sensitive endpoints, identifiers, credentials, cookies, webhooks, and "
                       "wallet contents are omitted.");
    QCOMPARE(AppInfo::formatSupportReport(system, runtime), expected);
}

void AppInfoTest::supportReportRejectsSensitiveValues()
{
    const QString apiKey = QStringLiteral("sk-live-DO-NOT-LEAK");
    const QString endpoint = QStringLiteral("https://user:pass@private.example/v1?token=secret");
    const QString account = QStringLiteral("account-private-123");
    const QString project = QStringLiteral("project-private-456");
    const QString cookie = QStringLiteral("session=private-cookie");
    const QString webhook = QStringLiteral("https://hooks.example/private-webhook");
    const QVariantMap system{
        {QStringLiteral("frontendVersion"), QStringLiteral("14.0.0")},
        {QStringLiteral("nativePluginVersion"), QStringLiteral("14.0.0")},
        {QStringLiteral("nativePluginPath"),
         QStringLiteral("/usr/lib64/qt6/qml/libaiusagemonitorplugin.so")},
        {QStringLiteral("nativeStatus"), QStringLiteral("ready")},
        {QStringLiteral("frontendLayer"), QStringLiteral("system")},
        {QStringLiteral("pluginLayer"), QStringLiteral("system")},
        {QStringLiteral("plasmaVersion"), QStringLiteral("plasmashell 6")},
        {QStringLiteral("distribution"), QStringLiteral("Fedora Linux")},
        {QStringLiteral("nextStep"), QStringLiteral("No installation repair is required.")}};
    const QVariantMap runtime{
        {QStringLiteral("walletOpen"), true},
        {QStringLiteral("database"), QVariantMap{{QStringLiteral("status"), QStringLiteral("ok")}}},
        {QStringLiteral("providerCatalogVersion"), QStringLiteral("4")},
        {QStringLiteral("subscriptionCatalogVersion"), QStringLiteral("1")},
        {QStringLiteral("apiKey"), apiKey},
        {QStringLiteral("endpoint"), endpoint},
        {QStringLiteral("accountId"), account},
        {QStringLiteral("projectId"), project},
        {QStringLiteral("cookie"), cookie},
        {QStringLiteral("webhookUrl"), webhook},
        {QStringLiteral("sources"),
         QVariantList{QVariantMap{{QStringLiteral("stableId"), apiKey},
                                  {QStringLiteral("errorCode"), endpoint}},
                      QVariantMap{{QStringLiteral("stableId"), QStringLiteral("openai")},
                                  {QStringLiteral("enabled"), true},
                                  {QStringLiteral("installed"), true},
                                  {QStringLiteral("readinessStateKey"), QStringLiteral("failed")},
                                  {QStringLiteral("errorCode"), account},
                                  {QStringLiteral("nextActionKey"), project},
                                  {QStringLiteral("lastVerifiedPresent"), false}}}}};

    const QString report = AppInfo::formatSupportReport(system, runtime);
    for (const QString &secret : {apiKey, endpoint, account, project, cookie, webhook})
        QVERIFY2(!report.contains(secret), qPrintable(secret));
    QVERIFY(report.contains(QStringLiteral("- openai")));
    QVERIFY(report.contains(QStringLiteral("error=unknown")));
}

void AppInfoTest::installationInspectionDetectsShadowingAndMismatch()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString userRoot = QDir(root.path()).filePath(QStringLiteral("home/.local/share"));
    const QString systemRoot = QDir(root.path()).filePath(QStringLiteral("usr/share"));
    writeMetadata(userRoot, QStringLiteral("14.0.0"));
    writeMetadata(systemRoot, QStringLiteral("13.0.0"));

    const QVariantMap result = AppInfo::inspectInstallation(
        QStringLiteral("14.0.0"), userRoot, {systemRoot},
        QStringLiteral("/usr/lib64/qt6/qml/com/github/loofi/aiusagemonitor/"
                       "libaiusagemonitorplugin.so"),
        QStringLiteral("13.0.0"));
    QCOMPARE(result.value(QStringLiteral("frontendLayer")).toString(),
             QStringLiteral("user-local"));
    QCOMPARE(result.value(QStringLiteral("pluginLayer")).toString(), QStringLiteral("system"));
    QVERIFY(result.value(QStringLiteral("shadowing")).toBool());
    QCOMPARE(result.value(QStringLiteral("nativeStatus")).toString(),
             QStringLiteral("version_mismatch"));
    QCOMPARE(result.value(QStringLiteral("repairCommand")).toString(),
             QStringLiteral("sudo dnf upgrade --refresh plasma-ai-usage-monitor"));
}

void AppInfoTest::installationInspectionClassifiesMissingPlugin()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString systemRoot = QDir(root.path()).filePath(QStringLiteral("usr/share"));
    writeMetadata(systemRoot, QStringLiteral("14.0.0"));

    const QVariantMap result = AppInfo::inspectInstallation(
        QStringLiteral("14.0.0"), QDir(root.path()).filePath(QStringLiteral("home/.local/share")),
        {systemRoot}, QString(), QString());
    QCOMPARE(result.value(QStringLiteral("nativeStatus")).toString(), QStringLiteral("missing"));
    QVERIFY(!result.value(QStringLiteral("nativeAvailable")).toBool());
    QCOMPARE(result.value(QStringLiteral("repairCommand")).toString(),
             QStringLiteral("sudo dnf install plasma-ai-usage-monitor"));
}

void AppInfoTest::databaseInspectionIsReadOnlyAndTyped()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString databasePath = QDir(root.path()).filePath(QStringLiteral("history.db"));
    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE observations (id INTEGER PRIMARY KEY)")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);

    const qint64 before = QFileInfo(databasePath).size();
    const QVariantMap healthy = AppInfo::inspectDatabase(databasePath);
    QCOMPARE(healthy.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    QCOMPARE(QFileInfo(databasePath).size(), before);

    const QString corruptPath = QDir(root.path()).filePath(QStringLiteral("corrupt.db"));
    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QVERIFY(corrupt.write("not a sqlite database") > 0);
    corrupt.close();
    const QVariantMap broken = AppInfo::inspectDatabase(corruptPath);
    const QStringList failureStates{QStringLiteral("open_failed"),
                                    QStringLiteral("integrity_failed")};
    QVERIFY(failureStates.contains(broken.value(QStringLiteral("status")).toString()));
}

QTEST_MAIN(AppInfoTest)
#include "test_appinfo.moc"
