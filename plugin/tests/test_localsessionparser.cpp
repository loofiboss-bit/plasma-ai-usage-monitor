#include "localsessionparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class LocalSessionParserTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void codexUsesTurnContextAndAllowsUnknownModels() {
    const QByteArray key(32, 'c');
    QVariantMap context;
    QVERIFY(LocalSessionParser::updateCodexContext(
        R"({"type":"session_meta","payload":{"id":"thread","cwd":"/private/project"}})",
        &context));
    const QByteArray record =
        R"({"type":"event_msg","timestamp":"2026-10-12T10:00:00Z","payload":{"type":"token_count","info":{"total_token_usage":{"input_tokens":100,"output_tokens":10,"cached_input_tokens":20}}}})";
    auto parsed =
        LocalSessionParser::parseRecord("codex", record, key, context);
    QVERIFY(!parsed.isEmpty());
    QVERIFY(!parsed.value("modelId").isNull());
    QCOMPARE(parsed.value("modelId").toString(), QStringLiteral(""));
    for (const QString &model :
         {QStringLiteral("gpt-5-codex"), QStringLiteral("gpt-5")}) {
      const QByteArray turn =
          QJsonDocument(QJsonObject{{"type", "turn_context"},
                                    {"payload", QJsonObject{{"model", model}}}})
              .toJson();
      QVERIFY(LocalSessionParser::updateCodexContext(turn, &context));
      parsed = LocalSessionParser::parseRecord("codex", record, key, context);
      QCOMPARE(parsed.value("modelId").toString(), model);
      QCOMPARE(parsed.value("inputTokens").toLongLong(), 100);
    }
  }

  void rejectsUnsupportedAndOversizedRecords() {
    const QByteArray key(32, 'k');
    QVERIFY(LocalSessionParser::parseRecord("unknown", "{}", key).isEmpty());
    QVERIFY(LocalSessionParser::parseRecord(
                "claude-code", QByteArray(1024 * 1024 + 1, ' '), key)
                .isEmpty());
    QVERIFY(LocalSessionParser::parseRecord("claude-code", "{not-json", key)
                .isEmpty());
    QVERIFY(LocalSessionParser::parseRecord("claude-code", "{}",
                                            QByteArray("short"))
                .isEmpty());
  }

  void parsesOnlyAllowlistedClaudeMetadata() {
    const QByteArray key(32, 'x');
    const QByteArray record =
        R"({"type":"assistant","sessionId":"session-secret","cwd":"/private/sentinel/project","timestamp":"2026-10-11T10:00:00.000Z","transcript":"PRIVATE_PROMPT_SENTINEL","message":{"id":"message-1","model":"claude-opus-4-1","content":"PRIVATE_REPLY_SENTINEL","usage":{"input_tokens":120,"output_tokens":30,"cache_read_input_tokens":20,"cache_creation_input_tokens":10}}})";
    const QVariantMap parsed = LocalSessionParser::parseRecord(
        QStringLiteral("claude-code"), record, key);
    QVERIFY(!parsed.isEmpty());
    QCOMPARE(parsed.value(QStringLiteral("inputTokens")).toLongLong(), 150);
    QCOMPARE(parsed.value(QStringLiteral("outputTokens")).toLongLong(), 30);
    QCOMPARE(parsed.value(QStringLiteral("cacheReadTokens")).toLongLong(), 20);
    QCOMPARE(parsed.value(QStringLiteral("cacheWriteTokens")).toLongLong(), 10);
    const QByteArray serialized = QJsonDocument::fromVariant(parsed).toJson();
    QVERIFY(!serialized.contains("PRIVATE_PROMPT_SENTINEL"));
    QVERIFY(!serialized.contains("PRIVATE_REPLY_SENTINEL"));
    QVERIFY(!serialized.contains("/private/sentinel/project"));
    QVERIFY(!serialized.contains("session-secret"));
    QVERIFY(!serialized.contains("message-1"));
    QCOMPARE(parsed.value(QStringLiteral("sessionId")).toString().size(), 64);
  }

  void missingClaudeCacheUsageStaysUnavailable() {
    const QByteArray key(32, 'q');
    for (const QByteArray &cache :
         {QByteArray(""), QByteArray(",\"cache_read_input_tokens\":null")}) {
      const QByteArray record =
          QByteArray(
              R"({"type":"assistant","sessionId":"s","cwd":"/tmp/private","timestamp":"2026-10-11T10:00:00Z","message":{"id":"m","model":"claude-sonnet","usage":{"input_tokens":4,"output_tokens":2,"cache_creation_input_tokens":3)") +
          cache + QByteArray("}}}");
      const QVariantMap parsed = LocalSessionParser::parseRecord(
          QStringLiteral("claude-code"), record, key);
      QVERIFY(!parsed.isEmpty());
      QVERIFY(parsed.value(QStringLiteral("usagePartial")).toBool());
      QVERIFY(!parsed.contains(QStringLiteral("inputTokens")));
      QCOMPARE(parsed.value(QStringLiteral("outputTokens")).toLongLong(), 2);
    }
  }

  void requiresCodexCumulativeCountersAndStableIdentity() {
    const QByteArray key(32, 'y');
    const QByteArray record =
        R"({"type":"event_msg","thread_id":"thread-secret","cwd":"/private/project","timestamp":"2026-10-11T10:00:00.000Z","payload":{"type":"token_count","info":{"model":"gpt-5-codex","total_token_usage":{"input_tokens":500,"output_tokens":40}}},"other":"DO_NOT_STORE"})";
    const QVariantMap parsed =
        LocalSessionParser::parseRecord(QStringLiteral("codex"), record, key);
    QVERIFY(!parsed.isEmpty());
    QVERIFY(parsed.value(QStringLiteral("cumulative")).toBool());
    QCOMPARE(parsed.value(QStringLiteral("inputTokens")).toLongLong(), 500);
    QVERIFY(!parsed.contains(QStringLiteral("other")));
    QVERIFY(LocalSessionParser::parseRecord(QStringLiteral("codex"), "{}", key)
                .isEmpty());
    QVERIFY(LocalSessionParser::parseRecord(QStringLiteral("codex"), record,
                                            QByteArray(32, 'z'))
                .value(QStringLiteral("sessionId")) !=
            parsed.value(QStringLiteral("sessionId")));
  }

  void usesSessionMetaContextAndIncludesCodexCacheCounters() {
    const QByteArray key(32, 'm');
    QVariantMap context;
    QVERIFY(LocalSessionParser::updateCodexContext(
        R"({"type":"session_meta","payload":{"id":"thread-private","cwd":"/private/sentinel/cwd"}})",
        &context));
    const QByteArray record =
        R"({"type":"event_msg","timestamp":"2026-10-11T10:00:00.000Z","payload":{"type":"token_count","info":{"model":"gpt-5-codex","total_token_usage":{"input_tokens":500,"output_tokens":40,"cached_input_tokens":80,"cache_write_input_tokens":12}}},"content":"PRIVATE_SENTINEL"})";
    const QVariantMap parsed = LocalSessionParser::parseRecord(
        QStringLiteral("codex"), record, key, context);
    QVERIFY(!parsed.isEmpty());
    QCOMPARE(parsed.value(QStringLiteral("inputTokens")).toLongLong(), 500);
    QCOMPARE(parsed.value(QStringLiteral("cacheReadTokens")).toLongLong(), 80);
    QCOMPARE(parsed.value(QStringLiteral("cacheWriteTokens")).toLongLong(), 12);
    const QByteArray serialized = QJsonDocument::fromVariant(parsed).toJson();
    QVERIFY(!serialized.contains("thread-private"));
    QVERIFY(!serialized.contains("/private/sentinel/cwd"));
    QVERIFY(!serialized.contains("PRIVATE_SENTINEL"));
  }

  void marksUnverifiedForkAndAgentLineage() {
    const QByteArray key(32, 'f');
    const QByteArray record =
        R"({"type":"event_msg","thread_id":"thread","cwd":"/work","timestamp":"2026-10-11T10:00:00Z","payload":{"type":"token_count","info":{"model":"gpt-5-codex","total_token_usage":{"input_tokens":500,"output_tokens":40}},"forked_from":"parent-thread"}})";
    const QVariantMap parsed =
        LocalSessionParser::parseRecord(QStringLiteral("codex"), record, key);
    QVERIFY(parsed.value(QStringLiteral("lineageUnverified")).toBool());
    QCOMPARE(parsed.value(QStringLiteral("coverage")).toString(),
             QStringLiteral("partial-lineage"));
    QVERIFY(parsed.contains(QStringLiteral("inputTokens")));
  }
};

QTEST_GUILESS_MAIN(LocalSessionParserTest)
#include "test_localsessionparser.moc"
