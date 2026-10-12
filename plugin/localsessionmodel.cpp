#include "localsessionmodel.h"
#include "budgetpolicyschema.h"
#include "localsessionparser.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <QTimeZone>
#include <QtConcurrentRun>
#include <QtEndian>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
QString dataDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
         QStringLiteral("/plasma-ai-usage-monitor");
}
QString databasePath() {
  return dataDirectory() + QStringLiteral("/usage_history.db");
}
QByteArray sessionKey() {
  QLockFile lock(dataDirectory() + QStringLiteral("/session-key.lock"));
  lock.setStaleLockTime(30000);
  if (!lock.tryLock(5000))
    return {};
  const QString path =
      dataDirectory() + QStringLiteral("/session-identity.key");
  QFile f(path);
  if (f.open(QIODevice::ReadOnly)) {
    const QByteArray key = f.read(64);
    if (key.size() == 32)
      return key;
    return {};
  }
  QByteArray key(32, Qt::Uninitialized);
  for (qsizetype i = 0; i < key.size(); i += 4) {
    const quint32 n = QRandomGenerator::system()->generate();
    qToLittleEndian(n, reinterpret_cast<uchar *>(key.data() + i));
  }
  QSaveFile out(path);
  out.setDirectWriteFallback(false);
  if (!out.open(QIODevice::WriteOnly))
    return {};
  out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  if (out.write(key) != key.size() || !out.commit())
    return {};
  QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  return key;
}
QSqlDatabase openDb(const QString &connection) {
  QSqlDatabase db =
      QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
  db.setDatabaseName(databasePath());
  if (!db.open())
    return {};
  QSqlQuery p(db);
  p.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
  p.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
  return db;
}
bool recordToolCoverage(QSqlDatabase &db, const QString &tool, bool partial) {
  QSqlQuery query(db);
  query.prepare(QStringLiteral(
      "INSERT INTO local_import_checkpoints(tool_id,coverage) VALUES(?,?) "
      "ON CONFLICT(tool_id) DO UPDATE SET coverage=CASE WHEN "
      "excluded.coverage='partial' THEN 'partial' ELSE "
      "local_import_checkpoints.coverage END"));
  query.addBindValue(tool);
  query.addBindValue(partial ? QStringLiteral("partial")
                             : QStringLiteral("reported"));
  return query.exec();
}
bool isMalformedUsageCandidate(const QString &tool, const QByteArray &line) {
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(line, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return !line.trimmed().isEmpty();
  const QJsonObject object = document.object();
  if (tool == QLatin1String("codex"))
    return object.value(QStringLiteral("type")).toString() ==
               QLatin1String("event_msg") &&
           object.value(QStringLiteral("payload"))
                   .toObject()
                   .value(QStringLiteral("type"))
                   .toString() == QLatin1String("token_count");
  return object.value(QStringLiteral("type")).toString() ==
         QLatin1String("assistant");
}
QString toolRoot(const QString &tool) {
  if (tool == QLatin1String("codex")) {
    const QString base = qEnvironmentVariable("CODEX_HOME");
    return (base.isEmpty() ? QDir::homePath() + QStringLiteral("/.codex")
                           : base) +
           QStringLiteral("/sessions");
  }
  if (tool == QLatin1String("claude-code")) {
    const QString base = qEnvironmentVariable("CLAUDE_CONFIG_DIR");
    return (base.isEmpty() ? QDir::homePath() + QStringLiteral("/.claude")
                           : base) +
           QStringLiteral("/projects");
  }
  return {};
}
QString canonicalTool(QString tool) {
  return tool == QLatin1String("codex-cli") ? QStringLiteral("codex") : tool;
}
} // namespace

LocalSessionModel::LocalSessionModel(QObject *parent) : QObject(parent) {
  m_importTimer = new QTimer(this);
  m_importTimer->setObjectName(QStringLiteral("local-session-import-timer"));
  m_importTimer->setInterval(60000);
  m_importTimer->setTimerType(Qt::CoarseTimer);
  connect(m_importTimer, &QTimer::timeout, this, [this]() {
    if (m_pending == 0)
      requestImport();
  });
}

LocalSessionModel::~LocalSessionModel() {
  m_generationState->fetch_add(1, std::memory_order_release);
}

void LocalSessionModel::setRetentionDays(int days) {
  const int bounded = qBound(1, days, 3650);
  if (m_retentionDays == bounded)
    return;
  m_retentionDays = bounded;
  emit retentionDaysChanged();
}

void LocalSessionModel::setToolEnabled(const QString &toolId, bool enabled) {
  bool *target = nullptr;
  if (toolId == QLatin1String("codex") || toolId == QLatin1String("codex-cli"))
    target = &m_codexEnabled;
  else if (toolId == QLatin1String("claude-code"))
    target = &m_claudeEnabled;
  else
    return;
  if (*target == enabled)
    return;
  *target = enabled;
  ++m_generation;
  m_generationState->store(m_generation, std::memory_order_release);
  if (m_codexEnabled || m_claudeEnabled)
    m_importTimer->start();
  else
    m_importTimer->stop();
  emit enabledChanged();
  m_status = (m_codexEnabled || m_claudeEnabled) ? QStringLiteral("enabled")
                                                 : QStringLiteral("disabled");
  emit statusChanged();
  if (enabled || m_codexEnabled || m_claudeEnabled)
    requestImport();
}

void LocalSessionModel::requestImport() {
  const quint64 generation = m_generation;
  const auto generationState = m_generationState;
  const int retention = m_retentionDays;
  const bool codex = m_codexEnabled, claude = m_claudeEnabled;
  if (!codex && !claude)
    return;
  if (m_pending > 0) {
    m_importAgain = true;
    return;
  }
  ++m_pending;
  emit statusChanged();
  auto *watcher = new QFutureWatcher<QVariantMap>(this);
  connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
          [this, watcher, generation]() {
            const QVariantMap result = watcher->result();
            watcher->deleteLater();
            m_pending = qMax(0, m_pending - 1);
            if (generation == m_generation) {
              m_status =
                  result.value(QStringLiteral("errorKey")).toString().isEmpty()
                      ? QStringLiteral("ready")
                      : QStringLiteral("error");
              emit statusChanged();
              QString completedTool =
                  result.value(QStringLiteral("toolId")).toString();
              if (completedTool == QLatin1String("codex"))
                completedTool = QStringLiteral("codex-cli");
              emit importFinished(
                  completedTool, result.value(QStringLiteral("count")).toInt(),
                  result.value(QStringLiteral("partial")).toBool(),
                  result.value(QStringLiteral("errorKey")).toString());
            }
            emit statusChanged();
            if (m_importAgain) {
              m_importAgain = false;
              requestImport();
            }
          });
  watcher->setFuture(QtConcurrent::run([codex, claude, generation, retention,
                                        generationState]() {
    QVariantMap result;
    result.insert(QStringLiteral("count"), 0);
    result.insert(QStringLiteral("partial"), false);
    QLockFile lock(dataDirectory() + QStringLiteral("/session-import.lock"));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) {
      result.insert(QStringLiteral("errorKey"),
                    QStringLiteral("session-import-busy"));
      return result;
    }
    const QByteArray key = sessionKey();
    if (key.size() != 32) {
      result.insert(QStringLiteral("errorKey"),
                    QStringLiteral("session-key-unavailable"));
      return result;
    }
    bool retentionCleanupFailed = false;
    {
      const QString cleanupConnection =
          QStringLiteral("local_sessions_retention_%1")
              .arg(QRandomGenerator::global()->generate());
      auto cleanupDb = openDb(cleanupConnection);
      if (!cleanupDb.isOpen()) {
        result.insert(QStringLiteral("errorKey"),
                      QStringLiteral("session-database-unavailable"));
        return result;
      }
      const QString cutoff = QDateTime::currentDateTimeUtc()
                                 .addDays(-retention)
                                 .toString(Qt::ISODateWithMs);
      QSqlQuery markTrimmed(cleanupDb), deleteEvents(cleanupDb),
          deleteSessions(cleanupDb);
      markTrimmed.prepare(QStringLiteral(
          "UPDATE local_sessions SET coverage='partial-retention' WHERE "
          "EXISTS(SELECT 1 FROM local_usage_events e WHERE "
          "e.session_id=local_sessions.session_id AND "
          "e.observed_at_utc<?)"));
      markTrimmed.addBindValue(cutoff);
      deleteEvents.prepare(QStringLiteral(
          "DELETE FROM local_usage_events WHERE observed_at_utc<?"));
      deleteEvents.addBindValue(cutoff);
      deleteSessions.prepare(QStringLiteral(
          "DELETE FROM local_sessions WHERE last_seen_at_utc<?"));
      deleteSessions.addBindValue(cutoff);
      const bool ok = cleanupDb.transaction() && markTrimmed.exec() &&
                      deleteEvents.exec() && deleteSessions.exec();
      if (!ok || !cleanupDb.commit()) {
        cleanupDb.rollback();
        retentionCleanupFailed = true;
        result.insert(QStringLiteral("partial"), true);
      }
      cleanupDb.close();
      cleanupDb = {};
      QSqlDatabase::removeDatabase(cleanupConnection);
    }
    for (const QString &tool :
         {QStringLiteral("codex"), QStringLiteral("claude-code")}) {
      if ((tool == QLatin1String("codex") && !codex) ||
          (tool == QLatin1String("claude-code") && !claude))
        continue;
      result.insert(QStringLiteral("toolId"), tool);
      QString cutoffUtc;
      bool rebaselinePending = false;
      QString persistedCoverage;
      {
        const QString checkpointConnection =
            QStringLiteral("local_sessions_checkpoint_%1")
                .arg(QRandomGenerator::global()->generate());
        auto checkpointDb = openDb(checkpointConnection);
        if (!checkpointDb.isOpen()) {
          result.insert(QStringLiteral("errorKey"),
                        QStringLiteral("session-database-unavailable"));
          return result;
        }
        QSqlQuery checkpoint(checkpointDb);
        checkpoint.prepare(
            QStringLiteral("SELECT cutoff_utc,rebaseline_pending,coverage FROM "
                           "local_import_checkpoints WHERE tool_id=?"));
        checkpoint.addBindValue(tool);
        if (checkpoint.exec() && checkpoint.next()) {
          cutoffUtc = checkpoint.value(0).toString();
          rebaselinePending = checkpoint.value(1).toBool();
          persistedCoverage = checkpoint.value(2).toString();
        }
        checkpointDb.close();
        checkpointDb = {};
        QSqlDatabase::removeDatabase(checkpointConnection);
      }
      bool toolPartial = retentionCleanupFailed ||
                         persistedCoverage == QLatin1String("partial");
      const auto markPartial = [&]() {
        toolPartial = true;
        result.insert(QStringLiteral("partial"), true);
      };
      const QString root = toolRoot(tool);
      const QFileInfo rootInfo(root);
      if (!rootInfo.exists()) {
        const QString statusConnection =
            QStringLiteral("local_sessions_status_%1")
                .arg(QRandomGenerator::global()->generate());
        auto statusDb = openDb(statusConnection);
        if (statusDb.isOpen()) {
          recordToolCoverage(statusDb, tool, true);
          statusDb.close();
          statusDb = {};
          QSqlDatabase::removeDatabase(statusConnection);
        }
        markPartial();
        continue;
      }
      if (rootInfo.isSymLink() || !rootInfo.isDir()) {
        markPartial();
        const QString statusConnection =
            QStringLiteral("local_sessions_status_%1")
                .arg(QRandomGenerator::global()->generate());
        auto statusDb = openDb(statusConnection);
        if (statusDb.isOpen()) {
          recordToolCoverage(statusDb, tool, true);
          statusDb.close();
          statusDb = {};
          QSqlDatabase::removeDatabase(statusConnection);
        }
        continue;
      }
      QDirIterator it(root, {QStringLiteral("*.jsonl")}, QDir::Files,
                      QDirIterator::Subdirectories);
      QStringList candidates;
      constexpr qsizetype maxDiscoveredFiles = 50000;
      while (it.hasNext() && candidates.size() < maxDiscoveredFiles) {
        if (generationState->load(std::memory_order_acquire) != generation)
          return result;
        candidates.append(it.next());
      }
      if (it.hasNext())
        markPartial();
      std::sort(candidates.begin(), candidates.end(),
                [](const QString &left, const QString &right) {
                  const QFileInfo leftInfo(left), rightInfo(right);
                  if (leftInfo.lastModified() != rightInfo.lastModified())
                    return leftInfo.lastModified() > rightInfo.lastModified();
                  return left < right;
                });
      if (candidates.size() > 5000) {
        candidates = candidates.mid(0, 5000);
        markPartial();
      }
      qsizetype fileCount = 0;
      qint64 bytesRead = 0;
      const QDateTime retentionBoundary =
          QDateTime::currentDateTimeUtc().addDays(-qMin(30, retention));
      for (const QString &path : candidates) {
        if (generationState->load(std::memory_order_acquire) != generation)
          return result;
        if (++fileCount > 5000 || bytesRead > 256ll * 1024 * 1024) {
          markPartial();
          break;
        }
        QFileInfo info(path);
        if (info.isSymLink() || !info.isFile() ||
            info.size() > 100ll * 1024 * 1024) {
          markPartial();
          continue;
        }
        const QByteArray nativePath = QFile::encodeName(path);
        const int sourceFd =
            ::open(nativePath.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        struct stat sourceStat{};
        if (sourceFd < 0 || ::fstat(sourceFd, &sourceStat) != 0 ||
            !S_ISREG(sourceStat.st_mode)) {
          if (sourceFd >= 0)
            ::close(sourceFd);
          markPartial();
          continue;
        }
        QFile file;
        if (!file.open(sourceFd, QIODevice::ReadOnly,
                       QFileDevice::AutoCloseHandle)) {
          ::close(sourceFd);
          markPartial();
          continue;
        }
        const QString conn =
            QStringLiteral("local_sessions_import_%1_%2")
                .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
                .arg(QRandomGenerator::global()->generate());
        QSqlDatabase db = openDb(conn);
        if (!db.isOpen()) {
          result.insert(QStringLiteral("errorKey"),
                        QStringLiteral("session-database-unavailable"));
          return result;
        }
        const QString fileId = QString::fromLatin1(
            LocalSessionParser::identity(key, "file", nativePath));
        const qint64 currentSize = sourceStat.st_size;
        const QString modifiedUtc =
            info.lastModified().toUTC().toString(Qt::ISODateWithMs);
        qint64 startOffset = 0, previousSize = -1;
        QString previousModified;
        QSqlQuery prior(db);
        prior.prepare(QStringLiteral(
            "SELECT file_offset,file_size,modified_at_utc FROM "
            "local_file_checkpoints WHERE tool_id=? AND file_id=?"));
        prior.addBindValue(tool);
        prior.addBindValue(fileId);
        if (prior.exec() && prior.next()) {
          startOffset = prior.value(0).toLongLong();
          previousSize = prior.value(1).toLongLong();
          previousModified = prior.value(2).toString();
        }
        if (currentSize == previousSize && modifiedUtc == previousModified &&
            startOffset == currentSize) {
          db.close();
          db = {};
          QSqlDatabase::removeDatabase(conn);
          continue;
        }
        if (currentSize < previousSize ||
            (previousSize >= 0 && currentSize <= previousSize &&
             modifiedUtc != previousModified)) {
          startOffset = 0;
          markPartial();
        }
        if (startOffset > currentSize)
          startOffset = 0;
        if (startOffset > 0 && !file.seek(startOffset)) {
          startOffset = 0;
          file.seek(0);
          markPartial();
        }
        qint64 safeOffset = startOffset;
        QVariantMap codexContext;
        if (tool == QLatin1String("codex") && startOffset > 0) {
          file.seek(0);
          while (file.pos() < startOffset && !file.atEnd()) {
            const QByteArray line = file.readLine(1024 * 1024 + 1);
            bytesRead += line.size();
            if (bytesRead > 256ll * 1024 * 1024) {
              markPartial();
              break;
            }
            LocalSessionParser::updateCodexContext(line, &codexContext);
          }
          if (bytesRead > 256ll * 1024 * 1024) {
            db.close();
            db = {};
            QSqlDatabase::removeDatabase(conn);
            break;
          }
          if (codexContext.isEmpty()) {
            startOffset = 0;
            safeOffset = 0;
            file.seek(0);
            markPartial();
          }
        }
        if (startOffset > 0 && !file.seek(startOffset)) {
          startOffset = 0;
          safeOffset = 0;
          markPartial();
        }
        if (!db.transaction()) {
          result.insert(QStringLiteral("errorKey"),
                        QStringLiteral("session-database-unavailable"));
          return result;
        }
        bool writesOk = true;
        int fileImportedCount = 0;
        QSqlQuery upsert(db), event(db);
        upsert.prepare(QStringLiteral(
            "INSERT INTO "
            "local_sessions(session_id,tool_id,project_id,started_at_utc,last_"
            "seen_at_utc,coverage,source_id) VALUES(?,?,?,?,?,'reported','') "
            "ON CONFLICT(session_id) DO UPDATE SET "
            "started_at_utc=MIN(started_at_utc,excluded.started_at_utc),last_"
            "seen_at_utc=MAX(last_seen_at_utc,excluded.last_seen_at_utc)"));
        event.prepare(QStringLiteral(
            "INSERT INTO "
            "local_usage_events(event_id,session_id,observed_at_utc,model_id,"
            "input_tokens,output_tokens,cache_read_tokens,cache_write_tokens,"
            "parser_revision,provenance) VALUES(?,?,?,?,?,?,?,?,1,'reported') "
            "ON CONFLICT(event_id) DO UPDATE SET "
            "input_tokens=COALESCE(excluded.input_tokens,input_tokens),output_"
            "tokens=COALESCE(excluded.output_tokens,output_tokens),cache_read_"
            "tokens=COALESCE(excluded.cache_read_tokens,cache_read_tokens),"
            "cache_write_tokens=COALESCE(excluded.cache_write_tokens,cache_"
            "write_tokens)"));
        while (!file.atEnd()) {
          if (generationState->load(std::memory_order_acquire) != generation) {
            db.rollback();
            db.close();
            db = {};
            QSqlDatabase::removeDatabase(conn);
            return result;
          }
          const QByteArray line = file.readLine(1024 * 1024 + 1);
          bytesRead += line.size();
          if (bytesRead > 256ll * 1024 * 1024) {
            markPartial();
            break;
          }
          if (!line.endsWith('\n') && !file.atEnd()) {
            markPartial();
            break;
          }
          if (line.size() > 1024 * 1024) {
            markPartial();
            if (line.endsWith('\n'))
              safeOffset = file.pos();
            continue;
          }
          if (line.endsWith('\n'))
            safeOffset = file.pos();
          if (tool == QLatin1String("codex") &&
              LocalSessionParser::updateCodexContext(line, &codexContext))
            continue;
          const QVariantMap row =
              LocalSessionParser::parseRecord(tool, line, key, codexContext);
          if (row.isEmpty()) {
            if (isMalformedUsageCandidate(tool, line))
              markPartial();
            continue;
          }
          const bool identityPartial =
              row.value(QStringLiteral("identityPartial")).toBool();
          if (identityPartial &&
              !row.contains(QStringLiteral("timestampUtc"))) {
            markPartial();
            continue;
          }
          if (!cutoffUtc.isEmpty() &&
              row.value(QStringLiteral("timestampUtc")).toString() <= cutoffUtc)
            continue;
          const bool outsideRetention =
              QDateTime::fromString(
                  row.value(QStringLiteral("timestampUtc")).toString(),
                  Qt::ISODateWithMs) < retentionBoundary;
          if (outsideRetention &&
              !row.value(QStringLiteral("cumulative")).toBool() &&
              !identityPartial)
            continue;
          if (identityPartial && (!row.contains(QStringLiteral("sessionId")) ||
                                  !row.contains(QStringLiteral("projectId")))) {
            markPartial();
            continue;
          }
          bool usageConflict = false;
          if (row.value(QStringLiteral("cumulative")).toBool()) {
            QSqlQuery duplicate(db);
            duplicate.prepare(QStringLiteral(
                "SELECT 1 FROM local_usage_events WHERE event_id=?"));
            duplicate.addBindValue(row.value("eventId"));
            if (!duplicate.exec()) {
              writesOk = false;
              break;
            }
            if (duplicate.next())
              continue;
          } else {
            QSqlQuery conflict(db);
            conflict.prepare(QStringLiteral(
                "SELECT 1 FROM local_usage_conflicts WHERE event_id=?"));
            conflict.addBindValue(row.value("eventId"));
            if (!conflict.exec()) {
              writesOk = false;
              break;
            }
            if (conflict.next())
              continue;
            QSqlQuery previous(db);
            previous.prepare(QStringLiteral(
                "SELECT model_id,input_tokens,output_tokens,cache_read_tokens,"
                "cache_write_tokens FROM local_usage_events WHERE event_id=?"));
            previous.addBindValue(row.value("eventId"));
            if (!previous.exec()) {
              writesOk = false;
              break;
            }
            if (previous.next()) {
              usageConflict = row.value("modelId").toString() !=
                              previous.value(0).toString();
              const QStringList fields{QStringLiteral("inputTokens"),
                                       QStringLiteral("outputTokens"),
                                       QStringLiteral("cacheReadTokens"),
                                       QStringLiteral("cacheWriteTokens")};
              for (int index = 0; index < fields.size(); ++index)
                if (row.contains(fields.at(index)) &&
                    !previous.value(index + 1).isNull() &&
                    row.value(fields.at(index)).toLongLong() !=
                        previous.value(index + 1).toLongLong())
                  usageConflict = true;
            }
          }
          upsert.bindValue(0, row.value(QStringLiteral("sessionId")));
          upsert.bindValue(1, tool);
          upsert.bindValue(2, row.value(QStringLiteral("projectId")));
          upsert.bindValue(3, row.value(QStringLiteral("timestampUtc")));
          upsert.bindValue(4, row.value(QStringLiteral("timestampUtc")));
          if (!upsert.exec()) {
            writesOk = false;
            break;
          }
          if (outsideRetention) {
            QSqlQuery markRetention(db);
            markRetention.prepare(QStringLiteral(
                "UPDATE local_sessions SET coverage='partial-retention' "
                "WHERE session_id=? AND coverage='reported'"));
            markRetention.addBindValue(row.value("sessionId"));
            if (!markRetention.exec()) {
              writesOk = false;
              break;
            }
          }
          if (identityPartial) {
            markPartial();
            QSqlQuery markIdentity(db);
            markIdentity.prepare(QStringLiteral(
                "UPDATE local_sessions SET coverage='partial-identity' "
                "WHERE session_id=? AND coverage='reported'"));
            markIdentity.addBindValue(row.value("sessionId"));
            if (!markIdentity.exec()) {
              writesOk = false;
              break;
            }
            continue;
          }
          if (row.value(QStringLiteral("lineageUnverified")).toBool()) {
            markPartial();
            QSqlQuery markLineage(db);
            markLineage.prepare(QStringLiteral(
                "UPDATE local_sessions SET coverage='partial-lineage' "
                "WHERE session_id=? AND coverage='reported'"));
            markLineage.addBindValue(row.value("sessionId"));
            if (!markLineage.exec()) {
              writesOk = false;
              break;
            }
            continue;
          }
          if (usageConflict) {
            markPartial();
            QSqlQuery markConflict(db);
            QSqlQuery removeAmbiguousEvent(db);
            removeAmbiguousEvent.prepare(QStringLiteral(
                "DELETE FROM local_usage_events WHERE event_id=?"));
            removeAmbiguousEvent.addBindValue(row.value("eventId"));
            QSqlQuery conflict(db);
            conflict.prepare(
                QStringLiteral("INSERT OR IGNORE INTO "
                               "local_usage_conflicts(event_id) VALUES(?)"));
            conflict.addBindValue(row.value("eventId"));
            if (!conflict.exec() || !removeAmbiguousEvent.exec()) {
              writesOk = false;
              break;
            }
            markConflict.prepare(QStringLiteral(
                "UPDATE local_sessions SET coverage='partial-conflict' "
                "WHERE session_id=? AND coverage='reported'"));
            markConflict.addBindValue(row.value("sessionId"));
            if (!markConflict.exec()) {
              writesOk = false;
              break;
            }
            continue;
          }
          if (row.value(QStringLiteral("usagePartial")).toBool()) {
            markPartial();
            QSqlQuery markUsage(db);
            markUsage.prepare(QStringLiteral(
                "UPDATE local_sessions SET coverage='partial-usage' "
                "WHERE session_id=? AND coverage='reported'"));
            markUsage.addBindValue(row.value("sessionId"));
            if (!markUsage.exec()) {
              writesOk = false;
              break;
            }
          }
          QVariantMap values = row;
          if (row.value(QStringLiteral("cumulative")).toBool()) {
            QSqlQuery old(db);
            old.prepare(QStringLiteral(
                "SELECT "
                "observed_at_utc,input_tokens,output_tokens,cache_read_tokens,"
                "cache_write_tokens FROM local_session_counters WHERE "
                "session_id=?"));
            old.addBindValue(row.value("sessionId"));
            QVariantList previous;
            if (!old.exec()) {
              writesOk = false;
              break;
            }
            if (old.next()) {
              if (old.value(0).toString() >
                  row.value("timestampUtc").toString())
                continue;
              previous = {old.value(1), old.value(2), old.value(3),
                          old.value(4)};
            }
            const QStringList counterFields{QStringLiteral("inputTokens"),
                                            QStringLiteral("outputTokens"),
                                            QStringLiteral("cacheReadTokens"),
                                            QStringLiteral("cacheWriteTokens")};
            bool unchanged = !previous.isEmpty();
            bool reset = false;
            const bool baselineOnly = previous.isEmpty() && rebaselinePending;
            for (qsizetype index = 0; index < counterFields.size(); ++index) {
              const QString &field = counterFields.at(index);
              if (!row.contains(field))
                continue;
              const qint64 current = row.value(field).toLongLong();
              const QVariant priorValue = previous.value(index);
              if (priorValue.isNull()) {
                unchanged = false;
                values.remove(field);
              } else {
                const qint64 prior = priorValue.toLongLong();
                if (current != prior)
                  unchanged = false;
                if (current < prior)
                  reset = true;
                values.insert(field,
                              current >= prior ? current - prior : QVariant());
              }
            }
            if (unchanged)
              continue;
            if (previous.isEmpty()) {
              for (const QString &field : counterFields)
                if (row.contains(field))
                  values.insert(field, row.value(field));
            }
            if (baselineOnly) {
              values.remove("inputTokens");
              values.remove("outputTokens");
              values.remove("cacheReadTokens");
              values.remove("cacheWriteTokens");
              QSqlQuery mark(db);
              mark.prepare(QStringLiteral("UPDATE local_sessions SET "
                                          "coverage='partial-clear-rebaseline' "
                                          "WHERE session_id=?"));
              mark.addBindValue(row.value("sessionId"));
              if (!mark.exec()) {
                writesOk = false;
                break;
              }
            }
            if (reset) {
              markPartial();
              values.remove("inputTokens");
              values.remove("outputTokens");
              values.remove("cacheReadTokens");
              values.remove("cacheWriteTokens");
              QSqlQuery mark(db);
              mark.prepare(QStringLiteral(
                  "UPDATE local_sessions SET coverage='partial-counter-reset' "
                  "WHERE session_id=?"));
              mark.addBindValue(row.value("sessionId"));
              if (!mark.exec()) {
                writesOk = false;
                break;
              }
            }
            QSqlQuery saveCounter(db);
            saveCounter.prepare(QStringLiteral(
                "INSERT INTO local_session_counters(session_id,observed_at_"
                "utc,input_tokens,output_tokens,cache_read_tokens,cache_"
                "write_tokens) VALUES(?,?,?,?,?,?) ON CONFLICT(session_id) "
                "DO UPDATE SET observed_at_utc=excluded.observed_at_utc,"
                "input_tokens=COALESCE(excluded.input_tokens,input_tokens),"
                "output_tokens=COALESCE(excluded.output_tokens,output_tokens),"
                "cache_read_tokens=COALESCE(excluded.cache_read_tokens,"
                "cache_read_tokens),cache_write_tokens=COALESCE(excluded."
                "cache_write_tokens,cache_write_tokens)"));
            saveCounter.addBindValue(row.value("sessionId"));
            saveCounter.addBindValue(row.value("timestampUtc"));
            saveCounter.addBindValue(row.value("inputTokens"));
            saveCounter.addBindValue(row.value("outputTokens"));
            saveCounter.addBindValue(row.value("cacheReadTokens"));
            saveCounter.addBindValue(row.value("cacheWriteTokens"));
            if (!saveCounter.exec()) {
              writesOk = false;
              break;
            }
          }
          if (outsideRetention ||
              (row.value(QStringLiteral("cumulative")).toBool() &&
               rebaselinePending && values.value("inputTokens").isNull() &&
               values.value("outputTokens").isNull() &&
               values.value("cacheReadTokens").isNull() &&
               values.value("cacheWriteTokens").isNull()))
            continue;
          event.bindValue(0, row.value(QStringLiteral("eventId")));
          event.bindValue(1, row.value(QStringLiteral("sessionId")));
          event.bindValue(2, row.value(QStringLiteral("timestampUtc")));
          event.bindValue(
              3, row.value(QStringLiteral("modelId"), QStringLiteral("")));
          event.bindValue(4, values.value(QStringLiteral("inputTokens")));
          event.bindValue(5, values.value(QStringLiteral("outputTokens")));
          event.bindValue(6, values.value(QStringLiteral("cacheReadTokens")));
          event.bindValue(7, values.value(QStringLiteral("cacheWriteTokens")));
          if (!event.exec()) {
            writesOk = false;
            break;
          }
          fileImportedCount += event.numRowsAffected();
        }
        QSqlQuery checkpoint(db);
        checkpoint.prepare(QStringLiteral(
            "INSERT INTO "
            "local_file_checkpoints(tool_id,file_id,file_offset,file_size,"
            "modified_at_utc) VALUES(?,?,?,?,?) ON CONFLICT(tool_id,file_id) "
            "DO UPDATE SET "
            "file_offset=excluded.file_offset,file_size=excluded.file_size,"
            "modified_at_utc=excluded.modified_at_utc"));
        checkpoint.addBindValue(tool);
        checkpoint.addBindValue(fileId);
        checkpoint.addBindValue(safeOffset);
        checkpoint.addBindValue(currentSize);
        checkpoint.addBindValue(modifiedUtc);
        writesOk = writesOk && checkpoint.exec();
        writesOk = writesOk && recordToolCoverage(db, tool, toolPartial);
        if (generationState->load(std::memory_order_acquire) != generation) {
          db.rollback();
          db.close();
          db = {};
          QSqlDatabase::removeDatabase(conn);
          return result;
        }
        if (!writesOk || file.error() != QFileDevice::NoError || !db.commit()) {
          db.rollback();
          markPartial();
          result.insert(QStringLiteral("errorKey"),
                        QStringLiteral("session-database-write-failed"));
        } else {
          result.insert(QStringLiteral("count"),
                        result.value(QStringLiteral("count")).toInt() +
                            fileImportedCount);
        }
        db.close();
        db = {};
        QSqlDatabase::removeDatabase(conn);
      }
      const QString statusConnection =
          QStringLiteral("local_sessions_status_%1")
              .arg(QRandomGenerator::global()->generate());
      auto statusDb = openDb(statusConnection);
      if (statusDb.isOpen()) {
        recordToolCoverage(statusDb, tool, toolPartial);
        if (!toolPartial) {
          QSqlQuery complete(statusDb);
          complete.prepare(QStringLiteral(
              "UPDATE local_import_checkpoints SET rebaseline_pending=0 "
              "WHERE tool_id=?"));
          complete.addBindValue(tool);
          complete.exec();
        }
        statusDb.close();
        statusDb = {};
        QSqlDatabase::removeDatabase(statusConnection);
      }
    }
    return result;
  }));
}

void LocalSessionModel::requestSessions(
    const QString &requestId, const QString &toolId, const QDateTime &fromUtc,
    const QDateTime &toUtc, const QString &projectId, int offset, int limit) {
  const QString storedTool = canonicalTool(toolId);
  offset = qMax(0, offset);
  limit = qBound(1, limit, 100);
  auto *w = new QFutureWatcher<QVariantList>(this);
  connect(w, &QFutureWatcher<QVariantList>::finished, this,
          [this, w, requestId]() {
            emit sessionsReady(requestId, w->result());
            w->deleteLater();
          });
  w->setFuture(QtConcurrent::run([=]() {
    QVariantList rows;
    const QString c = QStringLiteral("local_sessions_query_%1")
                          .arg(QRandomGenerator::global()->generate());
    auto db = openDb(c);
    if (!db.isOpen())
      return rows;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT "
        "s.session_id,s.project_id,s.started_at_utc,s.last_seen_at_utc,s."
        "coverage,COUNT(e.event_id),SUM(e.input_tokens),SUM(e.output_tokens),"
        "SUM(e.cache_read_tokens),SUM(e.cache_write_tokens) FROM "
        "local_sessions s JOIN local_usage_events e ON "
        "e.session_id=s.session_id AND e.observed_at_utc>=? AND "
        "e.observed_at_utc<? WHERE s.tool_id=? AND (?='' OR s.project_id=?) "
        "GROUP BY "
        "s.session_id ORDER BY MAX(e.observed_at_utc) DESC,s.session_id LIMIT "
        "? OFFSET ?"));
    q.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
    q.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
    q.addBindValue(storedTool);
    q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (q.exec())
      while (q.next()) {
        QVariantMap r;
        r.insert("sessionId", q.value(0));
        r.insert("projectId", q.value(1));
        r.insert("startedAtUtc", q.value(2));
        r.insert("lastSeenAtUtc", q.value(3));
        r.insert("coverage", q.value(4));
        r.insert("eventCount", q.value(5));
        r.insert("inputTokens", q.value(6));
        r.insert("outputTokens", q.value(7));
        r.insert("cacheReadTokens", q.value(8));
        r.insert("cacheWriteTokens", q.value(9));
        QSqlQuery models(db);
        models.prepare(QStringLiteral(
            "SELECT model_id,COUNT(*),SUM(input_tokens),SUM(output_tokens) "
            "FROM local_usage_events WHERE session_id=? AND observed_at_utc>=? "
            "AND observed_at_utc<? GROUP BY model_id ORDER BY COUNT(*) DESC "
            "LIMIT 100"));
        models.addBindValue(q.value(0));
        models.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
        models.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
        QVariantList modelRows;
        if (models.exec())
          while (models.next()) {
            QVariantMap m;
            m.insert("modelId", models.value(0));
            m.insert("eventCount", models.value(1));
            m.insert("inputTokens", models.value(2));
            m.insert("outputTokens", models.value(3));
            modelRows.append(m);
          }
        r.insert("modelBreakdown", modelRows);
        rows.append(r);
      }
    else
      db.close();
    db = {};
    QSqlDatabase::removeDatabase(c);
    return rows;
  }));
}

void LocalSessionModel::requestSummary(const QString &requestId,
                                       const QString &toolId,
                                       const QDateTime &fromUtc,
                                       const QDateTime &toUtc,
                                       const QString &projectId) {
  const QString storedTool = canonicalTool(toolId);
  const bool toolEnabled =
      storedTool == QLatin1String("codex") ? m_codexEnabled : m_claudeEnabled;
  auto *w = new QFutureWatcher<QVariantMap>(this);
  connect(w, &QFutureWatcher<QVariantMap>::finished, this,
          [this, w, requestId]() {
            emit summaryReady(requestId, w->result());
            w->deleteLater();
          });
  w->setFuture(QtConcurrent::run([=]() {
    QVariantMap result;
    const QString c = QStringLiteral("local_sessions_summary_%1")
                          .arg(QRandomGenerator::global()->generate());
    auto db = openDb(c);
    if (!db.isOpen())
      return result;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT "
        "s.session_id),COUNT(e.event_id),SUM(e.input_tokens),SUM(e.output_"
        "tokens),SUM(e.cache_read_tokens),SUM(e.cache_write_tokens),COUNT("
        "DISTINCT s.project_id),MAX(e.observed_at_utc),CASE WHEN SUM(CASE "
        "WHEN s.coverage!='reported' THEN 1 ELSE 0 END)>0 THEN 'partial' "
        "ELSE 'reported' END FROM local_sessions s "
        "JOIN "
        "local_usage_events e ON e.session_id=s.session_id AND "
        "e.observed_at_utc>=? AND e.observed_at_utc<? WHERE s.tool_id=? AND "
        "(?='' OR s.project_id=?)"));
    q.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
    q.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
    q.addBindValue(storedTool);
    q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    if (q.exec() && q.next()) {
      result.insert("sessionCount", q.value(0));
      result.insert("eventCount", q.value(1));
      result.insert("inputTokens", q.value(2));
      result.insert("outputTokens", q.value(3));
      result.insert("cacheReadTokens", q.value(4));
      result.insert("cacheWriteTokens", q.value(5));
      result.insert("projectCount", q.value(6));
      result.insert("lastActivityAtUtc", q.value(7));
      result.insert("coverage", q.value(0).toInt() == 0
                                    ? QStringLiteral("unavailable")
                                    : q.value(8).toString());
    }
    QString toolCoverage;
    QSqlQuery checkpoint(db);
    checkpoint.prepare(QStringLiteral(
        "SELECT coverage FROM local_import_checkpoints WHERE tool_id=?"));
    checkpoint.addBindValue(storedTool);
    if (checkpoint.exec() && checkpoint.next())
      toolCoverage = checkpoint.value(0).toString();
    QString persistedStatus = QStringLiteral("unknown");
    if (!toolEnabled) {
      persistedStatus = QStringLiteral("disabled");
    } else if (toolCoverage == QLatin1String("partial")) {
      persistedStatus = QStringLiteral("partial");
      result.insert("coverage", QStringLiteral("partial"));
    } else if (toolCoverage == QLatin1String("reported")) {
      persistedStatus = QStringLiteral("reported");
    }
    result.insert("toolCoverage", toolCoverage.isEmpty()
                                      ? QStringLiteral("unknown")
                                      : toolCoverage);
    result.insert("toolEnabled", toolEnabled);
    result.insert("importStatus", persistedStatus);
    QVariantList modelBreakdown;
    QSqlQuery models(db);
    models.prepare(QStringLiteral(
        "SELECT e.model_id,COUNT(*),SUM(e.input_tokens),SUM(e.output_tokens) "
        "FROM local_usage_events e JOIN local_sessions s ON "
        "s.session_id=e.session_id WHERE s.tool_id=? AND e.observed_at_utc>=? "
        "AND e.observed_at_utc<? AND (?='' OR s.project_id=?) GROUP BY "
        "e.model_id ORDER BY COUNT(*) DESC LIMIT 100"));
    models.addBindValue(storedTool);
    models.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
    models.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
    models.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    models.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    if (models.exec())
      while (models.next()) {
        QVariantMap row;
        row.insert("modelId", models.value(0));
        row.insert("eventCount", models.value(1));
        row.insert("inputTokens", models.value(2));
        row.insert("outputTokens", models.value(3));
        modelBreakdown.append(row);
      }
    result.insert("modelBreakdown", modelBreakdown);
    QVariantList projectBreakdown;
    QSqlQuery groupedProjects(db);
    groupedProjects.prepare(QStringLiteral(
        "SELECT s.project_id,COUNT(DISTINCT "
        "s.session_id),SUM(e.input_tokens),SUM(e.output_tokens) FROM "
        "local_sessions s JOIN local_usage_events e ON "
        "e.session_id=s.session_id AND e.observed_at_utc>=? AND "
        "e.observed_at_utc<? WHERE s.tool_id=? AND (?='' OR s.project_id=?) "
        "GROUP BY "
        "s.project_id ORDER BY COUNT(DISTINCT s.session_id) DESC LIMIT 100"));
    groupedProjects.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
    groupedProjects.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
    groupedProjects.addBindValue(storedTool);
    groupedProjects.addBindValue(projectId.isEmpty() ? QStringLiteral("")
                                                     : projectId);
    groupedProjects.addBindValue(projectId.isEmpty() ? QStringLiteral("")
                                                     : projectId);
    if (groupedProjects.exec())
      while (groupedProjects.next()) {
        QVariantMap row;
        row.insert("projectId", groupedProjects.value(0));
        row.insert("sessionCount", groupedProjects.value(1));
        row.insert("inputTokens", groupedProjects.value(2));
        row.insert("outputTokens", groupedProjects.value(3));
        QSqlQuery a(db);
        a.prepare(QStringLiteral(
            "SELECT alias FROM local_project_aliases WHERE project_id=?"));
        a.addBindValue(groupedProjects.value(0));
        if (a.exec() && a.next())
          row.insert("alias", a.value(0));
        projectBreakdown.append(row);
      }
    result.insert("projectBreakdown", projectBreakdown);
    QSqlQuery p(db);
    p.prepare(QStringLiteral("SELECT DISTINCT s.project_id FROM local_sessions "
                             "s JOIN local_usage_events e "
                             "ON e.session_id=s.session_id WHERE s.tool_id=? "
                             "AND e.observed_at_utc>=? "
                             "AND e.observed_at_utc<? ORDER BY s.project_id"));
    p.addBindValue(storedTool);
    p.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
    p.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
    QVariantList projects;
    if (p.exec())
      while (p.next()) {
        QVariantMap x;
        x.insert("projectId", p.value(0));
        QSqlQuery a(db);
        a.prepare(QStringLiteral(
            "SELECT alias FROM local_project_aliases WHERE project_id=?"));
        a.addBindValue(p.value(0));
        if (a.exec() && a.next())
          x.insert("alias", a.value(0));
        projects.append(x);
      }
    result.insert("projectOptions", projects);
    result.insert("toolId", toolId);
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(c);
    return result;
  }));
}

void LocalSessionModel::requestDaily(const QString &requestId,
                                     const QString &toolId,
                                     const QDateTime &fromUtc,
                                     const QDateTime &toUtc,
                                     const QString &projectId,
                                     const QString &timeZoneId) {
  const QString storedTool = canonicalTool(toolId);
  const QTimeZone zone = timeZoneId.isEmpty() ? QTimeZone::systemTimeZone()
                                              : QTimeZone(timeZoneId.toUtf8());
  auto *watcher = new QFutureWatcher<QVariantList>(this);
  connect(watcher, &QFutureWatcher<QVariantList>::finished, this,
          [this, watcher, requestId]() {
            emit dailyReady(requestId, watcher->result());
            watcher->deleteLater();
          });
  watcher->setFuture(QtConcurrent::run([=]() {
    QVariantList rows;
    if (!zone.isValid() || !fromUtc.isValid() || !toUtc.isValid() ||
        toUtc <= fromUtc)
      return rows;
    const QDate first = fromUtc.toTimeZone(zone).date();
    const QDate last = toUtc.addMSecs(-1).toTimeZone(zone).date();
    if (!first.isValid() || !last.isValid() || first.daysTo(last) > 30)
      return rows;
    const QString connection = QStringLiteral("local_sessions_daily_%1")
                                   .arg(QRandomGenerator::global()->generate());
    auto db = openDb(connection);
    if (!db.isOpen())
      return rows;
    for (QDate date = first; date <= last; date = date.addDays(1)) {
      const QDateTime dayStart(date, QTime(0, 0), zone);
      const QDateTime dayEnd(date.addDays(1), QTime(0, 0), zone);
      const QDateTime lower = qMax(fromUtc.toUTC(), dayStart.toUTC());
      const QDateTime upper = qMin(toUtc.toUTC(), dayEnd.toUTC());
      if (upper <= lower)
        continue;
      QSqlQuery query(db);
      query.prepare(QStringLiteral(
          "SELECT COUNT(DISTINCT s.session_id),COUNT(e.event_id),SUM(e.input_"
          "tokens),SUM(e.output_tokens),SUM(e.cache_read_tokens),SUM(e.cache_"
          "write_tokens) FROM local_usage_events e JOIN local_sessions s ON "
          "s.session_id=e.session_id WHERE s.tool_id=? AND "
          "e.observed_at_utc>=? "
          "AND e.observed_at_utc<? AND (?='' OR s.project_id=?)"));
      query.addBindValue(storedTool);
      query.addBindValue(lower.toString(Qt::ISODateWithMs));
      query.addBindValue(upper.toString(Qt::ISODateWithMs));
      query.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
      query.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
      if (query.exec() && query.next()) {
        QVariantMap row;
        row.insert("localDate", date.toString(Qt::ISODate));
        row.insert("sessionCount", query.value(0));
        row.insert("eventCount", query.value(1));
        row.insert("inputTokens", query.value(2));
        row.insert("outputTokens", query.value(3));
        row.insert("cacheReadTokens", query.value(4));
        row.insert("cacheWriteTokens", query.value(5));
        rows.append(row);
      }
    }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
    return rows;
  }));
}

void LocalSessionModel::requestExport(
    const QString &requestId, const QString &toolId, const QDateTime &fromUtc,
    const QDateTime &toUtc, const QString &projectId, const QString &format,
    const QString &destinationPath) {
  const QString storedTool = canonicalTool(toolId);
  auto *w = new QFutureWatcher<QString>(this);
  connect(w, &QFutureWatcher<QString>::finished, this, [this, w, requestId]() {
    emit exportReady(requestId, w->result());
    w->deleteLater();
  });
  w->setFuture(QtConcurrent::run([=]() {
    if (format != QLatin1String("json") && format != QLatin1String("csv"))
      return QStringLiteral("session-export-format-invalid");
    QFileInfo destinationInfo(destinationPath);
    if (destinationPath.isEmpty() || destinationInfo.isDir() ||
        destinationInfo.isSymLink())
      return QStringLiteral("session-export-destination-invalid");
    QVariantList rows;
    QByteArray exportKey(32, Qt::Uninitialized);
    for (qsizetype index = 0; index < exportKey.size(); index += 4) {
      const quint32 random = QRandomGenerator::system()->generate();
      qToLittleEndian(random,
                      reinterpret_cast<uchar *>(exportKey.data() + index));
    }
    const QString c = QStringLiteral("local_sessions_export_%1")
                          .arg(QRandomGenerator::global()->generate());
    auto db = openDb(c);
    if (!db.isOpen())
      return QStringLiteral("session-database-unavailable");
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT "
        "s.session_id,s.project_id,e.observed_at_utc,e.model_id,e.input_"
        "tokens,e.output_tokens,e.cache_read_tokens,e.cache_write_tokens FROM "
        "local_sessions s JOIN local_usage_events e ON "
        "e.session_id=s.session_id WHERE s.tool_id=? AND e.observed_at_utc>=? "
        "AND e.observed_at_utc<? AND (?='' OR s.project_id=?) ORDER BY "
        "e.observed_at_utc"));
    q.addBindValue(storedTool);
    q.addBindValue(fromUtc.toUTC().toString(Qt::ISODateWithMs));
    q.addBindValue(toUtc.toUTC().toString(Qt::ISODateWithMs));
    q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
    if (q.exec())
      while (q.next()) {
        QVariantMap r;
        r.insert("sessionId",
                 QString::fromLatin1(LocalSessionParser::identity(
                     exportKey, "export-session", q.value(0).toByteArray())));
        r.insert("projectId",
                 QString::fromLatin1(LocalSessionParser::identity(
                     exportKey, "export-project", q.value(1).toByteArray())));
        r.insert("timestampUtc", q.value(2));
        r.insert("modelId", q.value(3));
        r.insert("inputTokens", q.value(4));
        r.insert("outputTokens", q.value(5));
        r.insert("cacheReadTokens", q.value(6));
        r.insert("cacheWriteTokens", q.value(7));
        rows.append(r);
      }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(c);
    QSaveFile file(destinationPath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
      return QStringLiteral("session-export-write-failed");
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QByteArray bytes;
    if (format == QLatin1String("json"))
      bytes = QJsonDocument(
                  QJsonObject{{"schemaVersion", 1},
                              {"sessions", QJsonArray::fromVariantList(rows)}})
                  .toJson(QJsonDocument::Indented);
    else {
      bytes = "sessionId,projectId,timestampUtc,modelId,inputTokens,"
              "outputTokens,cacheReadTokens,cacheWriteTokens\n";
      for (const auto &v : rows) {
        const auto r = v.toMap();
        QStringList fields;
        for (const auto &k : {"sessionId", "projectId", "timestampUtc",
                              "modelId", "inputTokens", "outputTokens",
                              "cacheReadTokens", "cacheWriteTokens"}) {
          QString s = r.value(QLatin1String(k)).toString();
          s.replace('"', "\"\"");
          fields << ('"' + s + '"');
        }
        bytes += fields.join(',').toUtf8() + "\n";
      }
    }
    if (file.write(bytes) != bytes.size() || !file.commit())
      return QStringLiteral("session-export-write-failed");
    return QString();
  }));
}

QString LocalSessionModel::createProjectAlias(const QUrl &directory,
                                              const QString &alias) {
  const QString name = alias.trimmed();
  if (!directory.isLocalFile() || name.isEmpty() || name.size() > 80 ||
      alias.contains(QChar::Null))
    return {};
  const QFileInfo folder(directory.toLocalFile());
  if (!folder.isAbsolute() || !folder.isDir())
    return {};
  const QByteArray key = sessionKey();
  if (key.isEmpty())
    return {};
  const QString cleanPath = QDir::cleanPath(folder.absoluteFilePath());
  QStringList paths{
      directory.toLocalFile(), cleanPath, cleanPath + QStringLiteral("/"),
      cleanPath + QStringLiteral("/."), cleanPath + QStringLiteral("/./")};
  paths.removeDuplicates();
  QStringList candidates;
  for (const QString &path : paths)
    candidates.append(QString::fromLatin1(
        LocalSessionParser::identity(key, "project", path.toUtf8())));
  QString projectId = QString::fromLatin1(
      LocalSessionParser::identity(key, "project", cleanPath.toUtf8()));
  const QString connection = QStringLiteral("local_sessions_alias_identity_%1")
                                 .arg(QRandomGenerator::global()->generate());
  bool queried = true;
  {
    auto db = openDb(connection);
    queried = db.isOpen();
    if (queried) {
      QString existingAlias;
      bool foundSessions = false;
      QSqlQuery query(db);
      query.prepare(QStringLiteral(
          "SELECT EXISTS(SELECT 1 FROM local_sessions WHERE project_id=?),"
          "EXISTS(SELECT 1 FROM local_project_aliases WHERE project_id=?)"));
      // Imported cwd values retain their original spelling. Prefer an existing
      // session identity, then a saved alias, without rewriting either table.
      for (const QString &candidate : candidates) {
        query.bindValue(0, candidate);
        query.bindValue(1, candidate);
        if (!query.exec() || !query.next()) {
          queried = false;
          break;
        }
        if (query.value(0).toBool()) {
          projectId = candidate;
          foundSessions = true;
          break;
        }
        if (existingAlias.isEmpty() && query.value(1).toBool())
          existingAlias = candidate;
      }
      if (queried && !foundSessions && !existingAlias.isEmpty())
        projectId = existingAlias;
    }
  }
  QSqlDatabase::removeDatabase(connection);
  return queried && setProjectAlias(projectId, name) ? projectId : QString();
}

QVariantList LocalSessionModel::projectAliases() const {
  const QString connection = QStringLiteral("local_sessions_aliases_%1")
                                 .arg(QRandomGenerator::global()->generate());
  QVariantList rows;
  {
    auto db = openDb(connection);
    if (db.isOpen()) {
      QSqlQuery query(db);
      if (query.exec(QStringLiteral(
              "SELECT a.project_id,a.alias,COUNT(DISTINCT s.tool_id || ':' || "
              "s.session_id) FROM local_project_aliases a LEFT JOIN "
              "local_sessions s ON s.project_id=a.project_id WHERE "
              "TRIM(a.alias)<>'' GROUP BY a.project_id,a.alias ORDER BY "
              "a.alias COLLATE NOCASE,a.alias,a.project_id"))) {
        while (query.next())
          rows.append(
              QVariantMap{{QStringLiteral("projectId"), query.value(0)},
                          {QStringLiteral("alias"), query.value(1)},
                          {QStringLiteral("sessionCount"), query.value(2)}});
      }
    }
  }
  QSqlDatabase::removeDatabase(connection);
  return rows;
}

bool LocalSessionModel::setProjectAlias(const QString &projectId,
                                        const QString &alias) {
  const QString name = alias.trimmed();
  if (projectId.size() != 64 ||
      !QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
           .match(projectId)
           .hasMatch() ||
      name.size() > 80 || alias.contains(QChar::Null))
    return false;
  const QString connection = QStringLiteral("local_sessions_alias_%1")
                                 .arg(QRandomGenerator::global()->generate());
  bool ok = false;
  {
    auto db = openDb(connection);
    if (db.isOpen()) {
      QSqlQuery query(db);
      query.prepare(
          name.isEmpty()
              ? QStringLiteral("DELETE FROM local_project_aliases "
                               "WHERE project_id=?")
              : QStringLiteral(
                    "INSERT INTO local_project_aliases(project_id,alias) "
                    "VALUES(?,?) ON CONFLICT(project_id) DO UPDATE SET "
                    "alias=excluded.alias"));
      query.addBindValue(projectId);
      if (!name.isEmpty())
        query.addBindValue(name);
      ok = query.exec();
    }
  }
  QSqlDatabase::removeDatabase(connection);
  return ok;
}
QString LocalSessionModel::projectAlias(const QString &projectId) const {
  const QString c = QStringLiteral("local_sessions_alias_read_%1")
                        .arg(QRandomGenerator::global()->generate());
  auto db = openDb(c);
  if (!db.isOpen())
    return {};
  QSqlQuery q(db);
  q.prepare(QStringLiteral(
      "SELECT alias FROM local_project_aliases WHERE project_id=?"));
  q.addBindValue(projectId.isEmpty() ? QStringLiteral("") : projectId);
  QString result;
  if (q.exec() && q.next())
    result = q.value(0).toString();
  db.close();
  db = {};
  QSqlDatabase::removeDatabase(c);
  return result;
}
void LocalSessionModel::clearSessions() { clearSessions(QString()); }
void LocalSessionModel::clearSessions(const QString &requestId) {
  ++m_generation;
  m_generationState->store(m_generation, std::memory_order_release);
  const QString lockPath =
      dataDirectory() + QStringLiteral("/session-import.lock");
  auto *w = new QFutureWatcher<QVariantMap>(this);
  connect(w, &QFutureWatcher<QVariantMap>::finished, this,
          [this, w, requestId]() {
            const auto r = w->result();
            emit clearFinished(requestId, r.value("ok").toBool(),
                               r.value("errorKey").toString());
            w->deleteLater();
          });
  w->setFuture(QtConcurrent::run([lockPath]() {
    QVariantMap result;
    QLockFile lock(lockPath);
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) {
      result.insert("errorKey", QStringLiteral("session-clear-busy"));
      return result;
    }
    const QString c = QStringLiteral("local_sessions_clear_%1")
                          .arg(QRandomGenerator::global()->generate());
    auto db = openDb(c);
    if (!db.isOpen()) {
      result.insert("errorKey", QStringLiteral("session-database-unavailable"));
      return result;
    }
    const bool tx = db.transaction();
    QSqlQuery q(db);
    const QString cutoff =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    bool ok = tx && q.exec(QStringLiteral("DELETE FROM local_sessions")) &&
              q.exec(QStringLiteral("DELETE FROM local_usage_conflicts")) &&
              q.exec(QStringLiteral("DELETE FROM local_file_checkpoints")) &&
              q.exec(QStringLiteral("DELETE FROM local_project_aliases"));
    for (const QString &tool :
         {QStringLiteral("codex"), QStringLiteral("claude-code")}) {
      q.prepare(QStringLiteral(
          "INSERT INTO "
          "local_import_checkpoints(tool_id,generation,cutoff_utc,updated_at_"
          "utc,coverage,rebaseline_pending) VALUES(?,1,?,?,'reported',1) "
          "ON CONFLICT(tool_id) DO UPDATE SET "
          "generation=generation+1,cutoff_utc=excluded.cutoff_utc,updated_at_"
          "utc=excluded.updated_at_utc,coverage='reported',"
          "rebaseline_pending=1"));
      q.addBindValue(tool);
      q.addBindValue(cutoff);
      q.addBindValue(cutoff);
      ok = ok && q.exec();
    }
    ok = ok && db.commit();
    if (!ok) {
      db.rollback();
      result.insert("errorKey", QStringLiteral("session-clear-failed"));
    } else
      result.insert("ok", true);
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(c);
    return result;
  }));
}
void LocalSessionModel::requestProjects(const QString &requestId) {
  auto *w = new QFutureWatcher<QVariantList>(this);
  connect(w, &QFutureWatcher<QVariantList>::finished, this,
          [this, w, requestId]() {
            emit projectsReady(requestId, w->result());
            w->deleteLater();
          });
  w->setFuture(QtConcurrent::run([]() {
    QVariantList rows;
    const QString c = QStringLiteral("local_sessions_projects_%1")
                          .arg(QRandomGenerator::global()->generate());
    auto db = openDb(c);
    if (!db.isOpen())
      return rows;
    QSqlQuery q(db);
    if (q.exec(QStringLiteral(
            "SELECT s.tool_id,s.project_id,COALESCE(a.alias,''),COUNT(DISTINCT "
            "s.session_id) FROM local_sessions s LEFT JOIN "
            "local_project_aliases a ON a.project_id=s.project_id GROUP BY "
            "s.tool_id,s.project_id ORDER BY s.tool_id,s.project_id")))
      while (q.next()) {
        QVariantMap r;
        r.insert("toolId", q.value(0).toString() == QLatin1String("codex")
                               ? QStringLiteral("codex-cli")
                               : q.value(0).toString());
        r.insert("projectId", q.value(1));
        r.insert("alias", q.value(2));
        r.insert("sessionCount", q.value(3));
        rows.append(r);
      }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(c);
    return rows;
  }));
}
