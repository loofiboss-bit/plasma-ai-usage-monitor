#ifndef OPENAIPROVIDER_H
#define OPENAIPROVIDER_H

#include "providerbackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * OpenAI provider backend.
 *
 * Queries:
 * - GET /organization/usage/completions  -- token usage (bucketed)
 * - GET /organization/costs              -- dollar costs
 * - Rate limit headers from responses
 *
 * Requires an Admin API key for usage/costs endpoints.
 */
class OpenAIProvider : public ProviderBackend
{
    Q_OBJECT

    Q_PROPERTY(QString projectId READ projectId WRITE setProjectId NOTIFY projectIdChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit OpenAIProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("OpenAI"); }
    QString iconName() const override { return QStringLiteral("globe"); }

    QString projectId() const;
    void setProjectId(const QString &id);

    QString model() const;
    void setModel(const QString &model);

    void refreshImpl() override;

Q_SIGNALS:
    void projectIdChanged();
    void modelChanged();

private Q_SLOTS:
    void onUsageReply(QNetworkReply *reply);
    void onCostsReply(QNetworkReply *reply);
    void onMonthlyCostsReply(QNetworkReply *reply);

private:
    void fetchUsage();
    void fetchCosts();
    void fetchMonthlyCosts();
    void checkAllDone();

    QString m_projectId;
    QString m_model = QStringLiteral("gpt-5.4-pro");
    int m_pendingRequests = 0;

    static constexpr const char *BASE_URL = "https://api.openai.com/v1";
};

#endif // OPENAIPROVIDER_H
