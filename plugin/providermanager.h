#ifndef PROVIDERMANAGER_H
#define PROVIDERMANAGER_H

#include <QAbstractListModel>
#include <QHash>

class ProviderManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { StableIdRole = Qt::UserRole + 1, DisplayNameRole, DescriptorRole, BackendRole };
    Q_ENUM(Role)
    explicit ProviderManager(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QObject *backend(const QString &stableId) const;
    Q_INVOKABLE QVariantMap descriptor(const QString &stableId) const;
    Q_INVOKABLE void registerBackend(const QString &stableId, QObject *backend);

Q_SIGNALS:
    void countChanged();

private:
    QVariantList m_descriptors;
    QHash<QString, QObject *> m_backends;
};

#endif
