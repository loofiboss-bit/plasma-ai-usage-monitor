#ifndef DESCRIPTORPROVIDER_H
#define DESCRIPTORPROVIDER_H

#include "providerbackend.h"

#include <QSet>

class DescriptorProvider final : public ProviderBackend
{
    Q_OBJECT
    Q_PROPERTY(QString providerId READ providerId CONSTANT)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString monitoringLevel READ monitoringLevel CONSTANT)
    Q_PROPERTY(bool manualProbeOnly READ manualProbeOnly CONSTANT)
    Q_PROPERTY(QVariantList discoveredModels READ discoveredModels NOTIFY discoveredModelsChanged)
    Q_PROPERTY(QDateTime modelsLastDiscovered READ modelsLastDiscovered NOTIFY discoveredModelsChanged)
    Q_PROPERTY(bool selectedModelAvailable READ selectedModelAvailable NOTIFY discoveredModelsChanged)

public:
    explicit DescriptorProvider(const QVariantMap &descriptor, QObject *parent = nullptr);

    QString name() const override;
    QString iconName() const override;
    QString providerId() const;
    QString model() const;
    void setModel(const QString &model);
    QString monitoringLevel() const;
    bool manualProbeOnly() const;
    QVariantList discoveredModels() const { return m_discoveredModels; }
    QDateTime modelsLastDiscovered() const { return m_modelsLastDiscovered; }
    bool selectedModelAvailable() const;
    void refreshImpl() override;

Q_SIGNALS:
    void modelChanged();
    void discoveredModelsChanged();

private:
    void handleReply(QNetworkReply *reply);
    void requestPage(const QUrl &url, int generation);
    void finalizeModelDiscovery(bool partial = false, const QString &reason = QString());
    QVariantMap m_descriptor;
    QString m_model;
    QVariantList m_discoveredModels;
    QVariantList m_pendingDiscoveredModels;
    QSet<QString> m_pendingModelIds;
    QDateTime m_modelsLastDiscovered;
    int m_pageRequests = 0;
};

#endif
