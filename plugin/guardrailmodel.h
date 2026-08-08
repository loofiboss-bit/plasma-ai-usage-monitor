#ifndef GUARDRAILMODEL_H
#define GUARDRAILMODEL_H

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVariantList>
#include <QVariantMap>

#include <QtQmlIntegration/qqmlintegration.h>

#include <atomic>
#include <memory>

class GuardrailModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantMap query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList forecasts READ forecasts NOTIFY forecastsChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int generation READ generation NOTIFY generationChanged)
    Q_PROPERTY(int pendingWorkerCount READ pendingWorkerCount NOTIFY pendingWorkerCountChanged)
    Q_PROPERTY(QVariantMap methodContract READ methodContract CONSTANT)

public:
    enum Role {
        StableIdRole = Qt::UserRole + 1,
        KindRole,
        StateRole,
        SourceIdRole,
        SourceKindRole,
        WindowRole,
        ScopeRole,
        CurrentValueRole,
        ProjectedValueRole,
        LimitValueRole,
        UnitRole,
        CurrencyRole,
        PredictedAtRole,
        PeriodEndRole,
        SampleCountRole,
        CoveragePercentRole,
        EvidenceGradeRole,
        MethodIdRole,
        ReasonKeyRole,
        ReasonTextRole,
        GeneratedAtRole,
        ValueClassRole,
    };
    Q_ENUM(Role)

    explicit GuardrailModel(QObject *parent = nullptr);
    ~GuardrailModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVariantMap query() const;
    void setQuery(const QVariantMap &query);
    QVariantList forecasts() const;
    bool isBusy() const;
    int generation() const;
    int pendingWorkerCount() const;
    QVariantMap methodContract() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshWithQuery(const QVariantMap &query);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void invalidateCache();
    Q_INVOKABLE QString localizedReason(const QString &reasonKey) const;

Q_SIGNALS:
    void queryChanged();
    void forecastsChanged();
    void busyChanged();
    void generationChanged();
    void pendingWorkerCountChanged();
    void completed(int generation);

private:
    struct Work {
        int generation = 0;
        QByteArray cacheKey;
        std::shared_ptr<std::atomic_bool> cancelled;
        QFutureWatcher<QVariantList> *watcher = nullptr;
    };

    void start(const QVariantMap &query);
    void finish(const std::shared_ptr<Work> &work);
    QVariantMap decorated(const QVariantMap &forecast) const;
    QByteArray cacheKey(const QVariantMap &query) const;

    QVariantMap m_query;
    QVariantList m_forecasts;
    int m_generation = 0;
    int m_pendingWorkerCount = 0;
    QList<std::shared_ptr<Work>> m_work;
    QHash<QByteArray, QVariantList> m_cache;
    QList<QByteArray> m_cacheOrder;
};

#endif // GUARDRAILMODEL_H
