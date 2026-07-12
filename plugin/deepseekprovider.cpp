#include "deepseekprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QDebug>

DeepSeekProvider::DeepSeekProvider(QObject *parent)
    : OpenAICompatibleProvider(parent)
{
    setModel(QStringLiteral("deepseek-v4-flash"));
    registerCatalogPricing(QStringLiteral("deepseek"));
}

double DeepSeekProvider::balance() const { return m_balance; }

void DeepSeekProvider::refreshImpl()
{
    if (!hasApiKey()) {
        setErrorDetails(i18n("No API key configured"), ProviderErrorKind::Configuration);
        setConnected(false);
        return;
    }

    // Call parent's refresh which sets loading, clears error,
    // resets pending count, and kicks off chat completion request
    OpenAICompatibleProvider::refreshImpl();

    // Additionally fetch the balance (adds to pending count)
    fetchBalance();
}

void DeepSeekProvider::fetchBalance()
{
    // DeepSeek has a balance endpoint
    QUrl url(QStringLiteral("%1/user/balance").arg(effectiveBaseUrl(defaultBaseUrl())));

    QNetworkRequest request = createRequest(url);

    addPendingRequest();
    int gen = currentGeneration();
    QNetworkReply *reply = networkManager()->get(request);
    trackReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        if (!isCurrentGeneration(gen)) { reply->deleteLater(); return; }
        onBalanceReply(reply);
    });
}

void DeepSeekProvider::onBalanceReply(QNetworkReply *reply)
{
    reply->deleteLater();
    decrementPendingRequest();

    if (reply->error() != QNetworkReply::NoError) {
        // Non-fatal: rate limit data may still be available
        setNetworkError(reply, i18n("Balance API unavailable: %1", reply->errorString()));
        onAllRequestsDone();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        QJsonObject root = doc.object();
        // DeepSeek balance response: { "is_available": true, "balance_infos": [...] }
        QJsonArray balances = root.value(QStringLiteral("balance_infos")).toArray();
        double totalBalance = 0.0;
        for (const QJsonValue &bal : balances) {
            QJsonObject b = bal.toObject();
            const QJsonValue balanceValue = b.value(QStringLiteral("total_balance"));
            double parsedBalance = 0.0;

            if (balanceValue.isDouble()) {
                parsedBalance = balanceValue.toDouble();
            } else {
                bool ok = false;
                const double fromString = balanceValue.toString().toDouble(&ok);
                if (ok) {
                    parsedBalance = fromString;
                }
            }

            totalBalance += parsedBalance;
        }
        // Store remaining balance separately — this is NOT spending
        m_balance = totalBalance;
        Q_EMIT balanceChanged();
    }

    onAllRequestsDone();
}
