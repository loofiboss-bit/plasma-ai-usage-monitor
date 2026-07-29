#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

class AnthropicAdminParser final {
public:
  struct UsageRow {
    QDateTime periodStart;
    QDateTime periodEnd;
    QString model;
    QString project;
    QString serviceTier;
    qint64 input = 0;
    qint64 cacheRead = 0;
    qint64 cacheCreation = 0;
    qint64 output = 0;
  };

  struct CostRow {
    QDateTime periodStart;
    QDateTime periodEnd;
    QString model;
    QString project;
    QString serviceTier;
    QString lineItem;
    qint64 microUsd = 0;
  };

  struct Pagination {
    bool valid = false;
    bool complete = false;
    QUrl nextUrl;
    QString diagnostic;
  };

  static bool parseUsagePage(const QJsonObject &root, QList<UsageRow> *rows,
                             QString *diagnostic);
  static bool parseCostPage(const QJsonObject &root, QList<CostRow> *rows,
                            QString *diagnostic);
  static bool parseMicroUsd(const QString &fractionalCents, qint64 *microUsd);
  static Pagination pagination(const QJsonObject &root, const QUrl &currentUrl,
                               int pages, int maximumPages);
};
