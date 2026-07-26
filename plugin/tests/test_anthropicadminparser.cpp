#include "anthropicadminparser.h"

#include <QJsonArray>
#include <QTest>
#include <QUrlQuery>

class TestAnthropicAdminParser : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parsesAndRoundsMicroUsd();
  void aggregatesDuplicateUsageRows();
  void validatesPagination();
};

void TestAnthropicAdminParser::parsesAndRoundsMicroUsd() {
  qint64 value = 0;
  QVERIFY(
      AnthropicAdminParser::parseMicroUsd(QStringLiteral("1.23456"), &value));
  QCOMPARE(value, 12346);
  QVERIFY(
      AnthropicAdminParser::parseMicroUsd(QStringLiteral("-0.00005"), &value));
  QCOMPARE(value, -1);
  QVERIFY(!AnthropicAdminParser::parseMicroUsd(QStringLiteral("1e3"), &value));
}

void TestAnthropicAdminParser::aggregatesDuplicateUsageRows() {
  const QJsonObject result{
      {QStringLiteral("model"), QStringLiteral("claude-test")},
      {QStringLiteral("workspace_id"), QStringLiteral("workspace-1")},
      {QStringLiteral("service_tier"), QStringLiteral("standard")},
      {QStringLiteral("uncached_input_tokens"), 10},
      {QStringLiteral("cache_read_input_tokens"), 5},
      {QStringLiteral("cache_creation"),
       QJsonObject{{QStringLiteral("ephemeral_5m_input_tokens"), 2}}},
      {QStringLiteral("output_tokens"), 4},
  };
  const QJsonObject bucket{
      {QStringLiteral("starting_at"), QStringLiteral("2026-07-25T00:00:00Z")},
      {QStringLiteral("ending_at"), QStringLiteral("2026-07-26T00:00:00Z")},
      {QStringLiteral("results"), QJsonArray{result, result}},
  };
  const QJsonObject page{
      {QStringLiteral("data"), QJsonArray{bucket}},
      {QStringLiteral("has_more"), false},
  };

  QList<AnthropicAdminParser::UsageRow> rows;
  QString diagnostic;
  QVERIFY(AnthropicAdminParser::parseUsagePage(page, &rows, &diagnostic));
  QCOMPARE(rows.size(), 1);
  QCOMPARE(rows.constFirst().input, 20);
  QCOMPARE(rows.constFirst().cacheRead, 10);
  QCOMPARE(rows.constFirst().cacheCreation, 4);
  QCOMPARE(rows.constFirst().output, 8);
}

void TestAnthropicAdminParser::validatesPagination() {
  const QUrl current(
      QStringLiteral("https://api.anthropic.com/report?limit=31&page=old"));
  const auto next = AnthropicAdminParser::pagination(
      QJsonObject{{QStringLiteral("has_more"), true},
                  {QStringLiteral("next_page"), QStringLiteral("next-token")}},
      current, 1, 64);
  QVERIFY(next.valid);
  QVERIFY(!next.complete);
  QCOMPARE(QUrlQuery(next.nextUrl).queryItemValue(QStringLiteral("page")),
           QStringLiteral("next-token"));

  const auto limited = AnthropicAdminParser::pagination(
      QJsonObject{{QStringLiteral("has_more"), true},
                  {QStringLiteral("next_page"), QStringLiteral("next-token")}},
      current, 64, 64);
  QVERIFY(!limited.valid);
  QVERIFY(limited.diagnostic.contains(QStringLiteral("safety limit")));
}

QTEST_GUILESS_MAIN(TestAnthropicAdminParser)
#include "test_anthropicadminparser.moc"
