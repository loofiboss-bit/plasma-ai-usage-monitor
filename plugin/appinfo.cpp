#include "appinfo.h"
#include <QCoreApplication>
#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

#ifndef AIUSAGE_MONITOR_VERSION
#define AIUSAGE_MONITOR_VERSION "0.0.0"
#endif

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
}

QString AppInfo::version() const
{
    return QStringLiteral(AIUSAGE_MONITOR_VERSION);
}

QString AppInfo::pluginPath() const
{
    return QCoreApplication::applicationDirPath();
}

bool AppInfo::demoMode() const
{
    return qEnvironmentVariableIsSet("PLASMA_AI_MONITOR_DEMO");
}

QString AppInfo::smokeView() const
{
    return qEnvironmentVariable("PLASMA_AI_MONITOR_SMOKE_VIEW").trimmed().toLower();
}

bool AppInfo::exportConfig(const QString &jsonConfig, const QString &filePath) const
{
    QString localPath = filePath;
    if (localPath.startsWith("file://")) {
        localPath = QUrl(filePath).toLocalFile();
    }
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    
    QTextStream out(&file);
    out << jsonConfig;
    return true;
}

QString AppInfo::importConfig(const QString &filePath) const
{
    QString localPath = filePath;
    if (localPath.startsWith("file://")) {
        localPath = QUrl(filePath).toLocalFile();
    }
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    
    return QString(file.readAll());
}
