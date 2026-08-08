#ifndef RUNWAYQUERY_H
#define RUNWAYQUERY_H

#include "forecastcontract.h"
#include "quotarunwayquery.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <optional>

class RunwayQuery final {
public:
    using QuotaSample = QuotaRunwayQuery::Sample;
    using QuotaRequest = QuotaRunwayQuery::Request;

    struct BudgetDay {
        QDateTime periodStart;
        QDateTime periodEnd;
        std::optional<double> value;
        QString currency;
        ForecastContract::ValueClass valueClass = ForecastContract::ValueClass::Actual;
    };

    struct BudgetRequest {
        QString sourceId;
        QString sourceKind;
        QString window = QStringLiteral("calendar_month");
        QString scope;
        double budget = 0.0;
        QString budgetCurrency;
        ForecastContract::ValueClass valueClass = ForecastContract::ValueClass::Actual;
        QList<BudgetDay> days;
    };

    static ForecastContract::Result quotaRunway(const QuotaRequest &request, const QDateTime &generatedAt);
    static ForecastContract::Result budgetPacing(const BudgetRequest &request, const QDateTime &generatedAt);

    /**
     * Execute direct fixture series and optional read-only database descriptors.
     *
     * Direct request keys:
     * - quotaSeries: list of quota request maps with nested samples
     * - budgetSeries: list of budget request maps with nested days
     *
     * Database request keys:
     * - databasePath
     * - quotaSources: observation query descriptors
     * - budgets: cost observation query descriptors
     * - generatedAt: optional deterministic UTC calculation time
     */
    static QVariantList execute(const QVariantMap &request, const std::atomic_bool *cancelled = nullptr);

    static QVariantMap methodContract();
    static QString defaultDatabasePath();

    static constexpr int MinimumQuotaSamples = QuotaRunwayQuery::MinimumSamples;
    static constexpr qint64 MinimumQuotaSpanSeconds = QuotaRunwayQuery::MinimumSpanSeconds;
    static constexpr qint64 MaximumQuotaAgeSeconds = QuotaRunwayQuery::MaximumAgeSeconds;
    static constexpr int MaximumTheilSenSamples = QuotaRunwayQuery::MaximumTheilSenSamples;
    static constexpr int MinimumBudgetDays = 5;
    static constexpr double MinimumBudgetCoveragePercent = 70.0;
};

#endif // RUNWAYQUERY_H
