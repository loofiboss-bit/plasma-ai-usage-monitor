#ifndef FORECASTCONTRACT_H
#define FORECASTCONTRACT_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace ForecastContract {

enum class Kind {
    QuotaExhaustion,
    BudgetOverrun,
};

enum class State {
    Unavailable,
    Safe,
    Warning,
    Critical,
};

enum class EvidenceGrade {
    Unavailable,
    Usable,
    Strong,
};

enum class ValueClass {
    Actual,
    Estimated,
};

struct Result {
    Kind kind = Kind::QuotaExhaustion;
    State state = State::Unavailable;
    QString sourceId;
    QString sourceKind;
    QString window;
    QString scope;
    std::optional<double> currentValue;
    std::optional<double> projectedValue;
    std::optional<double> limitValue;
    QString unit;
    std::optional<QString> currency;
    std::optional<QDateTime> predictedAt;
    QDateTime periodEnd;
    int sampleCount = 0;
    double coveragePercent = 0.0;
    EvidenceGrade evidenceGrade = EvidenceGrade::Unavailable;
    QString methodId;
    QString reasonKey;
    QDateTime generatedAt;
    ValueClass valueClass = ValueClass::Actual;

    QString stableId() const;
    QVariantMap toVariantMap() const;
    bool isValid(QString *diagnostic = nullptr) const;
};

QString kindKey(Kind kind);
QString stateKey(State state);
QString evidenceGradeKey(EvidenceGrade grade);
QString valueClassKey(ValueClass valueClass);

std::optional<Result> fromVariantMap(const QVariantMap &map, QString *diagnostic = nullptr);
QStringList unavailableReasonKeys();
QString transitionFor(State previous, State current);

} // namespace ForecastContract

#endif // FORECASTCONTRACT_H
