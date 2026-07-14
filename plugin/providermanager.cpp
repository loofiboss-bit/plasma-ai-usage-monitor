#include "providermanager.h"
#include "descriptorprovider.h"
#include "providerpricingcatalog.h"

ProviderManager::ProviderManager(QObject *parent) : QAbstractListModel(parent)
{
    m_descriptors = ProviderPricingCatalog::instance()->providers();
    for (const QVariant &entry : std::as_const(m_descriptors)) {
        const QVariantMap row = entry.toMap();
        const QString id = row.value(QStringLiteral("stableId")).toString();
        if (!id.isEmpty() && !row.value(QStringLiteral("adapterType")).toString().isEmpty())
            m_backends.insert(id, new DescriptorProvider(row, this));
    }
}

int ProviderManager::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_descriptors.size(); }
QVariant ProviderManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_descriptors.size()) return {};
    const QVariantMap row = m_descriptors.at(index.row()).toMap();
    const QString id = row.value(QStringLiteral("stableId")).toString();
    if (role == StableIdRole) return id;
    if (role == DisplayNameRole) return row.value(QStringLiteral("displayName"));
    if (role == DescriptorRole) return row;
    if (role == BackendRole) return QVariant::fromValue(m_backends.value(id));
    return {};
}
QHash<int, QByteArray> ProviderManager::roleNames() const
{
    return {{StableIdRole,"stableId"},{DisplayNameRole,"displayName"},{DescriptorRole,"descriptor"},{BackendRole,"backend"}};
}
QObject *ProviderManager::backend(const QString &stableId) const { return m_backends.value(stableId); }
QVariantMap ProviderManager::descriptor(const QString &stableId) const
{
    for (const QVariant &entry : m_descriptors) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("stableId")).toString() == stableId) return row;
    }
    return {};
}
void ProviderManager::registerBackend(const QString &stableId, QObject *backend)
{
    if (stableId.isEmpty() || !backend || m_backends.value(stableId) == backend) return;
    QObject *previous = m_backends.value(stableId);
    m_backends.insert(stableId, backend);
    if (qobject_cast<DescriptorProvider *>(previous) && previous->parent() == this)
        previous->deleteLater();
    const int row = [&]() { for (int i=0;i<m_descriptors.size();++i) if (m_descriptors.at(i).toMap().value(QStringLiteral("stableId")).toString()==stableId) return i; return -1; }();
    if (row >= 0) Q_EMIT dataChanged(index(row), index(row), {BackendRole});
}
