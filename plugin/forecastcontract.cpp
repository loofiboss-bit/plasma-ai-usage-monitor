#include "forecastcontract.h"

#include <QCryptographicHash>

#include <cmath>

namespace ForecastContract {
namespace {

template <typename Enum> std::optional<Enum> enumForKey(const QString &key, const QList<QPair<QString, Enum>> &values)
{
    for (const auto &[candidate, value] : values) {
        if (candidate == key) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<double> optionalDouble(const QVariantMap &map, const QString &key, bool *valid)
{
    const QVariant value = map.value(key);
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        *valid = false;
        return std::nullopt;
    }
    return number;
}

std::optional<QString> optionalString(const QVariantMap &map, const QString &key)
{
    const QVariant value = map.value(key);
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    const QString text = value.toString().trimmed();
    return text.isEmpty() ? std::nullopt : std::optional<QString>(text);
}

std::optional<QDateTime> optionalDateTime(const QVariantMap &map, const QString &key, bool *valid)
{
    const QVariant value = map.value(key);
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    const QDateTime dateTime = value.toDateTime();
    if (!dateTime.isValid()) {
        *valid = false;
        return std::nullopt;
    }
    return dateTime.toUTC();
}

void insertOptional(QVariantMap *map, const QString &key, const std::optional<double> &value)
{
    map->insert(key, value ? QVariant(*value) : QVariant());
}

} // namespace

QString Result::stableId() const
{
    const QByteArray identity = QStringList { sourceId, kindKey(kind), window, scope, valueClassKey(valueClass),
        currency ? currency->trimmed().toUpper() : QString() }
                                    .join(QChar(0x1f))
                                    .toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}

QVariantMap Result::toVariantMap() const
{
    QVariantMap map {
        { QStringLiteral("kind"), kindKey(kind) },
        { QStringLiteral("state"), stateKey(state) },
        { QStringLiteral("sourceId"), sourceId },
        { QStringLiteral("sourceKind"), sourceKind },
        { QStringLiteral("window"), window },
        { QStringLiteral("scope"), scope },
        { QStringLiteral("unit"), unit },
        { QStringLiteral("periodEnd"), periodEnd.toUTC() },
        { QStringLiteral("sampleCount"), sampleCount },
        { QStringLiteral("coveragePercent"), coveragePercent },
        { QStringLiteral("evidenceGrade"), evidenceGradeKey(evidenceGrade) },
        { QStringLiteral("methodId"), methodId },
        { QStringLiteral("reasonKey"), reasonKey },
        { QStringLiteral("generatedAt"), generatedAt.toUTC() },
        { QStringLiteral("valueClass"), valueClassKey(valueClass) },
    };
    insertOptional(&map, QStringLiteral("currentValue"), currentValue);
    insertOptional(&map, QStringLiteral("projectedValue"), projectedValue);
    insertOptional(&map, QStringLiteral("limitValue"), limitValue);
    map.insert(QStringLiteral("currency"), currency ? QVariant(*currency) : QVariant());
    map.insert(QStringLiteral("predictedAt"), predictedAt ? QVariant(predictedAt->toUTC()) : QVariant());
    return map;
}

bool Result::isValid(QString *diagnostic) const
{
    const auto fail = [diagnostic](const QString &message) {
        if (diagnostic) {
            *diagnostic = message;
        }
        return false;
    };
    if (sourceId.trimmed().isEmpty() || sourceKind.trimmed().isEmpty() || window.trimmed().isEmpty()
        || scope.trimmed().isEmpty()) {
        return fail(QStringLiteral("forecast identity is incomplete"));
    }
    if (unit.trimmed().isEmpty() || methodId.trimmed().isEmpty()) {
        return fail(QStringLiteral("forecast method or unit is missing"));
    }
    if ((kind == Kind::BudgetOverrun
            && (!currency || currency->trimmed().isEmpty() || unit.compare(*currency, Qt::CaseInsensitive) != 0))
        || (kind == Kind::QuotaExhaustion && currency)) {
        return fail(QStringLiteral("forecast currency contract is invalid"));
    }
    if (!periodEnd.isValid() || !generatedAt.isValid()) {
        return fail(QStringLiteral("forecast UTC boundaries are invalid"));
    }
    if (sampleCount < 0 || !std::isfinite(coveragePercent) || coveragePercent < 0.0 || coveragePercent > 100.0) {
        return fail(QStringLiteral("forecast evidence values are invalid"));
    }
    if (currentValue && (!std::isfinite(*currentValue) || *currentValue < 0.0)) {
        return fail(QStringLiteral("forecast current value is invalid"));
    }
    if (projectedValue && (!std::isfinite(*projectedValue) || *projectedValue < 0.0)) {
        return fail(QStringLiteral("forecast projected value is invalid"));
    }
    if (limitValue && (!std::isfinite(*limitValue) || *limitValue < 0.0)) {
        return fail(QStringLiteral("forecast limit value is invalid"));
    }
    if (state == State::Unavailable) {
        if (reasonKey.trimmed().isEmpty() || !unavailableReasonKeys().contains(reasonKey)) {
            return fail(QStringLiteral("forecast unavailable reason is invalid"));
        }
        if (evidenceGrade != EvidenceGrade::Unavailable) {
            return fail(QStringLiteral("unavailable forecast has an evidence grade"));
        }
        if (predictedAt) {
            return fail(QStringLiteral("unavailable forecast predicts an event"));
        }
    } else {
        if (!currentValue || !projectedValue || !limitValue) {
            return fail(QStringLiteral("available forecast values are incomplete"));
        }
        if (evidenceGrade == EvidenceGrade::Unavailable) {
            return fail(QStringLiteral("available forecast lacks usable evidence"));
        }
        if (!reasonKey.trimmed().isEmpty()) {
            return fail(QStringLiteral("available forecast has an unavailable reason"));
        }
        if ((state == State::Warning || state == State::Critical) && !predictedAt) {
            return fail(QStringLiteral("risk forecast lacks a predicted time"));
        }
        if (state == State::Safe && predictedAt) {
            return fail(QStringLiteral("safe forecast predicts an event"));
        }
    }
    return true;
}

QString kindKey(Kind kind)
{
    switch (kind) {
    case Kind::QuotaExhaustion:
        return QStringLiteral("quota_exhaustion");
    case Kind::BudgetOverrun:
        return QStringLiteral("budget_overrun");
    }
    return { };
}

QString stateKey(State state)
{
    switch (state) {
    case State::Unavailable:
        return QStringLiteral("unavailable");
    case State::Safe:
        return QStringLiteral("safe");
    case State::Warning:
        return QStringLiteral("warning");
    case State::Critical:
        return QStringLiteral("critical");
    }
    return { };
}

QString evidenceGradeKey(EvidenceGrade grade)
{
    switch (grade) {
    case EvidenceGrade::Unavailable:
        return QStringLiteral("unavailable");
    case EvidenceGrade::Usable:
        return QStringLiteral("usable");
    case EvidenceGrade::Strong:
        return QStringLiteral("strong");
    }
    return { };
}

QString valueClassKey(ValueClass valueClass)
{
    switch (valueClass) {
    case ValueClass::Actual:
        return QStringLiteral("actual");
    case ValueClass::Estimated:
        return QStringLiteral("estimated");
    }
    return { };
}

std::optional<Result> fromVariantMap(const QVariantMap &map, QString *diagnostic)
{
    const QStringList requiredKeys {
        QStringLiteral("kind"),
        QStringLiteral("state"),
        QStringLiteral("sourceId"),
        QStringLiteral("sourceKind"),
        QStringLiteral("window"),
        QStringLiteral("scope"),
        QStringLiteral("currentValue"),
        QStringLiteral("projectedValue"),
        QStringLiteral("limitValue"),
        QStringLiteral("unit"),
        QStringLiteral("currency"),
        QStringLiteral("predictedAt"),
        QStringLiteral("periodEnd"),
        QStringLiteral("sampleCount"),
        QStringLiteral("coveragePercent"),
        QStringLiteral("evidenceGrade"),
        QStringLiteral("methodId"),
        QStringLiteral("reasonKey"),
        QStringLiteral("generatedAt"),
        QStringLiteral("valueClass"),
    };
    for (const QString &key : requiredKeys) {
        if (!map.contains(key)) {
            if (diagnostic) {
                *diagnostic = QStringLiteral("forecast required field is missing: %1").arg(key);
            }
            return std::nullopt;
        }
    }

    const auto kind = enumForKey<Kind>(map.value(QStringLiteral("kind")).toString(),
        { { QStringLiteral("quota_exhaustion"), Kind::QuotaExhaustion },
            { QStringLiteral("budget_overrun"), Kind::BudgetOverrun } });
    const auto state = enumForKey<State>(map.value(QStringLiteral("state")).toString(),
        { { QStringLiteral("unavailable"), State::Unavailable }, { QStringLiteral("safe"), State::Safe },
            { QStringLiteral("warning"), State::Warning }, { QStringLiteral("critical"), State::Critical } });
    const auto evidenceGrade = enumForKey<EvidenceGrade>(map.value(QStringLiteral("evidenceGrade")).toString(),
        { { QStringLiteral("unavailable"), EvidenceGrade::Unavailable },
            { QStringLiteral("usable"), EvidenceGrade::Usable }, { QStringLiteral("strong"), EvidenceGrade::Strong } });
    const auto valueClass = enumForKey<ValueClass>(map.value(QStringLiteral("valueClass")).toString(),
        { { QStringLiteral("actual"), ValueClass::Actual }, { QStringLiteral("estimated"), ValueClass::Estimated } });
    if (!kind || !state || !evidenceGrade || !valueClass) {
        if (diagnostic) {
            *diagnostic = QStringLiteral("forecast enum value is invalid");
        }
        return std::nullopt;
    }

    bool valuesValid = true;
    Result result;
    result.kind = *kind;
    result.state = *state;
    result.sourceId = map.value(QStringLiteral("sourceId")).toString();
    result.sourceKind = map.value(QStringLiteral("sourceKind")).toString();
    result.window = map.value(QStringLiteral("window")).toString();
    result.scope = map.value(QStringLiteral("scope")).toString();
    result.currentValue = optionalDouble(map, QStringLiteral("currentValue"), &valuesValid);
    result.projectedValue = optionalDouble(map, QStringLiteral("projectedValue"), &valuesValid);
    result.limitValue = optionalDouble(map, QStringLiteral("limitValue"), &valuesValid);
    result.unit = map.value(QStringLiteral("unit")).toString();
    result.currency = optionalString(map, QStringLiteral("currency"));
    result.predictedAt = optionalDateTime(map, QStringLiteral("predictedAt"), &valuesValid);
    result.periodEnd = map.value(QStringLiteral("periodEnd")).toDateTime().toUTC();
    result.sampleCount = map.value(QStringLiteral("sampleCount")).toInt();
    result.coveragePercent = map.value(QStringLiteral("coveragePercent")).toDouble();
    result.evidenceGrade = *evidenceGrade;
    result.methodId = map.value(QStringLiteral("methodId")).toString();
    result.reasonKey = map.value(QStringLiteral("reasonKey")).toString();
    result.generatedAt = map.value(QStringLiteral("generatedAt")).toDateTime().toUTC();
    result.valueClass = *valueClass;
    if (!valuesValid || !result.isValid(diagnostic)) {
        return std::nullopt;
    }
    return result;
}

QStringList unavailableReasonKeys()
{
    return {
        QStringLiteral("insufficient_samples"),
        QStringLiteral("insufficient_span"),
        QStringLiteral("stale_data"),
        QStringLiteral("missing_value"),
        QStringLiteral("unsupported_source"),
        QStringLiteral("incompatible_window"),
        QStringLiteral("reset_detected"),
        QStringLiteral("non_monotonic"),
        QStringLiteral("no_consumption"),
        QStringLiteral("missing_budget"),
        QStringLiteral("mixed_currency"),
        QStringLiteral("currency_mismatch"),
        QStringLiteral("mixed_value_class"),
        QStringLiteral("insufficient_coverage"),
        QStringLiteral("incomplete_period"),
        QStringLiteral("cancelled"),
        QStringLiteral("query_failed"),
    };
}

QString transitionFor(State previous, State current)
{
    if (current == State::Warning && previous != State::Warning) {
        return QStringLiteral("warning");
    }
    if (current == State::Critical && previous != State::Critical) {
        return QStringLiteral("critical");
    }
    if ((previous == State::Warning || previous == State::Critical)
        && (current == State::Safe || current == State::Unavailable)) {
        return QStringLiteral("recovered");
    }
    return { };
}

} // namespace ForecastContract
