#ifndef COSTENGINE_H
#define COSTENGINE_H

#include <QVariantMap>

class CostEngine
{
public:
    // Estimates are returned as a typed map so QML, history, and providers all
    // consume the same availability and provenance contract.
    static QVariantMap estimate(const QVariantMap &price,
                                const QVariantMap &usage,
                                const QString &catalogVersion = {},
                                const QString &sourceFingerprint = {});
};

#endif // COSTENGINE_H
