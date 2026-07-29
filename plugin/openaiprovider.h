#ifndef OPENAIPROVIDER_H
#define OPENAIPROVIDER_H

#include "providerbackend.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

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
    QML_ELEMENT

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

protected:
    virtual QDateTime currentDateTimeUtc() const;

private:
    enum class RequestKind {
        Usage,
        DailyCosts,
        MonthlyCosts,
    };

    struct PaginationState {
        QUrl baseUrl;
        QJsonArray buckets;
        QHash<QString, QByteArray> bucketPayloads;
        QSet<QByteArray> pagePayloads;
        QSet<QString> requestedCursors;
        int generation = 0;
        bool pending = false;
    };

    void fetchUsage();
    void fetchCosts();
    void fetchMonthlyCosts();
    void startPagination(RequestKind kind, const QUrl &url);
    void requestPage(RequestKind kind, const QString &cursor = QString());
    void handlePage(RequestKind kind, QNetworkReply *reply, bool retriesExhausted = false);
    void finishPagination(RequestKind kind, bool complete, const QString &diagnostic = QString(),
        ProviderErrorKind errorKind = ProviderErrorKind::None, int httpStatus = 0,
        const QDateTime &retryAfter = QDateTime());
    bool appendUniqueBuckets(PaginationState &state, const QJsonArray &buckets, QString *diagnostic);
    void publishUsage(const QJsonArray &buckets);
    void publishDailyCosts(const QJsonArray &buckets);
    void publishMonthlyCosts(const QJsonArray &buckets);
    PaginationState &paginationState(RequestKind kind);
    QString capabilityName(RequestKind kind) const;
    bool hasSuccessfulValue(RequestKind kind) const;
    void checkAllDone();

    QString m_projectId;
    QString m_model = QStringLiteral("gpt-5.4-pro");
    PaginationState m_usagePagination;
    PaginationState m_dailyCostsPagination;
    PaginationState m_monthlyCostsPagination;
    int m_pendingRequests = 0;
    int m_logicalPageRequests = 0;
    bool m_hasSuccessfulUsage = false;
    bool m_hasSuccessfulDailyCosts = false;
    bool m_hasSuccessfulMonthlyCosts = false;

    // At most six logical pages are started per refresh. ProviderBackend retries
    // each page at most twice, so a refresh performs at most 18 HTTP attempts.
    static constexpr int MAX_LOGICAL_PAGE_REQUESTS = 6;
    static constexpr int USAGE_BUCKET_LIMIT = 31;
    static constexpr int COST_BUCKET_LIMIT = 180;
    static constexpr const char *BASE_URL = "https://api.openai.com/v1";
};

#endif // OPENAIPROVIDER_H
