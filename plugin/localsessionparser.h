#ifndef LOCALSESSIONPARSER_H
#define LOCALSESSIONPARSER_H

#include <QVariantMap>
#include <functional>

class QIODevice;

namespace LocalSessionParser {
struct JsonlReadResult {
  QByteArray line;
  qint64 bytesRead = 0;
  bool discarded = false;
  bool complete = false;
  bool interrupted = false;
};
// Resume cursors may be inside a discarded oversized record. Such suffixes are
// consumed without parsing; ordinary unfinished records keep their old cursor.
JsonlReadResult readJsonlRecord(QIODevice &device, qint64 byteBudget,
                                const std::function<bool()> &cancelled = {});
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
