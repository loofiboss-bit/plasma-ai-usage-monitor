#ifndef GOOGLEVEOPROVIDER_H
#define GOOGLEVEOPROVIDER_H

#include "providerbackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * Google Veo provider backend (video generation).
 *
 * Similar to GoogleProvider but tracks Veo video-generation models.
 * Uses a lightweight GET /v1beta/models/{model} call to verify
 * API key validity and connectivity.
 *
 * Rate limits are applied from known Google documentation values.
 *
 * Models: veo-3, veo-2
 */
class GoogleVeoProvider : public ProviderBackend
{
    Q_OBJECT

    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString tier READ tier WRITE setTier NOTIFY tierChanged)

public:
    explicit GoogleVeoProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Google Veo"); }
    QString iconName() const override { return QStringLiteral("camera-video"); }

    QString model() const;
    void setModel(const QString &model);

    QString tier() const;
    void setTier(const QString &tier);

    void refreshImpl() override;

Q_SIGNALS:
    void modelChanged();
    void tierChanged();

private Q_SLOTS:
    void onModelInfoReply(QNetworkReply *reply);

private:
    void fetchModelInfo();

    QString m_model = QStringLiteral("veo-3");
    QString m_tier = QStringLiteral("paid");

    static constexpr const char *BASE_URL = "https://generativelanguage.googleapis.com/v1beta";
};

#endif // GOOGLEVEOPROVIDER_H
