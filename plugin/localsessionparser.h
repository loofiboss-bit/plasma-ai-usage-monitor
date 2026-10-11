#ifndef LOCALSESSIONPARSER_H
#define LOCALSESSIONPARSER_H

#include <QVariantMap>

namespace LocalSessionParser {
// Parses one untrusted JSONL record using metadata and usage fields only.
// Returned maps never contain arbitrary source strings or file paths.
QVariantMap parseRecord(const QString &toolId, const QByteArray &line,
                        const QByteArray &hmacKey,
                        const QVariantMap &context = {});
bool updateCodexContext(const QByteArray &line, QVariantMap *context);
QByteArray identity(const QByteArray &key, const QByteArray &kind,
                    const QByteArray &value);
} // namespace LocalSessionParser
#endif
