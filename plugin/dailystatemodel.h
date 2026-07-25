#ifndef DAILYSTATEMODEL_H
#define DAILYSTATEMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class ProviderBackend;
class SourceReadinessModel;
class SubscriptionToolBackend;

class DailyStateModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
  Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
  Q_PROPERTY(int warningThreshold READ warningThreshold WRITE
                 setWarningThreshold NOTIFY thresholdsChanged)
  Q_PROPERTY(int criticalThreshold READ criticalThreshold WRITE
                 setCriticalThreshold NOTIFY thresholdsChanged)

public:
  enum Role {
    StableIdRole = Qt::UserRole + 1,
    DisplayNameRole,
    SourceKindRole,
    MonitoringLevelRole,
    ReadinessStateRole,
    QualityClassRole,
    FreshnessStateRole,
    LastSuccessRole,
    LastAttemptRole,
    LastErrorKindRole,
    NextActionKeyRole,
    HasUsefulDataRole,
    HasActualDataRole,
    HasEstimatedDataRole,
    HasBalanceRole,
    ConnectivityOnlyRole,
    AttentionSeverityRole,
    AttentionReasonKeyRole,
    PrimaryMetricKindRole,
    PrimaryMetricAvailableRole,
    PrimaryMetricValueRole,
    PrimaryMetricUnitRole,
    PercentUsedAvailableRole,
    PercentUsedRole,
    PercentRemainingAvailableRole,
    PercentRemainingRole,
    ResetAtAvailableRole,
    ResetAtRole,
    CurrencyRole,
    CostAvailableRole,
    CostValueRole,
    CostSourceRole,
    BudgetAvailableRole,
    BudgetPercentUsedRole,
    QuotaWindowsRole
  };
  Q_ENUM(Role)

  explicit DailyStateModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  QVariantMap summary() const;
  int warningThreshold() const;
  int criticalThreshold() const;
  void setWarningThreshold(int threshold);
  void setCriticalThreshold(int threshold);

  Q_INVOKABLE void registerReadinessModel(QObject *model);
  Q_INVOKABLE void registerProviderBackend(const QString &stableId,
                                           QObject *backend);
  Q_INVOKABLE void registerLocalTool(const QString &stableId, QObject *backend);
  Q_INVOKABLE QVariantMap source(const QString &stableId) const;
  Q_INVOKABLE QStringList prioritizedSourceIds() const;
  Q_INVOKABLE void refresh();

Q_SIGNALS:
  void countChanged();
  void summaryChanged();
  void sourceChanged(const QString &stableId);
  void thresholdsChanged();

private:
  QVariantMap buildRow(const QVariantMap &readiness, int sourceOrder) const;
  QVariantMap buildProviderRow(QVariantMap row, ProviderBackend *backend) const;
  QVariantMap buildToolRow(QVariantMap row,
                           SubscriptionToolBackend *backend) const;
  QVariantMap finalizeRow(QVariantMap row, int sourceOrder) const;
  QVariantMap buildSummary(const QList<QVariantMap> &rows) const;
  void connectProvider(const QString &stableId, ProviderBackend *backend);
  void connectTool(const QString &stableId, SubscriptionToolBackend *backend);
  void rebuild();

  QPointer<SourceReadinessModel> m_readinessModel;
  QHash<QString, QPointer<ProviderBackend>> m_providerBackends;
  QHash<QString, QPointer<SubscriptionToolBackend>> m_toolBackends;
  QList<QVariantMap> m_rows;
  QVariantMap m_summary;
  int m_warningThreshold = 80;
  int m_criticalThreshold = 95;
};

#endif // DAILYSTATEMODEL_H
