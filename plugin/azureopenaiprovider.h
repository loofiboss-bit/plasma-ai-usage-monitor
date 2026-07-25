#ifndef AZUREOPENAIPROVIDER_H
#define AZUREOPENAIPROVIDER_H

#include "providerbackend.h"

/**
 * Azure OpenAI provider backend.
 *
 * Scheduled refresh uses a read-only model-list endpoint. A deployment-scoped
 * completion is available only as an explicit, potentially billable test.
 *
 * Notes:
 * - Authentication uses `api-key` header (not Bearer token)
 * - Rate limit headers are parsed from x-ratelimit-*
 * - Usage tokens are parsed from the completion response body
 * - Cost is estimated from token usage via model pricing table
 */
class AzureOpenAIProvider : public ProviderBackend
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString deploymentId READ deploymentId WRITE setDeploymentId NOTIFY deploymentIdChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion WRITE setApiVersion NOTIFY apiVersionChanged)

public:
    explicit AzureOpenAIProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("Azure OpenAI"); }
    QString iconName() const override { return QStringLiteral("globe"); }

    QString model() const;
    void setModel(const QString &model);

    QString deploymentId() const;
    void setDeploymentId(const QString &deploymentId);

    QString apiVersion() const;
    void setApiVersion(const QString &apiVersion);

    void refreshImpl() override;
    Q_INVOKABLE void testConnectionNow();

Q_SIGNALS:
    void modelChanged();
    void deploymentIdChanged();
    void apiVersionChanged();

private Q_SLOTS:
    void onCompletionReply(QNetworkReply *reply);
    void onModelsReply(QNetworkReply *reply);

private:
    QString endpointBaseUrl() const;
    QUrl completionUrl() const;
    QUrl modelsUrl() const;

    QString m_model = QStringLiteral("gpt-5.4-pro");
    QString m_deploymentId;
    QString m_apiVersion = QStringLiteral("2024-10-21");
    QByteArray m_lastRequestBody;

    qint64 m_sessionInputTokens = 0;
    qint64 m_sessionOutputTokens = 0;
    int m_sessionRequestCount = 0;
    double m_sessionTotalCost = 0.0;
    double m_sessionDailyCost = 0.0;
    double m_sessionMonthlyCost = 0.0;

    static constexpr int REQUEST_TIMEOUT_MS = 30000;
};

#endif // AZUREOPENAIPROVIDER_H
