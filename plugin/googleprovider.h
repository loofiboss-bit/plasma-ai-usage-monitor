#ifndef GOOGLEPROVIDER_H
#define GOOGLEPROVIDER_H

#include "providerbackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * Google Gemini provider backend.
 *
 * Google does NOT have a dedicated usage/billing API for the Gemini API.
 * Scheduled refresh uses the read-only models.list endpoint. countTokens is
 * available only as an explicit diagnostic.
 *
 * Rate limits are not exposed as live remaining values. Published caps stay
 * in catalog metadata and are never projected into compatibility fields.
 *
 * Endpoint: POST /v1beta/models/{model}:countTokens
 */
class GoogleProvider : public ProviderBackend
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString tier READ tier WRITE setTier NOTIFY tierChanged)
    Q_PROPERTY(QVariantList discoveredModels READ discoveredModels NOTIFY discoveredModelsChanged)
    Q_PROPERTY(QDateTime modelsLastDiscovered READ modelsLastDiscovered NOTIFY discoveredModelsChanged)
    Q_PROPERTY(bool selectedModelAvailable READ selectedModelAvailable NOTIFY discoveredModelsChanged)
    Q_PROPERTY(QString selectedModelWarning READ selectedModelWarning NOTIFY discoveredModelsChanged)

public:
    explicit GoogleProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Google Gemini"); }
    QString iconName() const override { return QStringLiteral("globe"); }

    QString model() const;
    void setModel(const QString &model);

    QString tier() const;
    void setTier(const QString &tier);

    void refreshImpl() override;
    QVariantList discoveredModels() const { return m_discoveredModels; }
    QDateTime modelsLastDiscovered() const { return m_modelsLastDiscovered; }
    bool selectedModelAvailable() const;
    QString selectedModelWarning() const;
    Q_INVOKABLE void countTokensDiagnostic();
    Q_INVOKABLE void refreshModelsNow();

Q_SIGNALS:
    void modelChanged();
    void tierChanged();
    void discoveredModelsChanged();

private Q_SLOTS:
    void onModelsReply(QNetworkReply *reply);
    void onCountTokensReply(QNetworkReply *reply);

private:
    void fetchStatus();
    void fetchModels(const QString &pageToken = QString());
    void finalizeModelDiscovery();
    void loadDiscoveryCache();
    void saveDiscoveryCache() const;

    QString m_model = QStringLiteral("gemini-3.1-flash-lite");
    QString m_tier = QStringLiteral("free");
    QVariantList m_discoveredModels;
    QVariantList m_pendingLiveModels;
    QStringList m_seenModelIds;
    int m_discoveryPageCount = 0;
    QDateTime m_modelsLastDiscovered;

    static constexpr const char *BASE_URL = "https://generativelanguage.googleapis.com/v1beta";
};

#endif // GOOGLEPROVIDER_H
