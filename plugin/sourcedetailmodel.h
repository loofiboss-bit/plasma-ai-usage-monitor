#ifndef SOURCEDETAILMODEL_H
#define SOURCEDETAILMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class DailyStateModel;
class UsageDatabase;

class SourceDetailModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(
      QString sourceId READ sourceId WRITE setSourceId NOTIFY sourceChanged)
  Q_PROPERTY(int count READ rowCount NOTIFY sourceChanged)
  Q_PROPERTY(QVariantMap source READ source NOTIFY sourceChanged)
  Q_PROPERTY(QString actionLabel READ actionLabel NOTIFY sourceChanged)
  Q_PROPERTY(QVariantList quotaWindows READ quotaWindows NOTIFY sourceChanged)
  Q_PROPERTY(QVariantMap coverage READ coverage NOTIFY sourceChanged)
  Q_PROPERTY(QVariantMap recentHistory READ recentHistory NOTIFY historyChanged)
  Q_PROPERTY(bool historyLoading READ historyLoading NOTIFY historyChanged)
  Q_PROPERTY(QString historyId READ historyId NOTIFY sourceChanged)
  Q_PROPERTY(QString historyMetric READ historyMetric NOTIFY sourceChanged)

public:
  enum Role {
    KindRole = Qt::UserRole + 1,
    AvailableRole,
    ValueRole,
    UnitRole,
    CurrencyRole,
    SourceRole,
    QualityRole,
    SemanticRole,
    ScopeRole,
    WindowRole,
    ResetAtRole
  };
  Q_ENUM(Role)

  explicit SourceDetailModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  QString sourceId() const;
  void setSourceId(const QString &sourceId);
  QVariantMap source() const;
  QString actionLabel() const;
  QVariantList quotaWindows() const;
  QVariantMap coverage() const;
  QVariantMap recentHistory() const;
  bool historyLoading() const;
  QString historyId() const;
  QString historyMetric() const;

  Q_INVOKABLE void registerDailyState(QObject *model);
  Q_INVOKABLE void registerHistoryDatabase(QObject *database);
  Q_INVOKABLE void refreshHistory();

Q_SIGNALS:
  void sourceChanged();
  void historyChanged();

private:
  void rebuild();
  void handleHistoryResult(const QString &requestId,
                           const QVariantMap &payload);
  QString chooseHistoryMetric() const;
  static QString actionLabelFor(const QVariantMap &source);

  QPointer<DailyStateModel> m_dailyState;
  QPointer<UsageDatabase> m_historyDatabase;
  QString m_sourceId;
  QVariantMap m_source;
  QVariantList m_metrics;
  QVariantMap m_coverage;
  QVariantMap m_recentHistory;
  QString m_historyRequestId;
  bool m_historyLoading = false;
};

#endif // SOURCEDETAILMODEL_H
