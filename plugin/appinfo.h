#ifndef APPINFO_H
#define APPINFO_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class AppInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString pluginPath READ pluginPath CONSTANT)
    Q_PROPERTY(bool demoMode READ demoMode CONSTANT)
    Q_PROPERTY(QString smokeView READ smokeView CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    QString version() const;
    QString pluginPath() const;
    bool demoMode() const;
    QString smokeView() const;

    Q_INVOKABLE QVariantMap systemDiagnostics(const QString &frontendVersion) const;
    Q_INVOKABLE QVariantMap databaseDiagnostics() const;
    Q_INVOKABLE QString buildSupportReport(const QString &frontendVersion,
                                           const QVariantMap &runtimeContext) const;

    Q_INVOKABLE bool exportConfig(const QString &jsonConfig, const QString &filePath) const;
    Q_INVOKABLE QString importConfig(const QString &filePath) const;

    static QVariantMap inspectInstallation(const QString &frontendVersion,
                                           const QString &userDataRoot,
                                           const QStringList &systemDataRoots,
                                           const QString &nativePluginPath,
                                           const QString &nativePluginVersion);
    static QVariantMap inspectDatabase(const QString &databasePath);
    static QString formatSupportReport(const QVariantMap &systemContext,
                                       const QVariantMap &runtimeContext);
};

#endif // APPINFO_H
