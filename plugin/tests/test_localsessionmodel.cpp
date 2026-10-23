#include "budgetpolicyschema.h"
#include "localsessionmodel.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>

class SessionFixture {
public:
  QTemporaryDir directory;
  QString root;
  QString claudeHome;
  QString database;
  bool valid = false;
  QMap<QByteArray, QByteArray> originalEnvironment;
  SessionFixture() {
    for (const auto &name :
         {QByteArray("HOME"), QByteArray("XDG_DATA_HOME"),
          QByteArray("CODEX_HOME"), QByteArray("CLAUDE_CONFIG_DIR")})
      originalEnvironment.insert(name, qgetenv(name.constData()));
    root = directory.filePath(QStringLiteral("codex/sessions"));
    claudeHome = directory.filePath(QStringLiteral("claude"));
    const QString data = directory.filePath(QStringLiteral("data"));
    database =
        data + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
    if (!QDir().mkpath(root) ||
        !QDir().mkpath(QFileInfo(database).absolutePath()))
      return;
    qputenv("HOME", QFile::encodeName(directory.path()));
    qputenv("XDG_DATA_HOME", QFile::encodeName(data));
    qputenv("CODEX_HOME",
            QFile::encodeName(directory.filePath(QStringLiteral("codex"))));
    qputenv("CLAUDE_CONFIG_DIR", QFile::encodeName(claudeHome));
    const QString connection = QStringLiteral("session_regression_seed");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(database);
    if (db.open()) {
      QSqlQuery query(db);
      QString error;
      valid = query.exec(QStringLiteral("PRAGMA user_version=8")) &&
              BudgetPolicySchema::migrateSessionSchema(db, &error);
    }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
  }
  ~SessionFixture() {
    for (auto it = originalEnvironment.cbegin();
         it != originalEnvironment.cend(); ++it) {
      if (it.value().isNull())
        qunsetenv(it.key().constData());
      else
        qputenv(it.key().constData(), it.value());
    }
  }
  QByteArray codexRecord(int input) const {
    return QJsonDocument(
               QJsonObject{
                   {"type", "event_msg"},
                   {"timestamp", QDateTime::currentDateTimeUtc().toString(
                                     Qt::ISODateWithMs)},
                   {"payload",
                    QJsonObject{
                        {"type", "token_count"},
                        {"info",
                         QJsonObject{
                             {"total_token_usage",
                              QJsonObject{{"input_tokens", input},
                                          {"output_tokens", 10},
                                          {"cached_input_tokens", 20}}}}}}}})
               .toJson(QJsonDocument::Compact) +
           '\n';
  }
  bool write(const QByteArray &bytes,
             QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QFile file(root + QStringLiteral("/session.jsonl"));
    return file.open(mode) && file.write(bytes) == bytes.size();
  }
  bool writeNamed(const QString &name, const QByteArray &bytes) {
    QFile file(root + QLatin1Char('/') + name);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
  }
};

class LocalSessionModelTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void oversizedRecordsDoNotBlockLaterUsageOrContextReplay() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    const QByteArray metadata =
        R"({"type":"session_meta","payload":{"id":"thread","cwd":"/private/project"}})";
    QVERIFY(fixture.write(metadata + '\n' + QByteArray(1024 * 1024 + 100, 'x') +
                          '\n' + fixture.codexRecord(100)));
    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled("codex", true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    QVERIFY(imports.takeFirst()[3].toString().isEmpty());
    QVERIFY(fixture.write(fixture.codexRecord(120), QIODevice::Append));
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    QVERIFY(imports.takeFirst()[3].toString().isEmpty());
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    const QString connection = "oversized-record-verify";
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          "SELECT COUNT(*),SUM(input_tokens) FROM local_usage_events"));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 2);
      QCOMPARE(query.value(1).toLongLong(), 120);
    }
    QSqlDatabase::removeDatabase(connection);
  }

  void codexAppendCacheRejectsAnInPlaceRewriteWithGrowth() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    const QString path = fixture.root + QStringLiteral("/session.jsonl");
    const QByteArray metadata =
        R"({"type":"session_meta","payload":{"id":"original-thread","cwd":"/original/project"}})";
    const QByteArray original = metadata + '\n' + fixture.codexRecord(100);
    QVERIFY(fixture.write(original));

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    QCOMPARE(imports.first().at(1).toInt(), 1);

    struct stat before{};
    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    QVERIFY(::fstat(beforeFile.handle(), &before) == 0);
    beforeFile.close();

    const QByteArray replacement =
        QByteArray("{\"padding\":\"") + QByteArray(original.size(), 'x') +
        QByteArray("\"}") + '\n' + fixture.codexRecord(250);
    QVERIFY(replacement.size() > original.size());
    QFile rewritten(path);
    QVERIFY(rewritten.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(rewritten.write(replacement), qint64(replacement.size()));
    QVERIFY(rewritten.setFileTime(QDateTime::currentDateTimeUtc().addSecs(3),
                                  QFileDevice::FileModificationTime));
    rewritten.close();
    struct stat after{};
    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QVERIFY(::fstat(afterFile.handle(), &after) == 0);
    afterFile.close();
    QCOMPARE(after.st_dev, before.st_dev);
    QCOMPARE(after.st_ino, before.st_ino);

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 2, 5000);
    QCOMPARE(imports.last().at(1).toInt(), 0);
    const QString connection = QStringLiteral("codex-rewrite-cache-verify");
    {
      auto db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          QStringLiteral("SELECT COUNT(*) FROM local_usage_events")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 1);
    }
    QSqlDatabase::removeDatabase(connection);
  }

  void importStatesAreToolSpecificAndKeepHistoricalGaps() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    QVERIFY(fixture.write(
        R"({"type":"session_meta","payload":{"id":"codex-thread","cwd":"/private/codex"}})" +
        QByteArray("\n") + fixture.codexRecord(100)));

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    QSignalSpy statesChanged(&model, &LocalSessionModel::importStatesChanged);
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    model.setToolEnabled(QStringLiteral("claude-code"), true);
    auto *timer =
        model.findChild<QTimer *>(QStringLiteral("local-session-import-timer"));
    QVERIFY(timer);
    timer->stop();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 30000);

    QVariantMap states = model.importStates();
    QCOMPARE(states.value(QStringLiteral("codex-cli"))
                 .toMap()
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("complete"));
    QCOMPARE(states.value(QStringLiteral("codex-cli"))
                 .toMap()
                 .value(QStringLiteral("importedEventCount"))
                 .toInt(),
             1);
    QCOMPARE(states.value(QStringLiteral("claude-code"))
                 .toMap()
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("partial"));
    QVERIFY(!states.value(QStringLiteral("codex-cli"))
                 .toMap()
                 .value(QStringLiteral("lastCompletedAtUtc"))
                 .toString()
                 .isEmpty());

    const QString claudeProjects =
        fixture.claudeHome + QStringLiteral("/projects/synthetic");
    QVERIFY(QDir().mkpath(claudeProjects));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QJsonObject usage{{QStringLiteral("input_tokens"), 12},
                            {QStringLiteral("output_tokens"), 3},
                            {QStringLiteral("cache_read_input_tokens"), 0},
                            {QStringLiteral("cache_creation_input_tokens"), 0}};
    const QJsonObject assistant{
        {QStringLiteral("type"), QStringLiteral("assistant")},
        {QStringLiteral("sessionId"), QStringLiteral("claude-session")},
        {QStringLiteral("cwd"), QStringLiteral("/private/claude")},
        {QStringLiteral("timestamp"), now.toString(Qt::ISODateWithMs)},
        {QStringLiteral("message"),
         QJsonObject{{QStringLiteral("id"), QStringLiteral("message-1")},
                     {QStringLiteral("model"), QStringLiteral("claude-sonnet")},
                     {QStringLiteral("usage"), usage}}}};
    QFile claudeLog(claudeProjects + QStringLiteral("/session.jsonl"));
    QVERIFY(claudeLog.open(QIODevice::WriteOnly));
    QCOMPARE(
        claudeLog.write(
            QJsonDocument(assistant).toJson(QJsonDocument::Compact) + '\n'),
        QJsonDocument(assistant).toJson(QJsonDocument::Compact).size() + 1);
    claudeLog.close();

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 2, 30000);
    states = model.importStates();
    QCOMPARE(states.value(QStringLiteral("claude-code"))
                 .toMap()
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("complete"));
    QCOMPARE(states.value(QStringLiteral("claude-code"))
                 .toMap()
                 .value(QStringLiteral("importedEventCount"))
                 .toInt(),
             1);
    QVERIFY(statesChanged.size() >= 4);

    QSignalSpy summaries(&model, &LocalSessionModel::summaryReady);
    model.requestSummary(QStringLiteral("coverage-after-recovery"),
                         QStringLiteral("claude-code"), now.addDays(-1),
                         now.addDays(1));
    QTRY_COMPARE_WITH_TIMEOUT(summaries.size(), 1, 5000);
    QCOMPARE(summaries.first()
                 .at(1)
                 .toMap()
                 .value(QStringLiteral("toolCoverage"))
                 .toString(),
             QStringLiteral("partial"));

    model.setToolEnabled(QStringLiteral("codex-cli"), false);
    QCOMPARE(model.importStates()
                 .value(QStringLiteral("codex-cli"))
                 .toMap()
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("disabled"));
    QTRY_COMPARE_WITH_TIMEOUT(model.pendingWorkerCount(), 0, 30000);
  }

  void largeImportsContinuePastTheFirstFiveThousandFiles() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    constexpr int fileCount = 5002;
    const QByteArray newline("\n");
    const QByteArray record = fixture.codexRecord(1);
    for (int i = 0; i < fileCount; ++i) {
      const QString id =
          QStringLiteral("thread-%1").arg(i, 5, 10, QLatin1Char('0'));
      const QByteArray metadata =
          QJsonDocument(
              QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("session_meta")},
                  {QStringLiteral("payload"),
                   QJsonObject{
                       {QStringLiteral("id"), id},
                       {QStringLiteral("cwd"), QStringLiteral("/synthetic")}}}})
              .toJson(QJsonDocument::Compact);
      QVERIFY2(
          fixture.writeNamed(id + QStringLiteral(".jsonl"),
                             metadata + newline + record),
          qPrintable(QStringLiteral("Failed to create fixture %1").arg(i)));
    }

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    auto *timer =
        model.findChild<QTimer *>(QStringLiteral("local-session-import-timer"));
    QVERIFY(timer);
    timer->stop();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 120000);
    QVERIFY(imports.first().at(2).toBool());

    auto countEvents = [&fixture]() {
      const QString connection = QStringLiteral("large-session-import-count");
      auto db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      db.setDatabaseName(fixture.database);
      if (!db.open())
        return qint64(-1);
      QSqlQuery query(db);
      const bool ok = query.exec(QStringLiteral(
                          "SELECT COUNT(*) FROM local_usage_events")) &&
                      query.next();
      const qint64 count = ok ? query.value(0).toLongLong() : -1;
      query.finish();
      db.close();
      db = {};
      QSqlDatabase::removeDatabase(connection);
      return count;
    };
    QCOMPARE(countEvents(), qint64(5000));

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 2, 120000);
    QVERIFY(!imports.last().at(2).toBool());
    QCOMPARE(countEvents(), qint64(5002));

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 3, 30000);
    QVERIFY(!imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 0);
    QCOMPARE(countEvents(), qint64(5002));

    const QByteArray metadata =
        R"({"type":"session_meta","payload":{"id":"late-thread","cwd":"/synthetic"}})";
    QVERIFY(fixture.writeNamed(QStringLiteral("late-session.jsonl"),
                               metadata + newline + record));
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 4, 30000);
    QCOMPARE(imports.last().at(1).toInt(), 1);
    QCOMPARE(countEvents(), qint64(5003));

    QDir sessionDirectory(fixture.root);
    const QStringList existingFiles =
        sessionDirectory.entryList({QStringLiteral("*.jsonl")}, QDir::Files);
    QCOMPARE(existingFiles.size(), fileCount + 1);
    const QDateTime changedAt = QDateTime::currentDateTimeUtc().addSecs(120);
    for (const QString &name : existingFiles) {
      QFile changedFile(sessionDirectory.filePath(name));
      QVERIFY(changedFile.open(QIODevice::ReadWrite));
      QVERIFY(changedFile.setFileTime(changedAt,
                                      QFileDevice::FileModificationTime));
    }
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 5, 120000);
    QVERIFY(imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 0);
    QCOMPARE(countEvents(), qint64(5003));

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 6, 120000);
    QVERIFY(imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 0);
    QCOMPARE(countEvents(), qint64(5003));

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 7, 30000);
    QVERIFY(!imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 0);
    QCOMPARE(countEvents(), qint64(5003));
    QCOMPARE(model.importStates()
                 .value(QStringLiteral("codex-cli"))
                 .toMap()
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("complete"));
  }

  void persistentRejectedNewFilesCannotStarveLaterValidFiles() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    constexpr int rejectedFileCount = 5000;
    constexpr qint64 oversizedFileSize = 100ll * 1024 * 1024 + 1;
    const QDateTime rejectedModified =
        QDateTime::currentDateTimeUtc().addSecs(120);
    const QDateTime validModified =
        QDateTime::currentDateTimeUtc().addSecs(-120);

    const QByteArray metadata =
        R"({"type":"session_meta","payload":{"id":"valid-thread","cwd":"/synthetic"}})";
    QVERIFY(fixture.writeNamed(QStringLiteral("z-valid.jsonl"),
                               metadata + '\n' + fixture.codexRecord(42)));
    QFile validFile(fixture.root + QStringLiteral("/z-valid.jsonl"));
    QVERIFY(validFile.open(QIODevice::ReadOnly));
    QVERIFY(validFile.setFileTime(validModified,
                                  QFileDevice::FileModificationTime));
    validFile.close();

    for (int i = 0; i < rejectedFileCount; ++i) {
      const QString path = fixture.root + QStringLiteral("/a-rejected-%1.jsonl")
                                              .arg(i, 5, 10, QLatin1Char('0'));
      QFile rejected(path);
      QVERIFY2(rejected.open(QIODevice::WriteOnly), qPrintable(path));
      QVERIFY(rejected.resize(oversizedFileSize));
      QVERIFY(rejected.setFileTime(rejectedModified,
                                   QFileDevice::FileModificationTime));
    }

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    auto *timer =
        model.findChild<QTimer *>(QStringLiteral("local-session-import-timer"));
    QVERIFY(timer);
    timer->stop();

    auto countEvents = [&fixture]() {
      const QString connection =
          QStringLiteral("rejected-new-file-event-count");
      auto db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      db.setDatabaseName(fixture.database);
      if (!db.open())
        return qint64(-1);
      QSqlQuery query(db);
      const bool ok = query.exec(QStringLiteral(
                          "SELECT COUNT(*) FROM local_usage_events")) &&
                      query.next();
      const qint64 count = ok ? query.value(0).toLongLong() : -1;
      query.finish();
      db.close();
      db = {};
      QSqlDatabase::removeDatabase(connection);
      return count;
    };

    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 30000);
    QVERIFY(imports.first().at(2).toBool());
    QCOMPARE(countEvents(), qint64(0));
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 2, 30000);
    QVERIFY(imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 1);
    QCOMPARE(countEvents(), qint64(1));

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 3, 30000);
    QCOMPARE(imports.last().at(1).toInt(), 0);
    QCOMPARE(countEvents(), qint64(1));
  }

  void mixedRejectedQueuesCannotStarveLaterValidNewFiles() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    constexpr int changedFileCount = 1000;
    constexpr int rejectedNewFileCount = 4000;
    constexpr qint64 oversizedFileSize = 100ll * 1024 * 1024 + 1;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QByteArray newline("\n");

    for (int i = 0; i < changedFileCount; ++i) {
      const QString id =
          QStringLiteral("changed-thread-%1").arg(i, 4, 10, QLatin1Char('0'));
      const QByteArray metadata =
          QJsonDocument(
              QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("session_meta")},
                  {QStringLiteral("payload"),
                   QJsonObject{
                       {QStringLiteral("id"), id},
                       {QStringLiteral("cwd"), QStringLiteral("/synthetic")}}}})
              .toJson(QJsonDocument::Compact);
      QVERIFY(fixture.writeNamed(id + QStringLiteral(".jsonl"),
                                 metadata + newline + fixture.codexRecord(1)));
    }

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    auto *timer =
        model.findChild<QTimer *>(QStringLiteral("local-session-import-timer"));
    QVERIFY(timer);
    timer->stop();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 30000);
    QCOMPARE(imports.last().at(1).toInt(), changedFileCount);

    QDir sessionDirectory(fixture.root);
    const QStringList checkpointed =
        sessionDirectory.entryList({QStringLiteral("*.jsonl")}, QDir::Files);
    QCOMPARE(checkpointed.size(), changedFileCount);
    for (const QString &name : checkpointed) {
      QFile changed(sessionDirectory.filePath(name));
      QVERIFY(changed.open(QIODevice::ReadWrite));
      QVERIFY(changed.resize(oversizedFileSize));
      QVERIFY(changed.setFileTime(now.addSecs(120),
                                  QFileDevice::FileModificationTime));
    }

    const QByteArray validMetadata =
        R"({"type":"session_meta","payload":{"id":"valid-later","cwd":"/synthetic"}})";
    QVERIFY(
        fixture.writeNamed(QStringLiteral("z-valid-later.jsonl"),
                           validMetadata + newline + fixture.codexRecord(42)));
    QFile validFile(
        sessionDirectory.filePath(QStringLiteral("z-valid-later.jsonl")));
    QVERIFY(validFile.open(QIODevice::ReadOnly));
    QVERIFY(validFile.setFileTime(now.addSecs(-120),
                                  QFileDevice::FileModificationTime));
    validFile.close();

    for (int i = 0; i < rejectedNewFileCount; ++i) {
      const QString path =
          sessionDirectory.filePath(QStringLiteral("a-rejected-%1.jsonl")
                                        .arg(i, 4, 10, QLatin1Char('0')));
      QFile rejected(path);
      QVERIFY(rejected.open(QIODevice::WriteOnly));
      QVERIFY(rejected.resize(oversizedFileSize));
      QVERIFY(rejected.setFileTime(now.addSecs(240),
                                   QFileDevice::FileModificationTime));
    }

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 2, 30000);
    QVERIFY(imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 0);
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 3, 30000);
    QCOMPARE(imports.last().at(1).toInt(), 1);
    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 4, 30000);
    QCOMPARE(imports.last().at(1).toInt(), 0);

    const QString connection =
        QStringLiteral("mixed-rejected-file-event-count");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(fixture.database);
    QVERIFY(db.open());
    {
      QSqlQuery query(db);
      QVERIFY(query.exec(
          QStringLiteral("SELECT COUNT(*) FROM local_usage_events")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), changedFileCount + 1);
    }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
  }

  void byteLimitedImportsResumeWithoutRepeatingCompletedFiles() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    constexpr qint64 sparseFileSize = 90ll * 1024 * 1024;
    for (int i = 0; i < 3; ++i) {
      QFile file(fixture.root + QStringLiteral("/large-%1.jsonl").arg(i));
      QVERIFY(file.open(QIODevice::WriteOnly));
      QVERIFY(file.resize(sparseFileSize));
    }

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    auto *timer =
        model.findChild<QTimer *>(QStringLiteral("local-session-import-timer"));
    QVERIFY(timer);
    timer->stop();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 30000);
    QVERIFY(imports.first().at(2).toBool());
    auto completedCheckpoints = [&fixture]() {
      const QString connection = QStringLiteral("byte-limit-checkpoint-count");
      auto db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      db.setDatabaseName(fixture.database);
      if (!db.open())
        return -1;
      QSqlQuery query(db);
      const bool ok = query.exec(QStringLiteral(
                          "SELECT COUNT(*) FROM local_file_checkpoints WHERE "
                          "tool_id='codex' "
                          "AND file_offset=file_size")) &&
                      query.next();
      const int count = ok ? query.value(0).toInt() : -1;
      query.finish();
      db.close();
      db = {};
      QSqlDatabase::removeDatabase(connection);
      return count;
    };
    QVERIFY(completedCheckpoints() < 3);

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 2, 30000);
    QVERIFY(imports.last().at(2).toBool());
    QCOMPARE(completedCheckpoints(), 3);

    model.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 3, 30000);
    QVERIFY(!imports.last().at(2).toBool());
    QCOMPARE(imports.last().at(1).toInt(), 0);
    QCOMPARE(completedCheckpoints(), 3);
    QCOMPARE(model.importStates()
                 .value(QStringLiteral("codex-cli"))
                 .toMap()
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("complete"));
  }

  void clearPolicySurvivesEmptyScansAndRestart() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    const QByteArray metadata =
        R"({"type":"session_meta","payload":{"id":"thread","cwd":"/private/project"}})";
    QVERIFY(fixture.write(metadata + '\n' + fixture.codexRecord(100)));
    {
      LocalSessionModel model;
      QSignalSpy imports(&model, &LocalSessionModel::importFinished);
      QSignalSpy clears(&model, &LocalSessionModel::clearFinished);
      model.setToolEnabled("codex", true);
      QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
      model.clearSessions("clear");
      QTRY_COMPARE_WITH_TIMEOUT(clears.size(), 1, 5000);
      QVERIFY(clears.first()[1].toBool());
      for (int i = 0; i < 2; ++i) {
        imports.clear();
        model.requestImport();
        QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
      }
    }
    // Simulate an older importer that prematurely cleared the pending flag.
    const QString legacyConnection = "clear-legacy-pending-seed";
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", legacyConnection);
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          "UPDATE local_import_checkpoints SET rebaseline_pending=0"));
    }
    QSqlDatabase::removeDatabase(legacyConnection);
    QTest::qWait(5);
    LocalSessionModel restarted;
    QSignalSpy imports(&restarted, &LocalSessionModel::importFinished);
    restarted.setToolEnabled("codex", true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    for (int total : {1000, 1020}) {
      imports.clear();
      QVERIFY(fixture.write(fixture.codexRecord(total), QIODevice::Append));
      restarted.requestImport();
      QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
      QVERIFY(imports.first()[3].toString().isEmpty());
    }
    // An old session discovered only after other sessions resumed still needs
    // its own baseline; completing another session must not end the policy.
    QFile delayed(fixture.root + "/delayed.jsonl");
    QVERIFY(delayed.open(QIODevice::WriteOnly));
    const QByteArray delayedMetadata =
        QJsonDocument(
            QJsonObject{
                {"type", "session_meta"},
                {"payload",
                 QJsonObject{{"id", "delayed-thread"},
                             {"cwd", "/private/delayed"},
                             {"timestamp", QDateTime::currentDateTimeUtc()
                                               .addDays(-1)
                                               .toString(Qt::ISODateWithMs)}}}})
            .toJson(QJsonDocument::Compact) +
        '\n';
    const QByteArray delayedFirst = delayedMetadata + fixture.codexRecord(300);
    QCOMPARE(delayed.write(delayedFirst), delayedFirst.size());
    delayed.close();
    imports.clear();
    restarted.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    QVERIFY(delayed.open(QIODevice::Append));
    const QByteArray delayedIncrease = fixture.codexRecord(340);
    QCOMPARE(delayed.write(delayedIncrease), delayedIncrease.size());
    delayed.close();
    imports.clear();
    restarted.requestImport();
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    const QString connection = "clear-empty-scan-verify";
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          "SELECT COUNT(*),SUM(input_tokens) FROM local_usage_events"));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 2);
      QCOMPARE(query.value(1).toLongLong(), 60);
    }
    QSqlDatabase::removeDatabase(connection);
  }

  void unprovenCreationMetadataCannotRestoreClearedTotals_data() {
    QTest::addColumn<QString>("proof");
    QTest::newRow("missing-timezone") << QStringLiteral("missing-timezone");
    QTest::newRow("mismatched-session") << QStringLiteral("mismatched-session");
    QTest::newRow("future-creation") << QStringLiteral("future-creation");
  }

  void unprovenCreationMetadataCannotRestoreClearedTotals() {
    QFETCH(QString, proof);
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    LocalSessionModel model;
    QSignalSpy clears(&model, &LocalSessionModel::clearFinished);
    model.clearSessions("clear-proof");
    QTRY_COMPARE_WITH_TIMEOUT(clears.size(), 1, 5000);
    QVERIFY(clears.first()[1].toBool());
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QString created = now.addSecs(proof == "future-creation" ? 20 : 1)
                          .toString(Qt::ISODateWithMs);
    if (proof == "missing-timezone")
      created.chop(1);
    const QByteArray metadata =
        QJsonDocument(
            QJsonObject{
                {"type", "session_meta"},
                {"payload", QJsonObject{{"id", proof == "mismatched-session"
                                                   ? "different-thread"
                                                   : "thread"},
                                        {"cwd", "/private/project"},
                                        {"timestamp", created}}}})
            .toJson(QJsonDocument::Compact) +
        '\n';
    const auto usage = [&](int total, int seconds) {
      QJsonObject row =
          QJsonDocument::fromJson(fixture.codexRecord(total)).object();
      row.insert("thread_id", "thread");
      row.insert("timestamp", now.addSecs(seconds).toString(Qt::ISODateWithMs));
      return QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
    };
    QVERIFY(fixture.write(metadata + usage(1000, 10) + usage(1020, 11)));
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled("codex", true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
    QVERIFY(imports.first()[3].toString().isEmpty());
    const QString connection = "clear-proof-verify";
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          "SELECT COUNT(*),SUM(input_tokens) FROM local_usage_events"));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 1);
      QCOMPARE(query.value(1).toLongLong(), 20);
      QVERIFY(query.exec("SELECT coverage FROM local_sessions"));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toString(),
               QStringLiteral("partial-clear-rebaseline"));
    }
    QSqlDatabase::removeDatabase(connection);
  }

  void modelFiltersAgreeAcrossQueriesAndExports() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", "filter-seed");
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery q(db);
      QVERIFY(q.exec("INSERT INTO "
                     "local_sessions(session_id,tool_id,project_id,started_at_"
                     "utc,last_seen_at_utc) "
                     "VALUES('mixed','codex','private-project','2026-10-10T12:"
                     "00:00.000Z','2026-10-10T12:00:00.000Z')"));
      QVERIFY(q.exec("INSERT INTO "
                     "local_sessions(session_id,tool_id,project_id,started_at_"
                     "utc,last_seen_at_utc) "
                     "VALUES('other-tool','claude-code','private-project','"
                     "2026-10-10T12:00:00.000Z','2026-10-10T12:00:00.000Z'),('"
                     "other-project','codex','other-project','2026-10-10T12:00:"
                     "00.000Z','2026-10-10T12:00:00.000Z')"));
      QVERIFY(q.exec("INSERT INTO "
                     "local_usage_events(event_id,session_id,observed_at_utc,"
                     "model_id,input_tokens,parser_revision) "
                     "VALUES('other-tool','other-tool','2026-10-10T12:00:00."
                     "000Z','model-a',900,1),('other-project','other-project','"
                     "2026-10-10T12:00:00.000Z','model-a',800,1)"));
      QVERIFY(q.exec("INSERT INTO local_project_aliases(project_id,alias) "
                     "VALUES('private-project','Private alias')"));
      QVERIFY(
          q.exec("INSERT INTO "
                 "local_usage_events(event_id,session_id,observed_at_utc,model_"
                 "id,input_tokens,output_tokens,parser_revision) "
                 "VALUES('a','mixed','2026-10-10T12:00:00.000Z','model-a',10,2,"
                 "1),('b','mixed','2026-10-10T12:01:00.000Z','model-b',20,3,1),"
                 "('u','mixed','2026-10-10T12:02:00.000Z','',NULL,NULL,1)"));
    }
    QSqlDatabase::removeDatabase("filter-seed");
    LocalSessionModel model;
    QSignalSpy summaries(&model, &LocalSessionModel::summaryReady);
    QSignalSpy sessions(&model, &LocalSessionModel::sessionsReady);
    QSignalSpy daily(&model, &LocalSessionModel::dailyReady);
    QSignalSpy exports(&model, &LocalSessionModel::exportReady);
    QSignalSpy failures(&model, &LocalSessionModel::queryFailed);
    const auto from =
        QDateTime::fromString("2026-10-10T00:00:00.000Z", Qt::ISODateWithMs);
    const auto to = from.addDays(1);
    const QList<QVariantMap> filters{{},
                                     {{"modelId", QStringLiteral("model-a")}},
                                     {{"modelId", QStringLiteral("")}},
                                     {{"modelId", QStringLiteral("absent")}}};
    const QList<int> counts{3, 1, 1, 0};
    for (int i = 0; i < filters.size(); ++i) {
      summaries.clear();
      sessions.clear();
      daily.clear();
      exports.clear();
      const auto filter = filters[i];
      model.requestSummary("summary", "codex-cli", from, to, "private-project",
                           filter);
      model.requestSessions("sessions", "codex", from, to, "private-project", 0,
                            100, filter);
      model.requestDaily("daily", "codex", from, to, "private-project", "UTC",
                         filter);
      const QString output =
          fixture.directory.filePath(QStringLiteral("export-%1.json").arg(i));
      model.requestExport("export", "codex", from, to, "private-project",
                          "json", output, filter);
      QTRY_COMPARE_WITH_TIMEOUT(summaries.size(), 1, 5000);
      QTRY_COMPARE_WITH_TIMEOUT(sessions.size(), 1, 5000);
      QTRY_COMPARE_WITH_TIMEOUT(daily.size(), 1, 5000);
      QTRY_COMPARE_WITH_TIMEOUT(exports.size(), 1, 5000);
      QCOMPARE(failures.size(), 0);
      const auto summary = summaries.first()[1].toMap();
      QCOMPARE(summary.value("eventCount").toInt(), counts[i]);
      QCOMPARE(summary.value("sessionCount").toInt(), counts[i] ? 1 : 0);
      QCOMPARE(summary.value("modelOptions").toList().size(), 3);
      const auto rows = sessions.first()[1].toList();
      QCOMPARE(rows.size(), counts[i] ? 1 : 0);
      if (!rows.isEmpty()) {
        const auto row = rows.first().toMap();
        QCOMPARE(row.value("eventCount").toInt(), counts[i]);
        QCOMPARE(row.value("inputTokens"), summary.value("inputTokens"));
        QCOMPARE(row.value("modelBreakdownTotalCount").toInt(), i == 0 ? 3 : 1);
        if (i == 2)
          QVERIFY(row.value("inputTokens").isNull());
      }
      const auto day = daily.first()[1].toList().first().toMap();
      QCOMPARE(day.value("eventCount").toInt(), counts[i]);
      QCOMPARE(day.value("inputTokens"), summary.value("inputTokens"));
      QVERIFY(exports.first()[1].toString().isEmpty());
      QFile file(output);
      QVERIFY(file.open(QIODevice::ReadOnly));
      const auto bytes = file.readAll();
      QVERIFY(!bytes.contains("private-project"));
      QVERIFY(!bytes.contains("Private alias"));
      QVERIFY(!bytes.contains(fixture.directory.path().toUtf8()));
      const auto exported =
          QJsonDocument::fromJson(bytes).object().value("sessions").toArray();
      QCOMPARE(exported.size(), counts[i]);
      if (i == 1)
        QCOMPARE(exported.first().toObject().value("modelId").toString(),
                 QStringLiteral("model-a"));
      if (i == 2)
        QVERIFY(exported.first().toObject().value("inputTokens").isNull());
      const QString csv =
          fixture.directory.filePath(QStringLiteral("export-%1.csv").arg(i));
      exports.clear();
      model.requestExport("csv", "codex", from, to, "private-project", "csv",
                          csv, filter);
      QTRY_COMPARE_WITH_TIMEOUT(exports.size(), 1, 5000);
      QVERIFY(exports.first()[1].toString().isEmpty());
      QFile csvFile(csv);
      QVERIFY(csvFile.open(QIODevice::ReadOnly));
      QCOMPARE(csvFile.readAll().count('\n'), counts[i] + 1);
    }
  }

  void dailyQueriesUseInclusiveLocalDaysAcrossDstChanges() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    {
      auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                          QStringLiteral("dst-session-seed"));
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(QStringLiteral(
          "INSERT INTO local_sessions(session_id,tool_id,project_id,started_at_"
          "utc,last_seen_at_utc) VALUES"
          "('spring-before','codex','','2026-03-28T22:59:59.999Z','2026-03-"
          "28T22:59:59.999Z'),"
          "('spring-lower','codex','','2026-03-28T23:00:00.000Z','2026-03-"
          "28T23:00:00.000Z'),"
          "('spring-mid','codex','','2026-03-29T22:30:00.000Z','2026-03-"
          "29T22:30:00.000Z'),"
          "('spring-last','codex','','2026-03-30T21:59:59.999Z','2026-03-"
          "30T21:59:59.999Z'),"
          "('spring-end','codex','','2026-03-30T22:00:00.000Z','2026-03-"
          "30T22:00:00.000Z'),"
          "('autumn-start','codex','','2026-10-24T22:00:00.000Z','2026-10-"
          "24T22:00:00.000Z'),"
          "('autumn-mid','codex','','2026-10-25T01:30:00.000Z','2026-10-"
          "25T01:30:00.000Z'),"
          "('autumn-last','codex','','2026-10-26T22:59:59.999Z','2026-10-"
          "26T22:59:59.999Z'),"
          "('autumn-end','codex','','2026-10-26T23:00:00.000Z','2026-10-"
          "26T23:00:00.000Z')")));
      QVERIFY(query.exec(QStringLiteral(
          "INSERT INTO local_usage_events(event_id,session_id,observed_at_utc,"
          "model_id,input_tokens,output_tokens,parser_revision) VALUES"
          "('spring-before','spring-before','2026-03-28T22:59:59.999Z','m',"
          "500,1,1),"
          "('spring-lower','spring-lower','2026-03-28T23:00:00.000Z','m',"
          "10,1,1),"
          "('spring-mid','spring-mid','2026-03-29T22:30:00.000Z','m',20,1,1),"
          "('spring-last','spring-last','2026-03-30T21:59:59.999Z','m',30,1,1),"
          "('spring-end','spring-end','2026-03-30T22:00:00.000Z','m',999,1,1),"
          "('autumn-start','autumn-start','2026-10-24T22:00:00.000Z','m',"
          "10,1,1),"
          "('autumn-mid','autumn-mid','2026-10-25T01:30:00.000Z','m',20,1,1),"
          "('autumn-last','autumn-last','2026-10-26T22:59:59.999Z','m',30,1,1),"
          "('autumn-end','autumn-end','2026-10-26T23:00:00.000Z','m',999,1,"
          "1)")));
      query.finish();
      db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("dst-session-seed"));

    LocalSessionModel model;
    QSignalSpy summaries(&model, &LocalSessionModel::summaryReady);
    QSignalSpy sessions(&model, &LocalSessionModel::sessionsReady);
    QSignalSpy daily(&model, &LocalSessionModel::dailyReady);
    QSignalSpy exports(&model, &LocalSessionModel::exportReady);
    const auto springFrom = QDateTime::fromString(
        QStringLiteral("2026-03-28T23:00:00.000Z"), Qt::ISODateWithMs);
    const auto springTo = QDateTime::fromString(
        QStringLiteral("2026-03-30T22:00:00.000Z"), Qt::ISODateWithMs);
    const auto autumnFrom = QDateTime::fromString(
        QStringLiteral("2026-10-24T22:00:00.000Z"), Qt::ISODateWithMs);
    const auto autumnTo = QDateTime::fromString(
        QStringLiteral("2026-10-26T23:00:00.000Z"), Qt::ISODateWithMs);
    QCOMPARE(springFrom.secsTo(springTo), 47 * 60 * 60);
    QCOMPARE(autumnFrom.secsTo(autumnTo), 49 * 60 * 60);

    model.requestSummary(QStringLiteral("spring-summary"),
                         QStringLiteral("codex"), springFrom, springTo);
    model.requestSessions(QStringLiteral("spring-sessions"),
                          QStringLiteral("codex-cli"), springFrom, springTo);
    model.requestDaily(QStringLiteral("spring-daily"), QStringLiteral("codex"),
                       springFrom, springTo, {},
                       QStringLiteral("Europe/Stockholm"));
    model.requestDaily(QStringLiteral("autumn-daily"), QStringLiteral("codex"),
                       autumnFrom, autumnTo, {},
                       QStringLiteral("Europe/Stockholm"));
    const QString output =
        fixture.directory.filePath(QStringLiteral("spring.json"));
    model.requestExport(QStringLiteral("spring-export"),
                        QStringLiteral("codex"), springFrom, springTo, {},
                        QStringLiteral("json"), output);
    QTRY_COMPARE_WITH_TIMEOUT(summaries.size(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(sessions.size(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(daily.size(), 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(exports.size(), 1, 5000);

    const QVariantMap springSummary = summaries.first().at(1).toMap();
    QCOMPARE(springSummary.value(QStringLiteral("eventCount")).toInt(), 3);
    QCOMPARE(springSummary.value(QStringLiteral("inputTokens")).toLongLong(),
             60);
    QCOMPARE(sessions.first().at(1).toList().size(), 3);
    QVERIFY(exports.first().at(1).toString().isEmpty());
    QFile outputFile(output);
    QVERIFY(outputFile.open(QIODevice::ReadOnly));
    const auto exported = QJsonDocument::fromJson(outputFile.readAll())
                              .object()
                              .value(QStringLiteral("sessions"))
                              .toArray();
    QCOMPARE(exported.size(), 3);

    QHash<QString, QVariantList> dailyByRequest;
    for (const auto &reply : daily)
      dailyByRequest.insert(reply.at(0).toString(), reply.at(1).toList());
    const QVariantList springRows =
        dailyByRequest.value(QStringLiteral("spring-daily"));
    QCOMPARE(springRows.size(), 2);
    QCOMPARE(
        springRows.at(0).toMap().value(QStringLiteral("localDate")).toString(),
        QStringLiteral("2026-03-29"));
    QCOMPARE(
        springRows.at(0).toMap().value(QStringLiteral("eventCount")).toInt(),
        1);
    QCOMPARE(springRows.at(0)
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             10);
    QCOMPARE(
        springRows.at(1).toMap().value(QStringLiteral("localDate")).toString(),
        QStringLiteral("2026-03-30"));
    QCOMPARE(
        springRows.at(1).toMap().value(QStringLiteral("eventCount")).toInt(),
        2);
    QCOMPARE(springRows.at(1)
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             50);

    const QVariantList autumnRows =
        dailyByRequest.value(QStringLiteral("autumn-daily"));
    QCOMPARE(autumnRows.size(), 2);
    QCOMPARE(
        autumnRows.at(0).toMap().value(QStringLiteral("localDate")).toString(),
        QStringLiteral("2026-10-25"));
    QCOMPARE(
        autumnRows.at(0).toMap().value(QStringLiteral("eventCount")).toInt(),
        2);
    QCOMPARE(autumnRows.at(0)
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             30);
    QCOMPARE(
        autumnRows.at(1).toMap().value(QStringLiteral("localDate")).toString(),
        QStringLiteral("2026-10-26"));
    QCOMPARE(
        autumnRows.at(1).toMap().value(QStringLiteral("eventCount")).toInt(),
        1);
    QCOMPARE(autumnRows.at(1)
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             30);
  }

  void queryErrorsAreDistinctFromEmptyAndRecover() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    LocalSessionModel model;
    QSignalSpy summaries(&model, &LocalSessionModel::summaryReady);
    QSignalSpy sessions(&model, &LocalSessionModel::sessionsReady);
    QSignalSpy daily(&model, &LocalSessionModel::dailyReady);
    QSignalSpy projects(&model, &LocalSessionModel::projectsReady);
    QSignalSpy failures(&model, &LocalSessionModel::queryFailed);
    const auto from =
        QDateTime::fromString("2026-10-10T00:00:00.000Z", Qt::ISODateWithMs);
    const auto to = from.addDays(1);
    model.requestSummary("empty", "codex", from, to);
    QTRY_COMPARE(summaries.size(), 1);
    QCOMPARE(summaries.first()[1].toMap().value("sessionCount").toInt(), 0);
    model.requestSessions("invalid", "invalid-tool", from, to);
    model.requestSummary("invalid-time", "codex", to, from);
    model.requestDaily("invalid-zone", "codex", from, to, {}, "invalid-zone");
    QTRY_COMPARE(failures.size(), 3);
    for (const auto &failure : failures)
      QCOMPARE(failure[1].toString(), QStringLiteral("session-invalid-query"));
    failures.clear();
    summaries.clear();
    QVERIFY(QFile::rename(fixture.database, fixture.database + ".saved"));
    QVERIFY(QDir().mkdir(fixture.database));
    model.requestSessions("unavailable", "codex", from, to);
    model.requestSummary("unavailable", "codex", from, to);
    model.requestDaily("unavailable", "codex", from, to);
    model.requestProjects("unavailable");
    QTRY_COMPARE(failures.size(), 4);
    for (const auto &failure : failures)
      QCOMPARE(failure[1].toString(),
               QStringLiteral("session-database-unavailable"));
    QCOMPARE(sessions.size(), 0);
    QCOMPARE(summaries.size(), 0);
    QCOMPARE(daily.size(), 0);
    QCOMPARE(projects.size(), 0);
    QVERIFY(QDir().rmdir(fixture.database));
    QFile empty(fixture.database);
    QVERIFY(empty.open(QIODevice::WriteOnly));
    empty.close();
    failures.clear();
    model.requestSessions("sql", "codex", from, to);
    model.requestSummary("sql", "codex", from, to);
    model.requestDaily("sql", "codex", from, to);
    model.requestProjects("sql");
    QTRY_COMPARE(failures.size(), 4);
    for (const auto &failure : failures)
      QCOMPARE(failure[1].toString(), QStringLiteral("session-query-failed"));
    QVERIFY(QFile::remove(fixture.database));
    QVERIFY(QFile::rename(fixture.database + ".saved", fixture.database));
    model.requestSummary("retry", "codex", from, to);
    QTRY_COMPARE(summaries.size(), 1);
    QCOMPARE(summaries.first()[0].toString(), QStringLiteral("retry"));
    for (const auto &name : QSqlDatabase::connectionNames())
      QVERIFY(!name.startsWith("local_sessions_"));
  }

  void failedDatabaseOpensDoNotLeakConnections() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    LocalSessionModel model;
    const QString projectId(64, QLatin1Char('a'));
    QVERIFY(
        !model
             .createProjectAlias(QUrl::fromLocalFile(fixture.directory.path()),
                                 QStringLiteral("Private alias"))
             .isEmpty());
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    QSignalSpy clears(&model, &LocalSessionModel::clearFinished);
    QSignalSpy failures(&model, &LocalSessionModel::queryFailed);
    QSignalSpy exports(&model, &LocalSessionModel::exportReady);
    const auto from = QDateTime::currentDateTimeUtc().addDays(-1);
    const auto to = from.addDays(2);
    QVERIFY(QFile::rename(fixture.database, fixture.database + ".saved"));
    QVERIFY(QDir().mkdir(fixture.database));
    for (int attempt = 0; attempt < 4; ++attempt) {
      QVERIFY(model.projectAliases().isEmpty());
      QVERIFY(model.projectAlias(projectId).isEmpty());
      QVERIFY(!model.setProjectAlias(projectId, QStringLiteral("Alias")));
      QVERIFY(
          model
              .createProjectAlias(QUrl::fromLocalFile(fixture.directory.path()),
                                  QStringLiteral("Alias"))
              .isEmpty());
      if (attempt == 0)
        model.setToolEnabled(QStringLiteral("codex"), true);
      else
        model.requestImport();
      QTRY_COMPARE_WITH_TIMEOUT(imports.size(), 1, 5000);
      QCOMPARE(imports.takeFirst()[3].toString(),
               QStringLiteral("session-database-unavailable"));
      model.clearSessions(QStringLiteral("failed-clear"));
      QTRY_COMPARE_WITH_TIMEOUT(clears.size(), 1, 5000);
      const auto cleared = clears.takeFirst();
      QVERIFY(!cleared[1].toBool());
      QCOMPARE(cleared[2].toString(),
               QStringLiteral("session-database-unavailable"));
      model.requestSessions("failed-sessions", "codex", from, to);
      model.requestSummary("failed-summary", "codex", from, to);
      model.requestDaily("failed-daily", "codex", from, to);
      model.requestProjects("failed-projects");
      model.requestExport("failed-export", "codex", from, to, {}, "json",
                          fixture.directory.filePath("export.json"));
      QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 4, 5000);
      QTRY_COMPARE_WITH_TIMEOUT(exports.size(), 1, 5000);
      for (const auto &failure : failures)
        QCOMPARE(failure[1].toString(),
                 QStringLiteral("session-database-unavailable"));
      QCOMPARE(exports.takeFirst()[1].toString(),
               QStringLiteral("session-database-unavailable"));
      failures.clear();
      for (const auto &name : QSqlDatabase::connectionNames())
        QVERIFY2(!name.startsWith("local_sessions_"), qPrintable(name));
    }
    model.setToolEnabled(QStringLiteral("codex"), false);
    QVERIFY(QDir().rmdir(fixture.database));
    QVERIFY(QFile::rename(fixture.database + ".saved", fixture.database));
    QVERIFY(model.setProjectAlias(projectId, QStringLiteral("Recovered")));
    QCOMPARE(model.projectAlias(projectId), QStringLiteral("Recovered"));
  }

  void streamsOneHundredThousandExportEventsAtomically() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    const QString modelId = QString::fromUtf8("model,\"quoted\",å\nline");
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", "export-stream-seed");
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          "INSERT INTO "
          "local_sessions(session_id,tool_id,project_id,started_at_utc,"
          "last_seen_at_utc) VALUES('private-session','codex',"
          "'private-project','2026-10-10T12:00:00.000Z',"
          "'2026-10-10T12:00:00.000Z')"));
      QVERIFY(query.exec("INSERT INTO local_project_aliases(project_id,alias) "
                         "VALUES('private-project','Private alias')"));
      query.prepare(
          "WITH RECURSIVE seq(i) AS (SELECT 0 UNION ALL SELECT i+1 "
          "FROM seq WHERE i<99999) INSERT INTO local_usage_events("
          "event_id,session_id,observed_at_utc,model_id,input_tokens,"
          "output_tokens,cache_read_tokens,cache_write_tokens,parser_revision) "
          "SELECT 'event-'||i,'private-session','2026-10-10T12:00:00.000Z',"
          "?,i,CASE WHEN i%2=0 THEN NULL ELSE 0 END,7,0,1 FROM seq");
      query.addBindValue(modelId);
      QVERIFY(query.exec());
      // A small separate tool result exceeds the export file cap without
      // requiring SQLite to spill its ORDER BY sorting into a temporary file.
      QVERIFY(query.exec(
          "INSERT INTO local_sessions(session_id,tool_id,project_id,"
          "started_at_utc,last_seen_at_utc) VALUES('failure-session',"
          "'claude-code','private-project','2026-10-10T12:00:00.000Z',"
          "'2026-10-10T12:00:00.000Z')"));
      QVERIFY(query.exec(
          "WITH RECURSIVE seq(i) AS (SELECT 0 UNION ALL SELECT i+1 "
          "FROM seq WHERE i<99) INSERT INTO local_usage_events("
          "event_id,session_id,observed_at_utc,model_id,input_tokens,parser_"
          "revision) "
          "SELECT 'failure-'||i,'failure-session','2026-10-10T12:00:00.000Z',"
          "'test-model',i,1 FROM seq"));
    }
    QSqlDatabase::removeDatabase("export-stream-seed");
    LocalSessionModel model;
    QSignalSpy exports(&model, &LocalSessionModel::exportReady);
    const auto from =
        QDateTime::fromString("2026-10-10T00:00:00.000Z", Qt::ISODateWithMs);
    const auto to = from.addDays(1);
    QJsonArray jsonRows;
    QString jsonSessionId, jsonProjectId;
    const auto exportFile = [&](const QString &format) {
      return fixture.directory.filePath("events." + format);
    };
    for (const auto &format : {QStringLiteral("json"), QStringLiteral("csv")}) {
      const auto path = exportFile(format);
      model.requestExport("stream-" + format, "codex", from, to, {}, format,
                          path);
      QTRY_COMPARE_WITH_TIMEOUT(exports.size(), 1, 30000);
      QVERIFY2(exports.first()[1].toString().isEmpty(),
               qPrintable(exports.first()[1].toString()));
      exports.clear();
      QFile file(path);
      QVERIFY(file.open(QIODevice::ReadOnly));
      const auto bytes = file.readAll();
      QVERIFY(!bytes.contains("private-session"));
      QVERIFY(!bytes.contains("private-project"));
      QVERIFY(!bytes.contains("Private alias"));
      QVERIFY(!bytes.contains(fixture.directory.path().toUtf8()));
      QCOMPARE(file.permissions() &
                   (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                    QFileDevice::ExeGroup | QFileDevice::ReadOther |
                    QFileDevice::WriteOther | QFileDevice::ExeOther),
               QFileDevice::Permissions());
      if (format == QLatin1String("json")) {
        QJsonParseError error;
        const auto object = QJsonDocument::fromJson(bytes, &error).object();
        QCOMPARE(error.error, QJsonParseError::NoError);
        QCOMPARE(object.value("schemaVersion").toInt(), 1);
        jsonRows = object.value("sessions").toArray();
        QCOMPARE(jsonRows.size(), 100000);
        jsonSessionId =
            jsonRows.first().toObject().value("sessionId").toString();
        jsonProjectId =
            jsonRows.first().toObject().value("projectId").toString();
        QCOMPARE(jsonSessionId.size(), 64);
        QCOMPARE(jsonProjectId.size(), 64);
      } else {
        // Parse quoted fields including escaped quotes, commas, Unicode and
        // embedded newlines, comparing each record with the JSON export.
        QList<QByteArray> fields;
        QByteArray field;
        bool quoted = false;
        int record = -1;
        QByteArray csvSessionId, csvProjectId;
        for (qsizetype i = 0; i < bytes.size(); ++i) {
          const char ch = bytes[i];
          if (ch == '"') {
            if (quoted && i + 1 < bytes.size() && bytes[i + 1] == '"') {
              field += '"';
              ++i;
            } else {
              quoted = !quoted;
            }
          } else if (!quoted && (ch == ',' || ch == '\n')) {
            fields.append(field);
            field.clear();
            if (ch != '\n')
              continue;
            QCOMPARE(fields.size(), 8);
            if (record >= 0) {
              const auto row = jsonRows[record].toObject();
              if (record == 0) {
                csvSessionId = fields[0];
                csvProjectId = fields[1];
                QCOMPARE(csvSessionId.size(), 64);
                QCOMPARE(csvProjectId.size(), 64);
                QVERIFY(csvSessionId != jsonSessionId.toUtf8());
                QVERIFY(csvProjectId != jsonProjectId.toUtf8());
              }
              QCOMPARE(fields[0], csvSessionId);
              QCOMPARE(fields[1], csvProjectId);
              QCOMPARE(fields[2],
                       row.value("timestampUtc").toString().toUtf8());
              QCOMPARE(QString::fromUtf8(fields[3]), modelId);
              QCOMPARE(fields[4].toLongLong(),
                       row.value("inputTokens").toInteger());
              QCOMPARE(fields[4].toLongLong(), record);
              if (record % 2 == 0) {
                QVERIFY(row.value("outputTokens").isNull());
                QVERIFY(fields[5].isEmpty());
              } else {
                QCOMPARE(row.value("outputTokens").toInt(), 0);
                QCOMPARE(fields[5], QByteArray("0"));
              }
              QCOMPARE(fields[6], QByteArray("7"));
              QCOMPARE(fields[7], QByteArray("0"));
              QCOMPARE(row.value("cacheReadTokens").toInt(), 7);
              QCOMPARE(row.value("cacheWriteTokens").toInt(), 0);
              QCOMPARE(row.value("sessionId").toString(), jsonSessionId);
              QCOMPARE(row.value("projectId").toString(), jsonProjectId);
              QCOMPARE(row.value("modelId").toString(), modelId);
            }
            ++record;
            fields.clear();
          } else {
            field += ch;
          }
        }
        QVERIFY(!quoted);
        QVERIFY(fields.isEmpty());
        QVERIFY(field.isEmpty());
        QCOMPARE(record, 100000);
      }
    }

    const auto target = fixture.directory.filePath("preserved.json");
    QFile existing(target);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write("original export"), qint64(15));
    existing.close();
    const auto originalPermissions = existing.permissions();
    {
      // Limit the temporary export's size to force a real mid-stream write
      // failure, rather than merely rejecting an invalid destination.
      struct FileSizeLimit {
        rlimit original{};
        struct sigaction originalSignal{};
        bool limited = false;
        ~FileSizeLimit() {
          if (limited) {
            setrlimit(RLIMIT_FSIZE, &original);
            sigaction(SIGXFSZ, &originalSignal, nullptr);
          }
        }
      } limit;
      QCOMPARE(getrlimit(RLIMIT_FSIZE, &limit.original), 0);
      struct sigaction ignored{};
      ignored.sa_handler = SIG_IGN;
      sigemptyset(&ignored.sa_mask);
      QCOMPARE(sigaction(SIGXFSZ, &ignored, &limit.originalSignal), 0);
      limit.limited = true;
      rlimit restricted = limit.original;
      restricted.rlim_cur = qMin<rlim_t>(4096, restricted.rlim_max);
      QCOMPARE(setrlimit(RLIMIT_FSIZE, &restricted), 0);
      model.requestExport("write-failure", "claude-code", from, to, {}, "json",
                          target);
      QTRY_COMPARE_WITH_TIMEOUT(exports.size(), 1, 30000);
      QCOMPARE(exports.first()[1].toString(),
               QStringLiteral("session-export-write-failed"));
    }
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("original export"));
    QCOMPARE(existing.permissions(), originalPermissions);
    const auto leftoverFiles =
        QDir(fixture.directory.path()).entryList(QDir::Files);
    QCOMPARE(leftoverFiles,
             QStringList({"events.csv", "events.json", "preserved.json"}));
  }

  void sessionModelBreakdownIsBoundedAndCounted() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", "session-model-seed");
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery q(db);
      QVERIFY(db.transaction());
      QVERIFY(q.exec("INSERT INTO "
                     "local_sessions(session_id,tool_id,project_id,started_at_"
                     "utc,last_seen_at_utc) "
                     "VALUES('mixed','codex','p','2026-10-10T12:00:00.000Z','"
                     "2026-10-10T12:00:00.000Z')"));
      for (int i = 0; i < 105; ++i) {
        const auto id = QStringLiteral("%1").arg(i, 3, 10, QChar('0'));
        q.prepare("INSERT INTO "
                  "local_usage_events(event_id,session_id,observed_at_utc,"
                  "model_id,input_tokens,parser_revision) "
                  "VALUES(?,'mixed','2026-10-10T12:00:00.000Z',?,1,1)");
        q.addBindValue(id);
        q.addBindValue(id);
        QVERIFY(q.exec());
      }
      QVERIFY(db.commit());
    }
    QSqlDatabase::removeDatabase("session-model-seed");
    LocalSessionModel model;
    QSignalSpy sessions(&model, &LocalSessionModel::sessionsReady);
    const auto from =
        QDateTime::fromString("2026-10-10T00:00:00.000Z", Qt::ISODateWithMs);
    model.requestSessions("bounded", "codex", from, from.addDays(1));
    QTRY_COMPARE(sessions.size(), 1);
    const auto row = sessions.first()[1].toList().first().toMap();
    QCOMPARE(row.value("eventCount").toInt(), 105);
    QCOMPARE(row.value("inputTokens").toInt(), 105);
    QCOMPARE(row.value("modelBreakdownTotalCount").toInt(), 105);
    const auto models = row.value("modelBreakdown").toList();
    QCOMPARE(models.size(), 100);
    QCOMPARE(models.first().toMap().value("modelId").toString(),
             QStringLiteral("000"));
    QCOMPARE(models.last().toMap().value("modelId").toString(),
             QStringLiteral("099"));
  }

  void paginationUsesMatchingSessionCounts_data() {
    QTest::addColumn<int>("count");
    for (int count : {0, 99, 100, 101, 200})
      QTest::newRow(qPrintable(QString::number(count))) << count;
  }
  void paginationUsesMatchingSessionCounts() {
    QFETCH(int, count);
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", "page-seed");
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery q(db);
      QVERIFY(db.transaction());
      for (int i = 0; i < count; ++i) {
        const auto id = QStringLiteral("%1").arg(i, 3, 10, QChar('0'));
        q.prepare("INSERT INTO "
                  "local_sessions(session_id,tool_id,project_id,started_at_utc,"
                  "last_seen_at_utc) "
                  "VALUES(?,'codex','p','2026-10-10T12:00:00.000Z','2026-10-"
                  "10T12:00:00.000Z')");
        q.addBindValue(id);
        QVERIFY(q.exec());
        q.prepare("INSERT INTO "
                  "local_usage_events(event_id,session_id,observed_at_utc,"
                  "model_id,input_tokens,parser_revision) "
                  "VALUES(?,?,'2026-10-10T12:00:00.000Z','m',1,1)");
        q.addBindValue(id);
        q.addBindValue(id);
        QVERIFY(q.exec());
      }
      QVERIFY(db.commit());
    }
    QSqlDatabase::removeDatabase("page-seed");
    LocalSessionModel model;
    QSignalSpy summary(&model, &LocalSessionModel::summaryReady);
    QSignalSpy sessions(&model, &LocalSessionModel::sessionsReady);
    const auto from =
        QDateTime::fromString("2026-10-10T00:00:00.000Z", Qt::ISODateWithMs);
    const QVariantMap filter{{"modelId", QStringLiteral("m")}};
    model.requestSummary("count", "codex", from, from.addDays(1), {}, filter);
    model.requestSessions("first", "codex", from, from.addDays(1), {}, 0, 100,
                          filter);
    model.requestSessions("second", "codex", from, from.addDays(1), {}, 100,
                          100, filter);
    QTRY_COMPARE(summary.size(), 1);
    QTRY_COMPARE(sessions.size(), 2);
    QCOMPARE(summary.first()[1].toMap().value("sessionCount").toInt(), count);
    for (const auto &reply : sessions)
      QCOMPARE(reply[1].toList().size(), reply[0].toString() == "first"
                                             ? qMin(count, 100)
                                             : qMax(0, count - 100));
  }

  void breakdownsAreBoundedCountedAndStable() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    {
      auto db = QSqlDatabase::addDatabase("QSQLITE", "bounded-seed");
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery q(db);
      QVERIFY(db.transaction());
      for (int i = 0; i < 105; ++i) {
        const auto id = QStringLiteral("%1").arg(i, 3, 10, QChar('0'));
        q.prepare("INSERT INTO "
                  "local_sessions(session_id,tool_id,project_id,started_at_utc,"
                  "last_seen_at_utc) "
                  "VALUES(?,'codex',?,'2026-10-10T12:00:00.000Z','2026-10-"
                  "10T12:00:00.000Z')");
        q.addBindValue(id);
        q.addBindValue(id);
        QVERIFY(q.exec());
        q.prepare("INSERT INTO "
                  "local_usage_events(event_id,session_id,observed_at_utc,"
                  "model_id,input_tokens,parser_revision) "
                  "VALUES(?,?,'2026-10-10T12:00:00.000Z',?,1,1)");
        q.addBindValue(id);
        q.addBindValue(id);
        q.addBindValue(id);
        QVERIFY(q.exec());
      }
      QVERIFY(db.commit());
    }
    QSqlDatabase::removeDatabase("bounded-seed");
    LocalSessionModel model;
    QSignalSpy summary(&model, &LocalSessionModel::summaryReady);
    const auto from =
        QDateTime::fromString("2026-10-10T00:00:00.000Z", Qt::ISODateWithMs);
    model.requestSummary("bounded", "codex", from, from.addDays(1));
    QTRY_COMPARE(summary.size(), 1);
    const auto result = summary.first()[1].toMap();
    QCOMPARE(result.value("eventCount").toInt(), 105);
    QCOMPARE(result.value("projectOptions").toList().size(), 105);
    QCOMPARE(result.value("modelOptions").toList().size(), 105);
    for (const auto &kind :
         {QStringLiteral("model"), QStringLiteral("project")}) {
      const auto rows = result.value(kind + "Breakdown").toList();
      QCOMPARE(rows.size(), 100);
      QCOMPARE(result.value(kind + "BreakdownTotalCount").toInt(), 105);
      QCOMPARE(rows.first().toMap().value(kind + "Id").toString(),
               QStringLiteral("000"));
      QCOMPARE(rows.last().toMap().value(kind + "Id").toString(),
               QStringLiteral("099"));
    }
  }

  void userCreatedAliasesMatchImportedProjectsAndKeepSessions() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    const QString folder =
        fixture.directory.filePath(QStringLiteral("private-chosen-project"));
    const QString secondFolder =
        fixture.directory.filePath(QStringLiteral("untracked-project"));
    QVERIFY(QDir().mkpath(folder));
    QVERIFY(QDir().mkpath(secondFolder));
    const QByteArray metadata =
        QJsonDocument(
            QJsonObject{{"type", "session_meta"},
                        {"payload", QJsonObject{{"id", "chosen-session"},
                                                {"cwd", folder}}}})
            .toJson(QJsonDocument::Compact);
    QVERIFY(fixture.write(metadata + '\n' + fixture.codexRecord(100)));
    LocalSessionModel model;
    QCOMPARE(model.projectAliases().size(), 0);
    const QString projectId = model.createProjectAlias(
        QUrl::fromLocalFile(folder + QStringLiteral("/./")),
        QStringLiteral("  My project  "));
    QCOMPARE(projectId.size(), 64);
    QCOMPARE(model.projectAlias(projectId), QStringLiteral("My project"));
    QCOMPARE(model.createProjectAlias(QUrl::fromLocalFile(folder),
                                      QStringLiteral("Renamed project")),
             projectId);
    const QString secondId = model.createProjectAlias(
        QUrl::fromLocalFile(secondFolder), QStringLiteral("Another project"));
    QCOMPARE(secondId.size(), 64);
    auto aliases = model.projectAliases();
    QCOMPARE(aliases.size(), 2);
    QCOMPARE(aliases.first().toMap().value("alias").toString(),
             QStringLiteral("Another project"));
    QCOMPARE(aliases.first().toMap().value("sessionCount").toInt(), 0);
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    QSignalSpy projects(&model, &LocalSessionModel::projectsReady);
    model.setToolEnabled(QStringLiteral("codex"), true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.count(), 1, 5000);
    QVERIFY(imports.first().at(3).toString().isEmpty());
    model.requestProjects(QStringLiteral("aliases-imported"));
    QTRY_COMPARE_WITH_TIMEOUT(projects.count(), 1, 5000);
    QCOMPARE(projects.first()
                 .at(1)
                 .toList()
                 .first()
                 .toMap()
                 .value("projectId")
                 .toString(),
             projectId);
    aliases = model.projectAliases();
    QCOMPARE(aliases.last().toMap().value("sessionCount").toInt(), 1);
    QVERIFY(model.setProjectAlias(projectId, QStringLiteral("  ")));
    QVERIFY(model.projectAlias(projectId).isEmpty());
    QCOMPARE(model.projectAliases().size(), 1);
    projects.clear();
    model.requestProjects(QStringLiteral("aliases-removed"));
    QTRY_COMPARE_WITH_TIMEOUT(projects.count(), 1, 5000);
    QCOMPARE(projects.first().at(1).toList().size(), 1);
    QVERIFY(projects.first()
                .at(1)
                .toList()
                .first()
                .toMap()
                .value("alias")
                .toString()
                .isEmpty());
    // The selected directory is an input to identity hashing, never stored
    // data.
    QFile database(fixture.database);
    QVERIFY(database.open(QIODevice::ReadOnly));
    QVERIFY(!database.readAll().contains(folder.toUtf8()));
    QFile wal(fixture.database + QStringLiteral("-wal"));
    if (wal.open(QIODevice::ReadOnly))
      QVERIFY(!wal.readAll().contains(folder.toUtf8()));
    QSignalSpy exported(&model, &LocalSessionModel::exportReady);
    const QString destination =
        fixture.directory.filePath(QStringLiteral("aliases-export.json"));
    const auto now = QDateTime::currentDateTimeUtc();
    model.requestExport(QStringLiteral("alias-privacy"),
                        QStringLiteral("codex"), now.addDays(-1),
                        now.addDays(1), {}, QStringLiteral("json"),
                        destination);
    QTRY_COMPARE_WITH_TIMEOUT(exported.count(), 1, 5000);
    QVERIFY(exported.first().at(1).toString().isEmpty());
    QFile output(destination);
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QByteArray bytes = output.readAll();
    QVERIFY(!bytes.contains(folder.toUtf8()));
    QVERIFY(!bytes.contains("Another project"));
  }

  void aliasAttachesToOriginalImportedPathSpelling_data() {
    QTest::addColumn<QString>("suffix");
    QTest::newRow("trailing-slash") << QStringLiteral("/");
    QTest::newRow("dot-suffix") << QStringLiteral("/.");
    QTest::newRow("dot-slash-suffix") << QStringLiteral("/./");
  }

  void aliasAttachesToOriginalImportedPathSpelling() {
    QFETCH(QString, suffix);
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    const QString folder =
        fixture.directory.filePath(QStringLiteral("chosen-project"));
    QVERIFY(QDir().mkpath(folder));
    const QByteArray metadata =
        QJsonDocument(
            QJsonObject{{"type", "session_meta"},
                        {"payload", QJsonObject{{"id", "spelled-session"},
                                                {"cwd", folder + suffix}}}})
            .toJson(QJsonDocument::Compact);
    QVERIFY(fixture.write(metadata + '\n' + fixture.codexRecord(100)));
    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    QSignalSpy projects(&model, &LocalSessionModel::projectsReady);
    model.setToolEnabled(QStringLiteral("codex"), true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.count(), 1, 5000);
    QVERIFY(imports.first().at(3).toString().isEmpty());
    model.requestProjects(QStringLiteral("path-spelling"));
    QTRY_COMPARE_WITH_TIMEOUT(projects.count(), 1, 5000);
    const QString importedId = projects.first()
                                   .at(1)
                                   .toList()
                                   .first()
                                   .toMap()
                                   .value("projectId")
                                   .toString();
    QCOMPARE(model.createProjectAlias(QUrl::fromLocalFile(folder),
                                      QStringLiteral("Selected project")),
             importedId);
    QCOMPARE(model.projectAliases().size(), 1);
    QCOMPARE(
        model.projectAliases().first().toMap().value("sessionCount").toInt(),
        1);
  }

  void aliasCreationRejectsInvalidInput() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    LocalSessionModel model;
    const QUrl directory = QUrl::fromLocalFile(fixture.directory.path());
    QVERIFY(model
                .createProjectAlias(
                    QUrl(QStringLiteral("https://example.com/project")),
                    QStringLiteral("Project"))
                .isEmpty());
    QVERIFY(
        model
            .createProjectAlias(QUrl::fromLocalFile(fixture.directory.filePath(
                                    QStringLiteral("missing"))),
                                QStringLiteral("Project"))
            .isEmpty());
    QVERIFY(model.createProjectAlias(directory, QStringLiteral(" ")).isEmpty());
    QVERIFY(model.createProjectAlias(directory, QString(81, QLatin1Char('a')))
                .isEmpty());
    QVERIFY(
        model.createProjectAlias(directory, QStringLiteral("a") + QChar::Null)
            .isEmpty());
    QVERIFY(fixture.write(QByteArray("test")));
    QVERIFY(model
                .createProjectAlias(
                    QUrl::fromLocalFile(fixture.root +
                                        QStringLiteral("/session.jsonl")),
                    QStringLiteral("File"))
                .isEmpty());
    QCOMPARE(model.projectAliases().size(), 0);
    // Old empty rows do not become artificial project entries.
    const QString connection = QStringLiteral("empty_alias_seed");
    {
      auto db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      db.setDatabaseName(fixture.database);
      QVERIFY(db.open());
      QSqlQuery query(db);
      QVERIFY(query.exec(
          QStringLiteral("INSERT INTO local_project_aliases(project_id,alias) "
                         "VALUES('%1','')")
              .arg(QString(64, QLatin1Char('a')))));
    }
    QSqlDatabase::removeDatabase(connection);
    QCOMPARE(model.projectAliases().size(), 0);
  }

  void failedWritesRollbackCountersAndCheckpoint_data() {
    QTest::addColumn<QString>("table");
    QTest::newRow("event") << QStringLiteral("local_usage_events");
    QTest::newRow("counter") << QStringLiteral("local_session_counters");
    QTest::newRow("checkpoint") << QStringLiteral("local_file_checkpoints");
  }

  void failedWritesRollbackCountersAndCheckpoint() {
    QFETCH(QString, table);
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    QVERIFY(fixture.write(
        QByteArray(
            R"({"type":"session_meta","payload":{"id":"thread","cwd":"/private/project"}})") +
        '\n' +
        QByteArray(
            R"({"type":"turn_context","payload":{"model":"gpt-5-codex"}})") +
        '\n' + fixture.codexRecord(100)));
    const QString connection = QStringLiteral("session_write_failure");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(fixture.database);
    QVERIFY(db.open());
    {
      QSqlQuery query(db);
      QVERIFY(query.exec(
          QStringLiteral("CREATE TRIGGER fail_write BEFORE INSERT ON %1 BEGIN "
                         "SELECT RAISE(ABORT,'synthetic failure'); END")
              .arg(table)));
      LocalSessionModel model;
      QSignalSpy imports(&model, &LocalSessionModel::importFinished);
      model.setToolEnabled(QStringLiteral("codex"), true);
      QTRY_COMPARE_WITH_TIMEOUT(imports.count(), 1, 5000);
      QCOMPARE(imports.takeFirst().at(3).toString(),
               QStringLiteral("session-database-write-failed"));
      QCOMPARE(model.status(), QStringLiteral("error"));
      for (const QString &target : {QStringLiteral("local_usage_events"),
                                    QStringLiteral("local_session_counters"),
                                    QStringLiteral("local_file_checkpoints")}) {
        QVERIFY(
            query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(target)));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        query.finish();
      }
      QVERIFY(query.exec(QStringLiteral("DROP TRIGGER fail_write")));
      model.requestImport();
      QTRY_COMPARE_WITH_TIMEOUT(imports.count(), 1, 5000);
      QVERIFY(imports.takeFirst().at(3).toString().isEmpty());
      QCOMPARE(model.status(), QStringLiteral("ready"));
      QVERIFY(query.exec(
          QStringLiteral("SELECT COUNT(*),SUM(input_tokens),MIN(model_id) FROM "
                         "local_usage_events")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 1);
      QCOMPARE(query.value(1).toInt(), 100);
      QCOMPARE(query.value(2).toString(), QStringLiteral("gpt-5-codex"));
      query.finish();
      // Incremental import must reconstruct turn context before its checkpoint.
      QVERIFY(fixture.write(fixture.codexRecord(120), QIODevice::Append));
      model.requestImport();
      QTRY_COMPARE_WITH_TIMEOUT(imports.count(), 1, 5000);
      imports.clear();
      QVERIFY(query.exec(
          QStringLiteral("SELECT COUNT(*),SUM(input_tokens),MIN(model_id) FROM "
                         "local_usage_events")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 2);
      QCOMPARE(query.value(1).toInt(), 120);
      QCOMPARE(query.value(2).toString(), QStringLiteral("gpt-5-codex"));
    }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
  }

  void periodicImportObservesNewSessionsAndStopsWhenDisabled() {
    SessionFixture fixture;
    QVERIFY(fixture.valid);
    LocalSessionModel model;
    auto *timer =
        model.findChild<QTimer *>(QStringLiteral("local-session-import-timer"));
    QVERIFY(timer);
    QCOMPARE(timer->interval(), 60000);
    QVERIFY(!timer->isActive());
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex"), true);
    QTRY_COMPARE_WITH_TIMEOUT(imports.count(), 1, 5000);
    imports.clear();
    QVERIFY(timer->isActive());
    QVERIFY(fixture.write(
        QByteArray(
            R"({"type":"session_meta","payload":{"id":"thread","cwd":"/private/project"}})") +
        '\n' + fixture.codexRecord(100)));
    timer->setInterval(20);
    QTRY_VERIFY_WITH_TIMEOUT(!imports.isEmpty(), 5000);
    model.setToolEnabled(QStringLiteral("codex"), false);
    QVERIFY(!timer->isActive());
    QTRY_COMPARE_WITH_TIMEOUT(model.pendingWorkerCount(), 0, 5000);
    const int completed = imports.count();
    QTest::qWait(80);
    QCOMPARE(imports.count(), completed);
    const QString connection = QStringLiteral("session_periodic_verify");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(fixture.database);
    QVERIFY(db.open());
    {
      QSqlQuery query(db);
      QVERIFY(query.exec(QStringLiteral(
          "SELECT COUNT(*),SUM(input_tokens),MIN(model_id),MIN(model_id IS "
          "NULL) FROM local_usage_events")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 1);
      QCOMPARE(query.value(1).toInt(), 100);
      QCOMPARE(query.value(2).toString(), QStringLiteral(""));
      QCOMPARE(query.value(3).toInt(), 0);
    }
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
  }

  void codexBackfillUsesOnlyThirtyDaysAndPriorCounterBaseline() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataHome = dir.filePath(QStringLiteral("data"));
    const QString codexHome = dir.filePath(QStringLiteral("codex"));
    const QString sessionsDir = codexHome + QStringLiteral("/sessions");
    QVERIFY(QDir().mkpath(sessionsDir));
    qputenv("XDG_DATA_HOME", QFile::encodeName(dataHome));
    qputenv("HOME", QFile::encodeName(dir.path()));
    qputenv("CODEX_HOME", QFile::encodeName(codexHome));
    const QString historyDirectory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/plasma-ai-usage-monitor");
    QVERIFY(QDir().mkpath(historyDirectory));
    const QString seedConnection = QStringLiteral("codex_backfill_seed");
    auto seed =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), seedConnection);
    seed.setDatabaseName(historyDirectory +
                         QStringLiteral("/usage_history.db"));
    QVERIFY(seed.open());
    QSqlQuery seedQuery(seed);
    QVERIFY(seedQuery.exec(QStringLiteral("PRAGMA user_version=8")));
    QString schemaError;
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(seed, &schemaError),
             qPrintable(schemaError));
    seed.close();
    seed = {};
    QSqlDatabase::removeDatabase(seedConnection);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto row = [&now](int input, int secondsAgo) {
      const QJsonObject totals{{QStringLiteral("input_tokens"), input},
                               {QStringLiteral("output_tokens"), 10}};
      const QJsonObject info{
          {QStringLiteral("model"), QStringLiteral("gpt-5-codex")},
          {QStringLiteral("total_token_usage"), totals}};
      const QJsonObject payload{
          {QStringLiteral("type"), QStringLiteral("token_count")},
          {QStringLiteral("info"), info}};
      const QJsonObject record{
          {QStringLiteral("type"), QStringLiteral("event_msg")},
          {QStringLiteral("thread_id"), QStringLiteral("backfill-thread")},
          {QStringLiteral("cwd"), QStringLiteral("/private/backfill")},
          {QStringLiteral("timestamp"),
           now.addSecs(-secondsAgo).toUTC().toString(Qt::ISODateWithMs)},
          {QStringLiteral("payload"), payload}};
      return QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
    };
    QFile file(sessionsDir + QStringLiteral("/backfill.jsonl"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray oldSnapshot = row(1000, 40 * 86400);
    const QByteArray recentSnapshot = row(1020, 60);
    QCOMPARE(file.write(oldSnapshot + recentSnapshot),
             qint64(oldSnapshot.size() + recentSnapshot.size()));
    file.close();

    LocalSessionModel model;
    model.setRetentionDays(90);
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("codex"), true);
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    QCOMPARE(imports.first().at(0).toString(), QStringLiteral("codex-cli"));
    const QString connection = QStringLiteral("codex_backfill_verify");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(historyDirectory + QStringLiteral("/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral(
        "SELECT COUNT(*),SUM(input_tokens) FROM local_usage_events")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
    QCOMPARE(q.value(1).toLongLong(), 20);
    q.finish();
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
    qunsetenv("CODEX_HOME");
    qunsetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
  }

  void conflictingClaudeMessageIdentityDoesNotCountEitherValue() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataHome = dir.filePath(QStringLiteral("data"));
    const QString claudeHome = dir.filePath(QStringLiteral("claude"));
    const QString projects = claudeHome + QStringLiteral("/projects/synthetic");
    QVERIFY(QDir().mkpath(projects));
    qputenv("XDG_DATA_HOME", QFile::encodeName(dataHome));
    qputenv("HOME", QFile::encodeName(dir.path()));
    qputenv("CLAUDE_CONFIG_DIR", QFile::encodeName(claudeHome));
    const QString historyDirectory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/plasma-ai-usage-monitor");
    QVERIFY(QDir().mkpath(historyDirectory));
    const QString seedConnection = QStringLiteral("claude_conflict_seed");
    auto seed =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), seedConnection);
    seed.setDatabaseName(historyDirectory +
                         QStringLiteral("/usage_history.db"));
    QVERIFY(seed.open());
    QSqlQuery seedQuery(seed);
    QVERIFY(seedQuery.exec(QStringLiteral("PRAGMA user_version=8")));
    QString schemaError;
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(seed, &schemaError),
             qPrintable(schemaError));
    seed.close();
    seed = {};
    QSqlDatabase::removeDatabase(seedConnection);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto assistant = [&now](int input, int output, int seconds) {
      const QJsonObject usage{
          {QStringLiteral("input_tokens"), input},
          {QStringLiteral("output_tokens"), output},
          {QStringLiteral("cache_read_input_tokens"), 2},
          {QStringLiteral("cache_creation_input_tokens"), 1}};
      const QJsonObject message{
          {QStringLiteral("id"), QStringLiteral("same-message")},
          {QStringLiteral("model"), QStringLiteral("claude-sonnet")},
          {QStringLiteral("usage"), usage}};
      const QJsonObject record{
          {QStringLiteral("type"), QStringLiteral("assistant")},
          {QStringLiteral("sessionId"), QStringLiteral("private-session")},
          {QStringLiteral("cwd"), QStringLiteral("/private/project")},
          {QStringLiteral("timestamp"),
           now.addSecs(seconds).toUTC().toString(Qt::ISODateWithMs)},
          {QStringLiteral("message"), message}};
      return QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
    };
    QFile log(projects + QStringLiteral("/session.jsonl"));
    QVERIFY(log.open(QIODevice::WriteOnly));
    const QByteArray first = assistant(10, 3, 0);
    const QByteArray conflict = assistant(99, 3, 1);
    QCOMPARE(log.write(first + conflict),
             qint64(first.size() + conflict.size()));
    log.close();

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("claude-code"), true);
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    QCOMPARE(imports.first().at(0).toString(), QStringLiteral("claude-code"));
    QVERIFY(log.open(QIODevice::Append));
    QCOMPARE(log.write(first), first.size());
    log.close();
    LocalSessionModel restarted;
    QSignalSpy replay(&restarted, &LocalSessionModel::importFinished);
    restarted.setToolEnabled(QStringLiteral("claude-code"), true);
    QTRY_COMPARE_WITH_TIMEOUT(replay.count(), 1, 5000);
    const QString connection = QStringLiteral("claude_conflict_verify");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(historyDirectory + QStringLiteral("/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM local_usage_events")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
    QVERIFY(
        q.exec(QStringLiteral("SELECT COUNT(*) FROM local_usage_conflicts")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
    QVERIFY(q.exec(QStringLiteral("SELECT coverage FROM local_sessions")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("partial-conflict"));
    q.finish();
    QSignalSpy cleared(&restarted, &LocalSessionModel::clearFinished);
    restarted.clearSessions(QStringLiteral("conflict-clear"));
    QTRY_COMPARE_WITH_TIMEOUT(cleared.count(), 1, 5000);
    QVERIFY(cleared.takeFirst().at(1).toBool());
    QVERIFY(
        q.exec(QStringLiteral("SELECT COUNT(*) FROM local_usage_conflicts")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
    q.finish();
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
    qunsetenv("CLAUDE_CONFIG_DIR");
    qunsetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
  }

  void laterClaudeUsageCompletesTheSameMessageObservation() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataHome = dir.filePath(QStringLiteral("data"));
    const QString claudeHome = dir.filePath(QStringLiteral("claude"));
    const QString projects = claudeHome + QStringLiteral("/projects/synthetic");
    QVERIFY(QDir().mkpath(projects));
    qputenv("XDG_DATA_HOME", QFile::encodeName(dataHome));
    qputenv("HOME", QFile::encodeName(dir.path()));
    qputenv("CLAUDE_CONFIG_DIR", QFile::encodeName(claudeHome));
    const QString historyDirectory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/plasma-ai-usage-monitor");
    QVERIFY(QDir().mkpath(historyDirectory));
    const QString seedConnection = QStringLiteral("claude_completion_seed");
    auto seed =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), seedConnection);
    seed.setDatabaseName(historyDirectory +
                         QStringLiteral("/usage_history.db"));
    QVERIFY(seed.open());
    QSqlQuery seedQuery(seed);
    QVERIFY(seedQuery.exec(QStringLiteral("PRAGMA user_version=8")));
    QString schemaError;
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(seed, &schemaError),
             qPrintable(schemaError));
    seed.close();
    seed = {};
    QSqlDatabase::removeDatabase(seedConnection);

    auto assistant = [](bool complete, const QDateTime &timestamp) {
      QJsonObject usage{{QStringLiteral("output_tokens"), 10}};
      if (complete) {
        usage.insert(QStringLiteral("input_tokens"), 100);
        usage.insert(QStringLiteral("cache_read_input_tokens"), 20);
        usage.insert(QStringLiteral("cache_creation_input_tokens"), 5);
      }
      const QJsonObject message{
          {QStringLiteral("id"), QStringLiteral("message-completes")},
          {QStringLiteral("model"), QStringLiteral("claude-sonnet")},
          {QStringLiteral("usage"), usage}};
      const QJsonObject record{
          {QStringLiteral("type"), QStringLiteral("assistant")},
          {QStringLiteral("sessionId"), QStringLiteral("private-session")},
          {QStringLiteral("cwd"), QStringLiteral("/private/project")},
          {QStringLiteral("timestamp"),
           timestamp.toUTC().toString(Qt::ISODateWithMs)},
          {QStringLiteral("message"), message}};
      return QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
    };
    const QString logPath = projects + QStringLiteral("/session.jsonl");
    QFile log(logPath);
    QVERIFY(log.open(QIODevice::WriteOnly));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QByteArray partial = assistant(false, now);
    QCOMPARE(log.write(partial), partial.size());
    log.close();

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    model.setToolEnabled(QStringLiteral("claude-code"), true);
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    QCOMPARE(imports.takeFirst().at(0).toString(),
             QStringLiteral("claude-code"));
    QVERIFY(log.open(QIODevice::Append));
    const QByteArray completed = assistant(true, now.addSecs(1));
    QCOMPARE(log.write(completed), completed.size());
    log.close();
    model.requestImport();
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    QCOMPARE(imports.takeFirst().at(0).toString(),
             QStringLiteral("claude-code"));

    const QString connection = QStringLiteral("claude_completion_verify");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(historyDirectory + QStringLiteral("/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(
        QStringLiteral("SELECT COUNT(*),SUM(input_tokens),SUM(output_tokens),"
                       "SUM(cache_read_tokens),SUM(cache_write_tokens) "
                       "FROM local_usage_events")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
    QCOMPARE(q.value(1).toLongLong(), 125);
    QCOMPARE(q.value(2).toLongLong(), 10);
    QCOMPARE(q.value(3).toLongLong(), 20);
    QCOMPARE(q.value(4).toLongLong(), 5);
    q.finish();
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
    qunsetenv("CLAUDE_CONFIG_DIR");
    qunsetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
  }

  void clearRebaselinesCumulativeCodexWithoutRestoringOldUsage() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataHome = dir.filePath(QStringLiteral("data"));
    const QString codexHome = dir.filePath(QStringLiteral("codex"));
    const QString sessionsDir = codexHome + QStringLiteral("/sessions");
    QVERIFY(QDir().mkpath(sessionsDir));
    qputenv("XDG_DATA_HOME", QFile::encodeName(dataHome));
    qputenv("HOME", QFile::encodeName(dir.path()));
    qputenv("CODEX_HOME", QFile::encodeName(codexHome));
    const QString historyDirectory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/plasma-ai-usage-monitor");
    QVERIFY(QDir().mkpath(historyDirectory));
    const QString seedConnection = QStringLiteral("clear_rebaseline_seed");
    auto seed =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), seedConnection);
    seed.setDatabaseName(historyDirectory +
                         QStringLiteral("/usage_history.db"));
    QVERIFY(seed.open());
    QSqlQuery seedQuery(seed);
    QVERIFY(seedQuery.exec(QStringLiteral("PRAGMA user_version=8")));
    QString schemaError;
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(seed, &schemaError),
             qPrintable(schemaError));
    seed.close();
    seed = {};
    QSqlDatabase::removeDatabase(seedConnection);
    const QString sessionFile = sessionsDir + QStringLiteral("/clear.jsonl");
    auto tokenRow = [](qint64 input, const QDateTime &time,
                       const QString &thread = QStringLiteral("clear-thread")) {
      QJsonObject usage{{QStringLiteral("input_tokens"), input},
                        {QStringLiteral("output_tokens"), 20}};
      QJsonObject info{{QStringLiteral("model"), QStringLiteral("gpt-5-codex")},
                       {QStringLiteral("total_token_usage"), usage}};
      QJsonObject payload{
          {QStringLiteral("type"), QStringLiteral("token_count")},
          {QStringLiteral("info"), info}};
      QJsonObject record{
          {QStringLiteral("type"), QStringLiteral("event_msg")},
          {QStringLiteral("thread_id"), thread},
          {QStringLiteral("cwd"), QStringLiteral("/private/project")},
          {QStringLiteral("timestamp"),
           time.toUTC().toString(Qt::ISODateWithMs)},
          {QStringLiteral("payload"), payload}};
      return QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
    };
    QFile file(sessionFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QDateTime beforeClear = QDateTime::currentDateTimeUtc().addSecs(-60);
    const QByteArray metadata =
        R"({"type":"session_meta","payload":{"id":"clear-thread","cwd":"/private/project"}})";
    const QByteArray first = tokenRow(100, beforeClear);
    QCOMPARE(file.write(metadata + '\n'), qint64(metadata.size() + 1));
    QCOMPARE(file.write(first), first.size());
    file.close();

    LocalSessionModel model;
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    QSignalSpy clears(&model, &LocalSessionModel::clearFinished);
    model.setToolEnabled(QStringLiteral("codex"), true);
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    QCOMPARE(imports.takeFirst().at(0).toString(), QStringLiteral("codex-cli"));
    model.clearSessions(QStringLiteral("clear-1"));
    if (clears.isEmpty())
      QVERIFY(clears.wait(5000));
    QVERIFY(clears.takeFirst().at(1).toBool());

    QVERIFY(file.open(QIODevice::Append));
    const QByteArray afterClear =
        tokenRow(1000, QDateTime::currentDateTimeUtc().addSecs(5));
    QCOMPARE(file.write(afterClear), afterClear.size());
    file.close();
    model.requestImport();
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    const QList<QVariant> reimport = imports.takeFirst();
    QCOMPARE(reimport.at(0).toString(), QStringLiteral("codex-cli"));
    QVERIFY2(reimport.at(3).toString().isEmpty(),
             qPrintable(reimport.at(3).toString()));

    model.requestImport();
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    imports.takeFirst();

    const QString newSessionPath =
        sessionsDir + QStringLiteral("/new-session.jsonl");
    QFile newSession(newSessionPath);
    QVERIFY(newSession.open(QIODevice::WriteOnly));
    const QString newThread = QStringLiteral("created-after-clear");
    const QJsonObject newPayload{
        {QStringLiteral("id"), newThread},
        {QStringLiteral("timestamp"),
         QDateTime::currentDateTimeUtc().addSecs(1).toString(
             Qt::ISODateWithMs)},
        {QStringLiteral("cwd"), QStringLiteral("/private/new-project")}};
    const QByteArray newMetadata =
        QJsonDocument(QJsonObject{{QStringLiteral("type"),
                                   QStringLiteral("session_meta")},
                                  {QStringLiteral("payload"), newPayload}})
            .toJson(QJsonDocument::Compact) +
        '\n';
    const QDateTime newSessionTime =
        QDateTime::currentDateTimeUtc().addSecs(10);
    const QByteArray newInitial = tokenRow(50, newSessionTime, newThread);
    const QByteArray newIncrease =
        tokenRow(60, newSessionTime.addSecs(1), newThread);
    QCOMPARE(
        newSession.write(newMetadata + newInitial + newIncrease),
        qint64(newMetadata.size() + newInitial.size() + newIncrease.size()));
    newSession.close();
    model.requestImport();
    if (imports.isEmpty())
      QVERIFY(imports.wait(5000));
    imports.takeFirst();

    const QString dbPath =
        dataHome + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
    const QString connection = QStringLiteral("clear_rebaseline_verify");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral(
        "SELECT COUNT(*),SUM(input_tokens) FROM local_usage_events")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);
    QCOMPARE(q.value(1).toLongLong(), 60);
    QVERIFY(q.exec(
        QStringLiteral("SELECT input_tokens FROM local_session_counters")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toLongLong(), 1000);
    q.finish();
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);
    qunsetenv("CODEX_HOME");
    qunsetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
  }

  void queriesHandleOneHundredThousandUsageEvents() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XDG_DATA_HOME", QFile::encodeName(dir.path()));
    qputenv("HOME", QFile::encodeName(dir.path()));
    const QString dataRoot =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QVERIFY(
        QDir().mkpath(dataRoot + QStringLiteral("/plasma-ai-usage-monitor")));
    const QString connection = QStringLiteral("session_perf_seed");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(
        dataRoot + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db"));
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=8")));
    QString error;
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(db, &error),
             qPrintable(error));
    QVERIFY(db.transaction());
    for (int i = 0; i < 1000; ++i) {
      q.prepare(QStringLiteral("INSERT INTO "
                               "local_sessions(session_id,tool_id,project_id,"
                               "started_at_utc,last_seen_at_utc) "
                               "VALUES(?,?,?,'2026-10-10T12:00:00.000Z','2026-"
                               "10-10T12:00:00.000Z')"));
      q.addBindValue(QStringLiteral("session-%1").arg(i));
      q.addBindValue(QStringLiteral("codex"));
      q.addBindValue(QStringLiteral("project-%1").arg(i % 20));
      QVERIFY(q.exec());
    }
    q.prepare(QStringLiteral(
        "INSERT INTO "
        "local_usage_events(event_id,session_id,observed_at_utc,model_id,input_"
        "tokens,output_tokens,cache_read_tokens,cache_write_tokens,parser_"
        "revision) "
        "VALUES(?,?,'2026-10-10T12:00:00.000Z','gpt-5-codex',10,2,1,0,1)"));
    for (int i = 0; i < 100000; ++i) {
      q.bindValue(0, QStringLiteral("event-%1").arg(i));
      q.bindValue(1, QStringLiteral("session-%1").arg(i % 1000));
      QVERIFY(q.exec());
    }
    QVERIFY(db.commit());
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);

    LocalSessionModel model;
    QSignalSpy sessionsSpy(&model, &LocalSessionModel::sessionsReady);
    QSignalSpy summarySpy(&model, &LocalSessionModel::summaryReady);
    const auto from = QDateTime::fromString(
        QStringLiteral("2026-10-10T00:00:00.000Z"), Qt::ISODateWithMs);
    const auto to = QDateTime::fromString(
        QStringLiteral("2026-10-11T00:00:00.000Z"), Qt::ISODateWithMs);
    QElapsedTimer timer;
    timer.start();
    model.requestSessions(QStringLiteral("perf-sessions"),
                          QStringLiteral("codex"), from, to, {}, 0, 100,
                          {{"modelId", QStringLiteral("gpt-5-codex")}});
    model.requestSummary(QStringLiteral("perf-summary"),
                         QStringLiteral("codex"), from, to, {},
                         {{"modelId", QStringLiteral("gpt-5-codex")}});
    if (sessionsSpy.isEmpty())
      QVERIFY(sessionsSpy.wait(10000));
    if (summarySpy.isEmpty())
      QVERIFY(summarySpy.wait(10000));
    QCOMPARE(sessionsSpy.takeFirst().at(1).toList().size(), 100);
    QCOMPARE(summarySpy.takeFirst()
                 .at(1)
                 .toMap()
                 .value(QStringLiteral("eventCount"))
                 .toInt(),
             100000);
    QVERIFY2(
        timer.elapsed() < 10000,
        qPrintable(QStringLiteral("Queries took %1 ms").arg(timer.elapsed())));
    qunsetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
  }

  void asyncQueriesReturnBoundedMetadataAndBreakdowns() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qunsetenv("CODEX_HOME");
    qunsetenv("CLAUDE_CONFIG_DIR");
    qputenv("XDG_DATA_HOME", QFile::encodeName(dir.path()));
    const QString home = dir.path() + QStringLiteral("/home");
    QVERIFY(QDir().mkpath(home));
    qputenv("HOME", QFile::encodeName(home));
    const QString dataRoot =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir().mkpath(dataRoot + QStringLiteral("/plasma-ai-usage-monitor"));
    const QString path =
        dataRoot + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
    const QString connection = QStringLiteral("session_model_seed");
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(path);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=8")));
    QString error;
    QVERIFY2(BudgetPolicySchema::migrateSessionSchema(db, &error),
             qPrintable(error));
    QVERIFY(q.exec(
        QStringLiteral("INSERT INTO "
                       "local_sessions(session_id,tool_id,project_id,started_"
                       "at_utc,last_seen_at_utc) "
                       "VALUES('session-id','codex','project-id','2026-10-"
                       "10T10:00:00.000Z','2026-10-10T10:02:00.000Z')")));
    QVERIFY(q.exec(
        QStringLiteral("INSERT INTO "
                       "local_usage_events(event_id,session_id,observed_at_utc,"
                       "model_id,input_tokens,output_tokens,cache_read_tokens,"
                       "cache_write_tokens,parser_revision) "
                       "VALUES('event-id','session-id','2026-10-10T10:02:00."
                       "000Z','gpt-5-codex',100,20,10,5,1)")));
    QVERIFY(q.exec(QStringLiteral(
        "INSERT INTO local_usage_events(event_id,session_id,observed_at_utc,"
        "model_id,input_tokens,output_tokens,cache_read_tokens,"
        "cache_write_tokens,parser_revision) VALUES('old-event','session-id',"
        "'2026-10-09T23:59:59.000Z','gpt-old',900,90,40,30,1)")));
    QVERIFY(
        q.exec(QStringLiteral("UPDATE local_sessions SET "
                              "last_seen_at_utc='2026-10-12T10:00:00.000Z'")));
    QVERIFY(q.exec(
        QStringLiteral("INSERT INTO "
                       "local_usage_events(event_id,session_id,observed_at_utc,"
                       "model_id,input_tokens,output_tokens,parser_revision) "
                       "VALUES('future-event','session-id','2026-10-12T10:00:"
                       "00.000Z','gpt-future',700,70,1)")));
    QVERIFY(q.exec(
        QStringLiteral("INSERT INTO local_project_aliases(project_id,alias) "
                       "VALUES('project-id','Work project')")));
    q.finish();
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(connection);

    LocalSessionModel model;
    QSignalSpy sessionsSpy(&model, &LocalSessionModel::sessionsReady);
    QSignalSpy summarySpy(&model, &LocalSessionModel::summaryReady);
    QSignalSpy projectsSpy(&model, &LocalSessionModel::projectsReady);
    QSignalSpy dailySpy(&model, &LocalSessionModel::dailyReady);
    QSignalSpy exportSpy(&model, &LocalSessionModel::exportReady);
    QSignalSpy importSpy(&model, &LocalSessionModel::importFinished);
    const auto from = QDateTime::fromString(
        QStringLiteral("2026-10-10T00:00:00.000Z"), Qt::ISODateWithMs);
    const auto to = QDateTime::fromString(
        QStringLiteral("2026-10-11T00:00:00.000Z"), Qt::ISODateWithMs);
    model.requestSessions(QStringLiteral("sessions-1"),
                          QStringLiteral("codex-cli"), from, to);
    model.requestSummary(QStringLiteral("summary-1"),
                         QStringLiteral("codex-cli"), from, to);
    model.requestProjects(QStringLiteral("projects-1"));
    model.requestDaily(QStringLiteral("daily-1"), QStringLiteral("codex-cli"),
                       from, to, {}, QStringLiteral("Europe/Stockholm"));
    if (sessionsSpy.isEmpty())
      QVERIFY(sessionsSpy.wait(5000));
    if (summarySpy.isEmpty())
      QVERIFY(summarySpy.wait(5000));
    if (projectsSpy.isEmpty())
      QVERIFY(projectsSpy.wait(5000));
    if (dailySpy.isEmpty())
      QVERIFY(dailySpy.wait(5000));
    const QVariantList sessionRows = sessionsSpy.takeFirst().at(1).toList();
    QCOMPARE(sessionRows.size(), 1);
    QCOMPARE(
        sessionRows.first().toMap().value(QStringLiteral("eventCount")).toInt(),
        1);
    QCOMPARE(sessionRows.first()
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             100);
    QCOMPARE(sessionRows.first()
                 .toMap()
                 .value(QStringLiteral("modelBreakdown"))
                 .toList()
                 .size(),
             1);
    const QVariantMap summary = summarySpy.takeFirst().at(1).toMap();
    QCOMPARE(summary.value(QStringLiteral("inputTokens")).toLongLong(), 100);
    QCOMPARE(summary.value(QStringLiteral("outputTokens")).toLongLong(), 20);
    QCOMPARE(summary.value(QStringLiteral("modelBreakdown"))
                 .toList()
                 .first()
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             100);
    QCOMPARE(summary.value(QStringLiteral("projectBreakdown"))
                 .toList()
                 .first()
                 .toMap()
                 .value(QStringLiteral("inputTokens"))
                 .toLongLong(),
             100);
    QCOMPARE(summary.value(QStringLiteral("projectOptions")).toList().size(),
             1);
    QCOMPARE(summary.value(QStringLiteral("sessionCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("lastActivityAtUtc")).toString(),
             QStringLiteral("2026-10-10T10:02:00.000Z"));
    QCOMPARE(summary.value(QStringLiteral("modelBreakdown")).toList().size(),
             1);
    QCOMPARE(summary.value(QStringLiteral("projectBreakdown")).toList().size(),
             1);
    const QVariantMap project =
        projectsSpy.takeFirst().at(1).toList().first().toMap();
    QCOMPARE(project.value(QStringLiteral("toolId")).toString(),
             QStringLiteral("codex-cli"));
    QCOMPARE(project.value(QStringLiteral("alias")).toString(),
             QStringLiteral("Work project"));
    const QVariantList dailyRows = dailySpy.takeFirst().at(1).toList();
    QCOMPARE(dailyRows.size(), 2);
    QCOMPARE(
        dailyRows.first().toMap().value(QStringLiteral("localDate")).toString(),
        QStringLiteral("2026-10-10"));
    QCOMPARE(
        dailyRows.last().toMap().value(QStringLiteral("localDate")).toString(),
        QStringLiteral("2026-10-11"));
    QCOMPARE(
        dailyRows.first().toMap().value(QStringLiteral("eventCount")).toInt(),
        1);
    QCOMPARE(
        dailyRows.last().toMap().value(QStringLiteral("eventCount")).toInt(),
        0);
    QVERIFY(model.setProjectAlias(QString(64, QLatin1Char('a')),
                                  QStringLiteral("PRIVATE_ALIAS_SENTINEL")));
    const QString exportPath = dir.filePath(QStringLiteral("sessions.json"));
    model.requestExport(QStringLiteral("export-1"), QStringLiteral("codex"),
                        from, to, {}, QStringLiteral("json"), exportPath);
    if (exportSpy.isEmpty())
      QVERIFY(exportSpy.wait(5000));
    QCOMPARE(exportSpy.takeFirst().at(1).toString(), QString());
    QFile exported(exportPath);
    QVERIFY(exported.open(QIODevice::ReadOnly));
    const QByteArray exportBytes = exported.readAll();
    QVERIFY(!exportBytes.contains("PRIVATE_ALIAS_SENTINEL"));
    QVERIFY(!exportBytes.contains("/private/"));
    const QJsonObject exportObject =
        QJsonDocument::fromJson(exportBytes).object();
    QCOMPARE(exportObject.value(QStringLiteral("schemaVersion")).toInt(), 1);
    const QJsonObject exportedRow =
        exportObject.value(QStringLiteral("sessions"))
            .toArray()
            .first()
            .toObject();
    const QSet<QString> allowedKeys{
        QStringLiteral("sessionId"),       QStringLiteral("projectId"),
        QStringLiteral("timestampUtc"),    QStringLiteral("modelId"),
        QStringLiteral("inputTokens"),     QStringLiteral("outputTokens"),
        QStringLiteral("cacheReadTokens"), QStringLiteral("cacheWriteTokens")};
    for (auto it = exportedRow.begin(); it != exportedRow.end(); ++it)
      QVERIFY(allowedKeys.contains(it.key()));
    QVERIFY(exportedRow.value(QStringLiteral("sessionId")).toString().size() ==
            64);
    QVERIFY(exportedRow.value(QStringLiteral("projectId")).toString().size() ==
            64);
    const QString secondExportPath =
        dir.filePath(QStringLiteral("sessions-2.json"));
    model.requestExport(QStringLiteral("export-2"), QStringLiteral("codex"),
                        from, to, {}, QStringLiteral("json"), secondExportPath);
    if (exportSpy.isEmpty())
      QVERIFY(exportSpy.wait(5000));
    QCOMPARE(exportSpy.takeFirst().at(1).toString(), QString());
    QFile secondExport(secondExportPath);
    QVERIFY(secondExport.open(QIODevice::ReadOnly));
    const QJsonObject secondRow =
        QJsonDocument::fromJson(secondExport.readAll())
            .object()
            .value(QStringLiteral("sessions"))
            .toArray()
            .first()
            .toObject();
    QVERIFY(exportedRow.value(QStringLiteral("sessionId")).toString() !=
            secondRow.value(QStringLiteral("sessionId")).toString());
    const QString csvPath = dir.filePath(QStringLiteral("sessions.csv"));
    model.requestExport(QStringLiteral("export-csv"), QStringLiteral("codex"),
                        from, to, {}, QStringLiteral("csv"), csvPath);
    if (exportSpy.isEmpty())
      QVERIFY(exportSpy.wait(5000));
    QCOMPARE(exportSpy.takeFirst().at(1).toString(), QString());
    QFile csv(csvPath);
    QVERIFY(csv.open(QIODevice::ReadOnly));
    const QByteArray csvBytes = csv.readAll();
    QVERIFY(csvBytes.startsWith(
        "sessionId,projectId,timestampUtc,modelId,inputTokens,outputTokens,"
        "cacheReadTokens,cacheWriteTokens\n"));
    QVERIFY(!csvBytes.contains("session-id"));
    QVERIFY(!csvBytes.contains("project-id"));
    QVERIFY(!csvBytes.contains("PRIVATE_ALIAS_SENTINEL"));
    QVERIFY(!csvBytes.contains("/private/"));
    const QString sessionsDirectory = home + QStringLiteral("/.codex/sessions");
    QVERIFY(QDir().mkpath(sessionsDirectory));
    QFile fixture(sessionsDirectory + QStringLiteral("/synthetic.jsonl"));
    QVERIFY(fixture.open(QIODevice::WriteOnly));
    const QByteArray metadata =
        "{\"type\":\"session_meta\",\"payload\":{\"id\":\"thread-private\","
        "\"cwd\":\"/private/sentinel/cwd\"}}\n";
    const QByteArray initial =
        "{\"type\":\"event_msg\",\"timestamp\":\"2026-10-11T10:00:00.000Z\","
        "\"payload\":{\"type\":\"token_count\",\"info\":{\"model\":\"gpt-5-"
        "codex-test\",\"total_token_usage\":{\"input_tokens\":500,\"output_"
        "tokens\":"
        "40,\"cached_input_tokens\":80,\"cache_write_input_tokens\":12}}}}\n";
    const QByteArray unchanged =
        "{\"type\":\"event_msg\",\"timestamp\":\"2026-10-11T10:00:01.000Z\","
        "\"payload\":{\"type\":\"token_count\",\"info\":{\"model\":\"gpt-5-"
        "codex-test\",\"total_token_usage\":{\"input_tokens\":500,\"output_"
        "tokens\":"
        "40,\"cached_input_tokens\":80,\"cache_write_input_tokens\":12}}}}\n";
    const QByteArray increased =
        "{\"type\":\"event_msg\",\"timestamp\":\"2026-10-11T10:00:02.000Z\","
        "\"payload\":{\"type\":\"token_count\",\"info\":{\"model\":\"gpt-5-"
        "codex-test\",\"total_token_usage\":{\"input_tokens\":550,\"output_"
        "tokens\":"
        "65,\"cached_input_tokens\":100,\"cache_write_input_tokens\":12}}}}\n";
    const QByteArray reset =
        "{\"type\":\"event_msg\",\"timestamp\":\"2026-10-11T10:00:03.000Z\","
        "\"payload\":{\"type\":\"token_count\",\"info\":{\"model\":\"gpt-5-"
        "codex-test\",\"total_token_usage\":{\"input_tokens\":3,\"output_"
        "tokens\":2,"
        "\"cached_input_tokens\":1,\"cache_write_input_tokens\":0}}}}\n";
    QCOMPARE(fixture.write(metadata + initial + unchanged + increased + reset),
             qint64(metadata.size() + initial.size() + unchanged.size() +
                    increased.size() + reset.size()));
    fixture.close();
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    if (importSpy.isEmpty())
      QVERIFY(importSpy.wait(5000));
    QCOMPARE(importSpy.first().at(0).toString(), QStringLiteral("codex-cli"));
    QVERIFY(importSpy.first().at(2).toBool());
    const QString verifyConnection = QStringLiteral("session_import_verify");
    auto verifyDb =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnection);
    verifyDb.setDatabaseName(path);
    QVERIFY(verifyDb.open());
    QSqlQuery verify(verifyDb);
    QVERIFY(verify.exec(
        QStringLiteral("SELECT COUNT(*),SUM(input_tokens),SUM(output_tokens),"
                       "SUM(cache_read_tokens),SUM(cache_write_tokens) FROM "
                       "local_usage_events "
                       "WHERE model_id='gpt-5-codex-test'")));
    QVERIFY(verify.next());
    QCOMPARE(verify.value(0).toInt(), 3);
    QCOMPARE(verify.value(1).toLongLong(), 550);
    QCOMPARE(verify.value(2).toLongLong(), 65);
    QCOMPARE(verify.value(3).toLongLong(), 100);
    QCOMPARE(verify.value(4).toLongLong(), 12);
    verify.finish();
    verifyDb.close();
    verifyDb = {};
    QSqlDatabase::removeDatabase(verifyConnection);
    LocalSessionModel restarted;
    QSignalSpy restartedSummary(&restarted, &LocalSessionModel::summaryReady);
    QSignalSpy restartedImport(&restarted, &LocalSessionModel::importFinished);
    restarted.setToolEnabled(QStringLiteral("codex-cli"), true);
    restarted.requestSummary(QStringLiteral("persisted-coverage"),
                             QStringLiteral("codex-cli"), from, to);
    if (restartedSummary.isEmpty())
      QVERIFY(restartedSummary.wait(5000));
    const QVariantMap persisted = restartedSummary.takeFirst().at(1).toMap();
    QCOMPARE(persisted.value(QStringLiteral("toolCoverage")).toString(),
             QStringLiteral("partial"));
    QCOMPARE(persisted.value(QStringLiteral("importStatus")).toString(),
             QStringLiteral("partial"));
    if (restartedImport.isEmpty())
      QVERIFY(restartedImport.wait(5000));
    qunsetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
  }
};

QTEST_GUILESS_MAIN(LocalSessionModelTest)
#include "test_localsessionmodel.moc"
