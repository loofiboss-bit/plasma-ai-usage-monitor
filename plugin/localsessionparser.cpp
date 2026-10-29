#include "localsessionparser.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>

namespace {
qint64 number(const QJsonObject &o, const QString &key, bool *present) {
  const auto v = o.value(key);
  if (!v.isDouble() || v.toDouble() < 0 || v.toDouble() > 9.0e15)
    return -1;
  *present = true;
  return static_cast<qint64>(v.toDouble());
}
// Performance optimization: fast character loop avoids constructing and compiling
// a QRegularExpression on every JSON line processed during session log parsing.
bool isSafeModelChar(QChar ch) {
  const char16_t u = ch.unicode();
  return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') ||
         (u >= '0' && u <= '9') || u == '_' || u == '.' || u == ':' ||
         u == '/' || u == '-';
}

QString safeModel(QString model) {
  if (model.isEmpty() || model.size() > 96)
    return {};
  for (const QChar c : model) {
    if (!isSafeModelChar(c))
      return {};
  }
  return model;
}
bool containsUnverifiedLineage(const QJsonValue &value, int depth = 0) {
  if (depth > 8)
    return false;
  if (value.isObject()) {
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
      const QString key = it.key().toLower();
      if (key.contains(QLatin1String("fork")) ||
          key.contains(QLatin1String("subagent")) ||
          key == QLatin1String("agent_id") ||
          key == QLatin1String("agent_type") ||
          key == QLatin1String("parent_thread_id") ||
          key == QLatin1String("parentthreadid") ||
          key == QLatin1String("source_thread_id")) {
        const QJsonValue marker = it.value();
        if (!marker.isNull() && !marker.isUndefined() &&
            !(marker.isBool() && !marker.toBool()) &&
            !(marker.isString() && marker.toString().isEmpty()))
          return true;
      }
      if (containsUnverifiedLineage(it.value(), depth + 1))
        return true;
    }
  } else if (value.isArray()) {
    const QJsonArray array = value.toArray();
    for (qsizetype index = 0; index < qMin<qsizetype>(array.size(), 128);
         ++index)
      if (containsUnverifiedLineage(array.at(index), depth + 1))
        return true;
  }
  return false;
}
} // namespace

namespace LocalSessionParser {
JsonlReadResult readJsonlRecord(QIODevice &device, qint64 byteBudget,
                                const std::function<bool()> &cancelled) {
  JsonlReadResult result;
  constexpr qint64 maxRecordBytes = 1024 * 1024;
  const qint64 start = device.pos();
  if (start > 0) {
    if (!device.seek(start - 1)) {
      result.interrupted = true;
      return result;
    }
    const QByteArray preceding = device.read(1);
    if (!device.seek(start) || preceding.size() != 1) {
      result.interrupted = true;
      return result;
    }
    result.discarded = preceding != "\n";
  }
  while (!device.atEnd()) {
    if (result.bytesRead >= byteBudget || (cancelled && cancelled())) {
      result.interrupted = true;
      break;
    }
    const qint64 chunkSize =
        qMin<qint64>(64 * 1024, byteBudget - result.bytesRead);
    const QByteArray chunk = device.readLine(chunkSize + 1);
    if (chunk.isEmpty()) {
      result.interrupted = true;
      break;
    }
    result.bytesRead += chunk.size();
    if (!result.discarded) {
      if (result.line.size() + chunk.size() > maxRecordBytes) {
        result.discarded = true;
        result.line.clear();
      } else {
        result.line.append(chunk);
      }
    }
    if (chunk.endsWith('\n')) {
      result.complete = true;
      break;
    }
  }
  return result;
}

QByteArray identity(const QByteArray &key, const QByteArray &kind,
                    const QByteArray &value) {
  QByteArray input = kind;
  input.append('\0');
  input.append(value);
  return QMessageAuthenticationCode::hash(input, key,
                                          QCryptographicHash::Sha256)
      .toHex();
}

QVariantMap parseRecord(const QString &toolId, const QByteArray &line,
                        const QByteArray &hmacKey, const QVariantMap &context) {
  QVariantMap out;
  if ((toolId != QLatin1String("codex") &&
       toolId != QLatin1String("claude-code")) ||
      line.isEmpty() || line.size() > 1024 * 1024 || hmacKey.size() < 32)
    return out;
  QJsonParseError error;
  const auto doc = QJsonDocument::fromJson(line, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject())
    return out;
  const QJsonObject root = doc.object();
  const bool unverifiedLineage =
      toolId == QLatin1String("codex") &&
      (context.value(QStringLiteral("lineageUnverified")).toBool() ||
       containsUnverifiedLineage(root));
  QString session, project, timestamp, model;
  QJsonObject usage;
  if (toolId == QLatin1String("claude-code")) {
    const QJsonObject message =
        root.value(QStringLiteral("message")).toObject();
    if (root.value(QStringLiteral("type")).toString() !=
        QLatin1String("assistant"))
      return out;
    session = root.value(QStringLiteral("sessionId")).toString();
    project = root.value(QStringLiteral("cwd")).toString();
    timestamp = root.value(QStringLiteral("timestamp")).toString();
    model = message.value(QStringLiteral("model")).toString();
    usage = message.value(QStringLiteral("usage")).toObject();
    const QString messageId = message.value(QStringLiteral("id")).toString();
    const bool identityPartial = session.isEmpty() || project.isEmpty() ||
                                 messageId.isEmpty() || messageId.size() > 256;
    out.insert(QStringLiteral("identityPartial"), identityPartial);
    if (!identityPartial) {
      QByteArray eventIdentity = session.toUtf8();
      eventIdentity.append('\0');
      eventIdentity.append(messageId.toUtf8());
      out.insert(QStringLiteral("eventId"),
                 QString::fromLatin1(identity(
                     hmacKey, QByteArray("event:claude-code"), eventIdentity)));
      bool i = false, o = false, cr = false, cw = false;
      const auto input = number(usage, QStringLiteral("input_tokens"), &i);
      const auto output = number(usage, QStringLiteral("output_tokens"), &o);
      const auto cacheRead =
          number(usage, QStringLiteral("cache_read_input_tokens"), &cr);
      const auto cacheWrite =
          number(usage, QStringLiteral("cache_creation_input_tokens"), &cw);
      const bool usagePartial = !i || !cr || !cw;
      out.insert(QStringLiteral("usagePartial"), usagePartial);
      if (i && cr && cw)
        out.insert(QStringLiteral("inputTokens"),
                   input + cacheRead + cacheWrite);
      if (o)
        out.insert(QStringLiteral("outputTokens"), output);
      if (cr)
        out.insert(QStringLiteral("cacheReadTokens"), cacheRead);
      if (cw)
        out.insert(QStringLiteral("cacheWriteTokens"), cacheWrite);
    }
  } else {
    // Codex records cumulative token_count snapshots. Only a trustworthy thread
    // id and explicit total_usage object is accepted; callers compute deltas.
    const QJsonObject payload =
        root.value(QStringLiteral("payload")).toObject();
    if (root.value(QStringLiteral("type")).toString() !=
            QLatin1String("event_msg") ||
        payload.value(QStringLiteral("type")).toString() !=
            QLatin1String("token_count"))
      return out;
    const QJsonObject info = payload.value(QStringLiteral("info")).toObject();
    const QJsonObject total =
        info.value(QStringLiteral("total_token_usage")).toObject();
    session = root.value(QStringLiteral("thread_id")).toString();
    if (session.isEmpty())
      session = root.value(QStringLiteral("threadId")).toString();
    if (session.isEmpty())
      session = context.value(QStringLiteral("sessionId")).toString();
    project = root.value(QStringLiteral("cwd")).toString();
    if (project.isEmpty())
      project = context.value(QStringLiteral("cwd")).toString();
    timestamp = root.value(QStringLiteral("timestamp")).toString();
    if (session.isEmpty() || total.isEmpty())
      return {};
    bool i = false, o = false, cr = false, cw = false;
    const auto input = number(total, QStringLiteral("input_tokens"), &i);
    const auto output = number(total, QStringLiteral("output_tokens"), &o);
    const auto cacheRead =
        number(total, QStringLiteral("cached_input_tokens"), &cr);
    const auto cacheWrite =
        number(total, QStringLiteral("cache_write_input_tokens"), &cw);
    if (i)
      out.insert(QStringLiteral("inputTokens"), input);
    if (o)
      out.insert(QStringLiteral("outputTokens"), output);
    if (cr)
      out.insert(QStringLiteral("cacheReadTokens"), cacheRead);
    if (cw)
      out.insert(QStringLiteral("cacheWriteTokens"), cacheWrite);
    model = context.value(QStringLiteral("modelId")).toString();
    if (model.isEmpty())
      model = info.value(QStringLiteral("model")).toString();
    out.insert(QStringLiteral("cumulative"), true);
    out.insert(QStringLiteral("eventId"),
               QString::fromLatin1(
                   identity(hmacKey, "event:codex-snapshot",
                            (session + QLatin1Char('|') + timestamp +
                             QLatin1Char('|') + QString::number(input) +
                             QLatin1Char('|') + QString::number(output) +
                             QLatin1Char('|') + QString::number(cacheRead) +
                             QLatin1Char('|') + QString::number(cacheWrite))
                                .toUtf8())));
  }
  const QDateTime parsed = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
  if (session.isEmpty() || project.isEmpty() || !parsed.isValid())
    return toolId == QLatin1String("claude-code")
               ? QVariantMap{{QStringLiteral("identityPartial"), true}}
               : QVariantMap{};
  if (!parsed.isValid())
    return {};
  out.insert(
      QStringLiteral("sessionId"),
      QString::fromLatin1(identity(hmacKey, "session", session.toUtf8())));
  out.insert(
      QStringLiteral("projectId"),
      QString::fromLatin1(identity(hmacKey, "project", project.toUtf8())));
  out.insert(QStringLiteral("timestampUtc"),
             parsed.toUTC().toString(Qt::ISODateWithMs));
  const QString filtered = safeModel(model);
  out.insert(QStringLiteral("modelId"),
             filtered.isEmpty() ? QStringLiteral("") : filtered);
  out.insert(QStringLiteral("parserRevision"), 1);
  if (unverifiedLineage)
    out.insert(QStringLiteral("lineageUnverified"), true);
  out.insert(QStringLiteral("coverage"),
             out.value(QStringLiteral("identityPartial")).toBool()
                 ? QStringLiteral("partial-identity")
             : out.value(QStringLiteral("lineageUnverified")).toBool()
                 ? QStringLiteral("partial-lineage")
             : out.value(QStringLiteral("usagePartial")).toBool()
                 ? QStringLiteral("partial-usage")
                 : QStringLiteral("reported"));
  return out;
}

bool updateCodexContext(const QByteArray &line, QVariantMap *context) {
  if (!context || line.isEmpty() || line.size() > 1024 * 1024)
    return false;
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject())
    return false;
  const QJsonObject root = doc.object();
  const QString type = root.value(QStringLiteral("type")).toString();
  if (type == QLatin1String("turn_context")) {
    const QString model = safeModel(root.value(QStringLiteral("payload"))
                                        .toObject()
                                        .value(QStringLiteral("model"))
                                        .toString());
    context->insert(QStringLiteral("modelId"),
                    model.isEmpty() ? QStringLiteral("") : model);
    return true;
  }
  if (type != QLatin1String("session_meta"))
    return false;
  const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
  const QString session = payload.value(QStringLiteral("id")).toString();
  const QString cwd = payload.value(QStringLiteral("cwd")).toString();
  if (session.isEmpty() || cwd.isEmpty())
    return false;
  context->clear();
  context->insert(QStringLiteral("modelId"),
                  safeModel(payload.value(QStringLiteral("model")).toString()));
  context->insert(QStringLiteral("sessionId"), session);
  context->insert(QStringLiteral("cwd"), cwd);
  const QString createdText =
      payload.value(QStringLiteral("timestamp")).toString();
  const QDateTime created =
      QDateTime::fromString(createdText, Qt::ISODateWithMs);
  // A local wall-clock time cannot prove which side of the UTC clear cutoff
  // this session was created on, even if Qt can parse it using the host zone.
  static const QRegularExpression explicitZone(
      QStringLiteral("(?:Z|[+-][0-9]{2}:[0-9]{2})$"));
  if (created.isValid() && explicitZone.match(createdText).hasMatch())
    context->insert(QStringLiteral("createdAtUtc"), created.toUTC());
  if (containsUnverifiedLineage(payload))
    context->insert(QStringLiteral("lineageUnverified"), true);
  return true;
}
} // namespace LocalSessionParser
