#ifndef APPINFO_H
#define APPINFO_H

#include <QObject>
#include <QString>

class AppInfo : public QObject
{
    Q_OBJECT
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
    
    Q_INVOKABLE bool exportConfig(const QString &jsonConfig, const QString &filePath) const;
    Q_INVOKABLE QString importConfig(const QString &filePath) const;
};

#endif // APPINFO_H
