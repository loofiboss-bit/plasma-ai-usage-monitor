#ifndef QMLTESTSETUP_H
#define QMLTESTSETUP_H

#include <KLocalizedQmlContext>
#include <QObject>
#include <QQmlEngine>

class QmlTestSetup : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        KLocalization::setupLocalizedContext(engine);
    }
};

#endif
