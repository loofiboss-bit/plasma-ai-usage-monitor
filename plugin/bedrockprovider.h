#ifndef BEDROCKPROVIDER_H
#define BEDROCKPROVIDER_H

#include "providerbackend.h"

class BedrockProvider : public ProviderBackend
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString secretAccessKey READ secretAccessKey WRITE setSecretAccessKey NOTIFY credentialsChanged)
    Q_PROPERTY(QString sessionToken READ sessionToken WRITE setSessionToken NOTIFY credentialsChanged)

public:
    explicit BedrockProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("AWS Bedrock"); }
    QString iconName() const override { return QStringLiteral("network-server"); }

    QString region() const;
    void setRegion(const QString &region);

    QString model() const;
    void setModel(const QString &model);

    QString accessKeyId() const;
    void setAccessKeyId(const QString &accessKeyId);

    QString secretAccessKey() const;
    void setSecretAccessKey(const QString &secret);

    QString sessionToken() const;
    void setSessionToken(const QString &token);

    void refreshImpl() override;

Q_SIGNALS:
    void regionChanged();
    void modelChanged();
    void credentialsChanged();

private:
    QString endpointBase() const;

    QString m_region = QStringLiteral("us-east-1");
    QString m_model;
    QString m_secretAccessKey;
    QString m_sessionToken;
};

#endif // BEDROCKPROVIDER_H
