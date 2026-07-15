#include "deepseekprovider.h"
#include <KLocalizedString>
#include <QNetworkRequest>
#include <QDebug>
#include <QMap>

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

    // The shared scheduled refresh is a read-only model-list request.
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
        setCapabilityStatus(QStringLiteral("balance"), QStringLiteral("failed"),
                            i18n("Balance is unavailable; model discovery may still succeed"));
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
        QMap<QString, double> totals;
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

            const QString currency = b.value(QStringLiteral("currency")).toString(QStringLiteral("UNKNOWN")).toUpper();
            totals[currency] += parsedBalance;
        }
        m_balancesByCurrency.clear();
        for (auto it = totals.cbegin(); it != totals.cend(); ++it) {
            m_balancesByCurrency.insert(it.key(), it.value());
            setProviderMetric(MetricKind::CreditBalance, it.value(), it.key(), it.key(),
                              QStringLiteral("api_key"), QStringLiteral("current"),
                              MetricSource::UsageApi, QStringLiteral("actual"));
        }
        // Preserve the legacy scalar only when a single currency makes it
        // meaningful. Mixed balances remain separate in Metric Contract v2.
        m_balanceAvailable = totals.size() == 1;
        if (m_balanceAvailable) m_balance = totals.cbegin().value();
        setCapabilityStatus(QStringLiteral("balance"),
                            totals.isEmpty() ? QStringLiteral("unavailable") : QStringLiteral("available"));
        Q_EMIT balanceChanged();
    }

    onAllRequestsDone();
}
