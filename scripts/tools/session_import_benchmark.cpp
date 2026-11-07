#include "budgetpolicyschema.h"
#include "localsessionmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir fixture;
    if (!fixture.isValid()) return 1;
    const QString codex = fixture.filePath("codex");
    const QString data = fixture.filePath("data");
    const QString database = data + "/plasma-ai-usage-monitor/usage_history.db";
    QDir().mkpath(codex + "/sessions");
    QDir().mkpath(QFileInfo(database).absolutePath());
    qputenv("HOME", QFile::encodeName(fixture.path()));
    qputenv("XDG_DATA_HOME", QFile::encodeName(data));
    qputenv("CODEX_HOME", QFile::encodeName(codex));
    qputenv("CLAUDE_CONFIG_DIR", QFile::encodeName(fixture.filePath("claude")));
    auto db = QSqlDatabase::addDatabase("QSQLITE", "benchmark");
    db.setDatabaseName(database);
    QString error;
    if (!db.open()) return 2;
    QSqlQuery query(db);
    if (!query.exec("PRAGMA user_version=8") || !BudgetPolicySchema::migrateSessionSchema(db, &error)) return 3;
    QFile file(codex + "/sessions/synthetic.jsonl");
    if (!file.open(QIODevice::WriteOnly)) return 4;
    file.write("{\"type\":\"session_meta\",\"payload\":{\"id\":\"benchmark-session\",\"cwd\":\"/synthetic/project\"}}\n");
    for (int i=1; i<=100000; ++i) {
        const auto line = QByteArray("{\"type\":\"event_msg\",\"timestamp\":\"2026-11-01T00:00:00.000Z\",\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{\"input_tokens\":")
            + QByteArray::number(i*10) + ",\"output_tokens\":" + QByteArray::number(i*2)
            + ",\"cached_input_tokens\":0}}}}\n";
        if (file.write(line) != line.size()) return 5;
    }
    const qint64 fixtureBytes = file.size();
    file.close();
    LocalSessionModel model;
    model.setRetentionDays(3650);
    QSignalSpy imports(&model, &LocalSessionModel::importFinished);
    QElapsedTimer timer;
    timer.start();
    model.setToolEnabled(QStringLiteral("codex-cli"), true);
    int cycles=0;
    int rows=0;
    while (rows<100000 && timer.elapsed()<180000) {
        if (imports.isEmpty() && !imports.wait(30000)) return 6;
        while (!imports.isEmpty()) {
            const auto result=imports.takeFirst();
            if (!result.at(3).toString().isEmpty()) {
                std::cerr << result.at(3).toString().toStdString(); return 7;
            }
            ++cycles;
        }
        if (!query.exec("SELECT COUNT(*) FROM local_usage_events") || !query.next()) return 8;
        rows=query.value(0).toInt();
        query.finish();
        if (rows<100000) model.requestImport();
    }
    const auto elapsed=timer.elapsed();
    if (rows != 100000) { std::cerr << "incomplete rows=" << rows; return 9; }
    std::cout << QJsonDocument(QJsonObject{{"rows",rows},{"importMs",elapsed},
        {"importCycles",cycles},{"fixtureBytes",fixtureBytes}}).toJson(QJsonDocument::Compact).constData() << '\n';
    return 0;
}
