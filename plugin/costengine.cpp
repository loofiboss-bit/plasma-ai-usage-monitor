#include "costengine.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {

constexpr qint64 kRateScale = 1'000'000'000LL;
constexpr qint64 kTokenScale = 1'000'000LL;

using WideInt = __int128_t;

QString wideToString(WideInt value)
{
    if (value == 0) {
        return QStringLiteral("0");
    }

    const bool negative = value < 0;
    if (negative) {
        value = -value;
    }

    QString digits;
    while (value > 0) {
        digits.prepend(QChar::fromLatin1('0' + static_cast<char>(value % 10)));
        value /= 10;
    }
    return negative ? QStringLiteral("-") + digits : digits;
}

QString trimDecimal(QString value)
{
    value = value.trimmed();
    if (value.contains(QLatin1Char('e'), Qt::CaseInsensitive)) {
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (!ok || !std::isfinite(parsed)) {
            return {};
        }
        value = QString::number(parsed, 'f', 12);
    }
    return value;
}

WideInt parseScaled(const QVariant &value, qint64 scale, bool *ok)
{
    *ok = false;
    QString text;
    if (value.userType() == QMetaType::Double || value.userType() == QMetaType::Float) {
        const double number = value.toDouble();
        if (!std::isfinite(number)) {
            return 0;
        }
        text = QString::number(number, 'f', 12);
    } else if (value.canConvert<QString>()) {
        text = value.toString();
    } else {
        return 0;
    }

    text = trimDecimal(text);
    if (text.isEmpty()) {
        return 0;
    }

    bool negative = false;
    if (text.startsWith(QLatin1Char('+')) || text.startsWith(QLatin1Char('-'))) {
        negative = text.startsWith(QLatin1Char('-'));
        text.remove(0, 1);
    }

    const QStringList parts = text.split(QLatin1Char('.'));
    if (parts.size() > 2 || (parts.first().isEmpty() && parts.size() == 1)) {
        return 0;
    }

    const QString integerPart = parts.first().isEmpty() ? QStringLiteral("0") : parts.first();
    if (!integerPart.contains(QRegularExpression(QStringLiteral("^\\d+$")))) {
        return 0;
    }

    QString fractionalPart = parts.size() == 2 ? parts.at(1) : QString();
    if (!fractionalPart.isEmpty()
        && !fractionalPart.contains(QRegularExpression(QStringLiteral("^\\d+$")))) {
        return 0;
    }

    bool integerOk = false;
    const WideInt integerValue = integerPart.toLongLong(&integerOk);
    if (!integerOk) {
        return 0;
    }

    const int scaleDigits = QString::number(scale).size() - 1;
    if (fractionalPart.size() > scaleDigits + 1) {
        fractionalPart.truncate(scaleDigits + 1);
    }
    const bool roundUp = fractionalPart.size() > scaleDigits
        && fractionalPart.at(scaleDigits) >= QLatin1Char('5');
    fractionalPart.truncate(scaleDigits);
    fractionalPart += QString(scaleDigits - fractionalPart.size(), QLatin1Char('0'));

    bool fractionalOk = false;
    const WideInt fractionalValue = fractionalPart.isEmpty()
        ? 0
        : fractionalPart.toLongLong(&fractionalOk);
    if (!fractionalPart.isEmpty() && !fractionalOk) {
        return 0;
    }

    WideInt result = integerValue * scale + fractionalValue;
    if (roundUp) {
        ++result;
    }
    *ok = true;
    return negative ? -result : result;
}

WideInt roundDivide(WideInt numerator, WideInt denominator)
{
    if (denominator <= 0) {
        return 0;
    }
    const bool negative = numerator < 0;
    if (negative) {
        numerator = -numerator;
    }
    WideInt result = (numerator + denominator / 2) / denominator;
    return negative ? -result : result;
}

WideInt decimalQuantityToRate(const QVariant &quantity, bool *ok)
{
    return parseScaled(quantity, kTokenScale, ok);
}

WideInt rateValue(const QVariant &value, bool *ok)
{
    return parseScaled(value, kRateScale, ok);
}

double scaledToDouble(WideInt value, qint64 scale)
{
    return static_cast<double>(value) / static_cast<double>(scale);
}

QString scaledToText(WideInt value, qint64 scale)
{
    const bool negative = value < 0;
    if (negative) {
        value = -value;
    }
    const QString digits = wideToString(value);
    const int fractionalDigits = QString::number(scale).size() - 1;
    QString padded = digits;
    if (padded.size() <= fractionalDigits) {
        padded.prepend(QString(fractionalDigits + 1 - padded.size(), QLatin1Char('0')));
    }
    const int split = padded.size() - fractionalDigits;
    QString result = padded.left(split) + QLatin1Char('.') + padded.mid(split);
    while (result.endsWith(QLatin1Char('0'))) {
        result.chop(1);
    }
    if (result.endsWith(QLatin1Char('.'))) {
        result.chop(1);
    }
    return negative ? QStringLiteral("-") + result : result;
}

void addMissing(QStringList &missing, const QString &dimension)
{
    if (!dimension.isEmpty() && !missing.contains(dimension)) {
        missing.append(dimension);
    }
}

QVariant valueForFirstKey(const QVariantMap &map, const QStringList &keys)
{
    for (const QString &key : keys) {
        if (map.contains(key) && !map.value(key).isNull()) {
            return map.value(key);
        }
    }
    return {};
}

int currencyMinorDigits(const QString &currency)
{
    if (currency == QLatin1String("JPY") || currency == QLatin1String("KRW")
        || currency == QLatin1String("CLP") || currency == QLatin1String("VND")) {
        return 0;
    }
    if (currency == QLatin1String("KWD") || currency == QLatin1String("BHD")
        || currency == QLatin1String("JOD") || currency == QLatin1String("OMR")) {
        return 3;
    }
    if (currency == QLatin1String("USD") || currency == QLatin1String("EUR")
        || currency == QLatin1String("GBP") || currency == QLatin1String("SEK")
        || currency == QLatin1String("NOK") || currency == QLatin1String("DKK")
        || currency == QLatin1String("CAD") || currency == QLatin1String("AUD")
        || currency == QLatin1String("CHF") || currency == QLatin1String("CNY")
        || currency == QLatin1String("INR") || currency == QLatin1String("BRL")) {
        return 2;
    }
    return -1;
}

QDateTime parseTimestamp(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return {};
    }
    const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (parsed.isValid()) {
        return parsed.toUTC();
    }
    return QDateTime::fromString(value.toString(), Qt::ISODate).toUTC();
}

QVariantMap component(const QString &kind,
                      WideInt quantity,
                      WideInt amount,
                      const QVariant &unitRate,
                      const QString &unit)
{
    QVariantMap row;
    row.insert(QStringLiteral("kind"), kind);
    row.insert(QStringLiteral("quantity"), scaledToDouble(quantity, kTokenScale));
    row.insert(QStringLiteral("quantityText"), scaledToText(quantity, kTokenScale));
    row.insert(QStringLiteral("unit"), unit);
    row.insert(QStringLiteral("unitRate"), unitRate);
    row.insert(QStringLiteral("amount"), scaledToDouble(amount, kRateScale));
    row.insert(QStringLiteral("amountText"), scaledToText(amount, kRateScale));
    return row;
}

void mergeRateMap(QVariantMap &rates, const QVariantMap &source)
{
    for (auto it = source.cbegin(); it != source.cend(); ++it) {
        rates.insert(it.key(), it.value());
    }
}

} // namespace

QVariantMap CostEngine::estimate(const QVariantMap &price,
                                 const QVariantMap &usage,
                                 const QString &catalogVersion,
                                 const QString &sourceFingerprint)
{
    QVariantMap result{{QStringLiteral("available"), false},
                       {QStringLiteral("complete"), false},
                       {QStringLiteral("estimateStatus"), QStringLiteral("unavailable")},
                       {QStringLiteral("amount"), QVariant()},
                       {QStringLiteral("amountText"), QVariant()},
                       {QStringLiteral("amountMinor"), QVariant()},
                       {QStringLiteral("currency"), price.value(QStringLiteral("currency"))},
                       {QStringLiteral("priceId"), price.value(QStringLiteral("priceId"))},
                       {QStringLiteral("effectiveFrom"), price.value(QStringLiteral("effectiveFrom"))},
                       {QStringLiteral("effectiveTo"), price.value(QStringLiteral("effectiveTo"))},
                       {QStringLiteral("catalogVersion"), catalogVersion},
                       {QStringLiteral("sourceFingerprint"), sourceFingerprint},
                       {QStringLiteral("precision"), price.value(QStringLiteral("precision"))},
                       {QStringLiteral("sourceRefs"), price.value(QStringLiteral("sourceRefs"))},
                       {QStringLiteral("priceChange"), price.value(QStringLiteral("priceChange"))},
                       {QStringLiteral("lifecycle"), price.value(QStringLiteral("lifecycle"))},
                       {QStringLiteral("verificationState"), price.value(QStringLiteral("verificationState"))},
                       {QStringLiteral("missingDimensions"), QStringList{}},
                       {QStringLiteral("selectedDimensions"), QVariantMap{}},
                       {QStringLiteral("components"), QVariantList{}}};

    QStringList missing;
    const QString currency = price.value(QStringLiteral("currency")).toString().trimmed().toUpper();
    if (currency.isEmpty()) {
        addMissing(missing, QStringLiteral("currency"));
    } else if (currencyMinorDigits(currency) < 0) {
        addMissing(missing, QStringLiteral("currency:unsupported"));
    }

    const QString status = price.value(QStringLiteral("status"),
                                      price.value(QStringLiteral("pricingStatus")))
                               .toString().trimmed().toLower();
    if (price.isEmpty() || status == QLatin1String("unknown")) {
        addMissing(missing, QStringLiteral("pricing"));
    }
    const QString lifecycleStatus = price.value(QStringLiteral("lifecycleStatus")).toString().trimmed().toLower();
    if (lifecycleStatus == QLatin1String("retired")) {
        addMissing(missing, QStringLiteral("pricing:retired"));
    }
    if (price.contains(QStringLiteral("estimatesAllowed"))
        && !price.value(QStringLiteral("estimatesAllowed")).toBool()) {
        addMissing(missing, QStringLiteral("catalog:expired"));
    }

    const bool requireTimestamp = price.value(QStringLiteral("requireTimestamp"), false).toBool()
        || usage.value(QStringLiteral("requirePricingTimestamp"), false).toBool();
    const QDateTime observedAt = parseTimestamp(valueForFirstKey(
        usage, {QStringLiteral("pricingTimestamp"), QStringLiteral("observedAt"), QStringLiteral("timestamp")}));
    const QDateTime effectiveFrom = parseTimestamp(price.value(QStringLiteral("effectiveFrom")));
    const QDateTime effectiveTo = parseTimestamp(price.value(QStringLiteral("effectiveTo")));
    if (requireTimestamp && !observedAt.isValid()) {
        addMissing(missing, QStringLiteral("pricingTimestamp"));
    }
    if (observedAt.isValid()) {
        if (effectiveFrom.isValid() && observedAt < effectiveFrom) {
            addMissing(missing, QStringLiteral("pricing:before-effectiveFrom"));
        }
        if (effectiveTo.isValid() && observedAt >= effectiveTo) {
            addMissing(missing, QStringLiteral("pricing:after-effectiveTo"));
        }
    }

    QVariantList components;
    QVariantMap selectedDimensions;
    QVariantMap rates = price;
    const QString unit = price.value(QStringLiteral("unit")).toString();
    if (!unit.isEmpty() && unit != QLatin1String("1M_tokens")) {
        const QVariantMap unitUsage = usage.value(QStringLiteral("unitUsage")).toMap();
        if (!price.contains(QStringLiteral("amount"))) {
            addMissing(missing, unit);
        } else if (!unitUsage.contains(unit) || unitUsage.value(unit).isNull()) {
            addMissing(missing, unit);
        } else {
            bool quantityOk = false;
            bool unitRateOk = false;
            const WideInt quantity = decimalQuantityToRate(unitUsage.value(unit), &quantityOk);
            const WideInt unitRate = rateValue(price.value(QStringLiteral("amount")), &unitRateOk);
            if (!quantityOk || quantity < 0 || !unitRateOk) {
                addMissing(missing, unit + QLatin1String(":invalid"));
            } else {
                const WideInt amount = roundDivide(quantity * unitRate, kTokenScale);
                components.append(component(QStringLiteral("unit"), quantity, amount,
                                            price.value(QStringLiteral("amount")), unit));
            }
        }
    } else {
        const bool hasInput = usage.contains(QStringLiteral("inputTokens"))
            && !usage.value(QStringLiteral("inputTokens")).isNull();
        const bool hasOutput = usage.contains(QStringLiteral("outputTokens"))
            && !usage.value(QStringLiteral("outputTokens")).isNull();
        if (!hasInput) addMissing(missing, QStringLiteral("inputTokens"));
        if (!hasOutput) addMissing(missing, QStringLiteral("outputTokens"));

        bool inputOk = false;
        bool outputOk = false;
        const WideInt inputTokens = hasInput ? parseScaled(usage.value(QStringLiteral("inputTokens")), 1, &inputOk) : 0;
        const WideInt outputTokens = hasOutput ? parseScaled(usage.value(QStringLiteral("outputTokens")), 1, &outputOk) : 0;
        if (!inputOk || inputTokens < 0) addMissing(missing, QStringLiteral("inputTokens:invalid"));
        if (!outputOk || outputTokens < 0) addMissing(missing, QStringLiteral("outputTokens:invalid"));

        bool contextOk = true;
        const WideInt contextTokens = usage.contains(QStringLiteral("contextTokens"))
            ? parseScaled(usage.value(QStringLiteral("contextTokens")), 1, &contextOk)
            : inputTokens;
        if (!contextOk || contextTokens < 0) {
            addMissing(missing, QStringLiteral("contextTokens:invalid"));
        }
        const QVariantList contextTiers = price.value(QStringLiteral("contextTiers")).toList();
        for (const QVariant &entry : contextTiers) {
            const QVariantMap tier = entry.toMap();
            bool minimumOk = false;
            bool maximumOk = true;
            const WideInt minimum = parseScaled(tier.value(QStringLiteral("minInputTokens")), 1, &minimumOk);
            const WideInt maximum = tier.contains(QStringLiteral("maxInputTokens"))
                ? parseScaled(tier.value(QStringLiteral("maxInputTokens")), 1, &maximumOk)
                : std::numeric_limits<qint64>::max();
            if (minimumOk && maximumOk && contextTokens >= minimum && contextTokens <= maximum) {
                mergeRateMap(rates, tier);
                selectedDimensions.insert(QStringLiteral("contextTier"), tier.value(QStringLiteral("id"), tier.value(QStringLiteral("name"))));
                break;
            }
        }

        const QVariantMap modalityRates = price.value(QStringLiteral("modalityRates")).toMap();
        const bool modalityProvided = usage.contains(QStringLiteral("modality"))
            && !usage.value(QStringLiteral("modality")).toString().trimmed().isEmpty();
        const QString modality = modalityProvided
            ? usage.value(QStringLiteral("modality")).toString().trimmed().toLower()
            : QStringLiteral("text");
        selectedDimensions.insert(QStringLiteral("modality"), modality);
        if (!modalityRates.isEmpty()) {
            if (!modalityProvided) {
                addMissing(missing, QStringLiteral("modality"));
            }
            const QVariantMap selected = modalityRates.value(modality).toMap();
            if (selected.isEmpty()) addMissing(missing, QStringLiteral("modality:") + modality);
            else mergeRateMap(rates, selected);
        }

        const QVariantMap serviceTierRates = price.value(QStringLiteral("serviceTierRates")).toMap();
        const QVariantMap priorityRates = price.value(QStringLiteral("priorityRates")).toMap();
        const bool serviceTierRequired = !serviceTierRates.isEmpty() || !priorityRates.isEmpty();
        const bool serviceTierProvided = usage.contains(QStringLiteral("serviceTier"))
            && !usage.value(QStringLiteral("serviceTier")).toString().trimmed().isEmpty();
        const QString serviceTier = serviceTierProvided
            ? usage.value(QStringLiteral("serviceTier")).toString().trimmed().toLower()
            : QStringLiteral("standard");
        selectedDimensions.insert(QStringLiteral("serviceTier"), serviceTier);
        if (serviceTierRequired && !serviceTierProvided)
            addMissing(missing, QStringLiteral("serviceTier"));
        if (!serviceTierRates.isEmpty()) {
            const QVariantMap selected = serviceTierRates.value(serviceTier).toMap();
            if (selected.isEmpty()) addMissing(missing, QStringLiteral("serviceTier:") + serviceTier);
            else mergeRateMap(rates, selected);
        }
        if (serviceTier == QLatin1String("priority")) {
            const QVariantMap priority = priorityRates;
            if (priority.isEmpty()) addMissing(missing, QStringLiteral("serviceTier:priority"));
            else mergeRateMap(rates, priority);
        }

        for (const auto &dimension : {QStringLiteral("route"), QStringLiteral("region")}) {
            const QVariantMap dimensionRates = price.value(dimension + QLatin1String("Rates")).toMap();
            if (dimensionRates.isEmpty()) continue;
            const QString selected = usage.value(dimension).toString();
            selectedDimensions.insert(dimension, selected);
            const QVariantMap selectedRates = dimensionRates.value(selected).toMap();
            if (selectedRates.isEmpty()) addMissing(missing, dimension + QLatin1Char(':') + selected);
            else mergeRateMap(rates, selectedRates);
        }

        bool cachedOk = true;
        const QVariant cachedValue = valueForFirstKey(usage, {QStringLiteral("cachedInputTokens"), QStringLiteral("cacheReadTokens")});
        const WideInt cachedTokens = cachedValue.isValid()
            ? parseScaled(cachedValue, 1, &cachedOk)
            : 0;
        bool writeOk = true;
        const QVariant cacheWriteValue = valueForFirstKey(usage, {QStringLiteral("cacheWriteTokens"), QStringLiteral("cacheWriteInputTokens")});
        const WideInt cacheWriteTokens = cacheWriteValue.isValid()
            ? parseScaled(cacheWriteValue, 1, &writeOk)
            : 0;
        if (!cachedOk || cachedTokens < 0 || cachedTokens > inputTokens)
            addMissing(missing, QStringLiteral("cachedInputTokens:invalid"));
        if (!writeOk || cacheWriteTokens < 0) addMissing(missing, QStringLiteral("cacheWriteTokens:invalid"));
        if (cachedTokens + cacheWriteTokens > inputTokens)
            addMissing(missing, QStringLiteral("cacheTokens:invalid"));

        const QVariant inputRateValue = rates.value(QStringLiteral("input"));
        const QVariant outputRateValue = rates.value(QStringLiteral("output"));
        bool inputRateOk = false;
        bool outputRateOk = false;
        const WideInt inputRate = rateValue(inputRateValue, &inputRateOk);
        const WideInt outputRate = rateValue(outputRateValue, &outputRateOk);
        if (!inputRateOk) addMissing(missing, QStringLiteral("inputRate"));
        if (!outputRateOk) addMissing(missing, QStringLiteral("outputRate"));

        const QVariant cachedRateValue = valueForFirstKey(rates, {QStringLiteral("cachedInput"), QStringLiteral("cacheRead"), QStringLiteral("cachedInputRate")});
        bool cachedRateOk = false;
        const WideInt cachedRate = cachedRateValue.isValid() ? rateValue(cachedRateValue, &cachedRateOk) : 0;
        if (cachedTokens > 0 && !cachedRateOk) addMissing(missing, QStringLiteral("cachedInputRate"));

        const QVariant cacheWriteRateValue = valueForFirstKey(rates, {QStringLiteral("cacheWrite"), QStringLiteral("cacheWriteInput"), QStringLiteral("cacheWriteRate")});
        bool cacheWriteRateOk = false;
        const WideInt cacheWriteRate = cacheWriteRateValue.isValid() ? rateValue(cacheWriteRateValue, &cacheWriteRateOk) : 0;
        if (cacheWriteTokens > 0 && !cacheWriteRateOk) addMissing(missing, QStringLiteral("cacheWriteRate"));

        if (inputRateOk && inputTokens >= cachedTokens + cacheWriteTokens
            && cachedTokens >= 0 && cacheWriteTokens >= 0) {
            const WideInt billableInput = inputTokens - cachedTokens - cacheWriteTokens;
            const WideInt amount = roundDivide(billableInput * inputRate, kTokenScale);
            components.append(component(QStringLiteral("input"), billableInput, amount, inputRateValue, QStringLiteral("1M_tokens")));
        }
        if (cachedRateOk && cachedTokens > 0) {
            const WideInt amount = roundDivide(cachedTokens * cachedRate, kTokenScale);
            components.append(component(QStringLiteral("cacheRead"), cachedTokens, amount, cachedRateValue, QStringLiteral("1M_tokens")));
        }
        if (outputRateOk && outputTokens >= 0) {
            const WideInt amount = roundDivide(outputTokens * outputRate, kTokenScale);
            components.append(component(QStringLiteral("output"), outputTokens, amount, outputRateValue, QStringLiteral("1M_tokens")));
        }
        if (cacheWriteRateOk && cacheWriteTokens > 0) {
            const WideInt amount = roundDivide(cacheWriteTokens * cacheWriteRate, kTokenScale);
            components.append(component(QStringLiteral("cacheWrite"), cacheWriteTokens, amount, cacheWriteRateValue, QStringLiteral("1M_tokens")));
        }

        if (serviceTier == QLatin1String("batch")) {
            bool discountOk = false;
            const WideInt discount = parseScaled(price.value(QStringLiteral("batchDiscountPercent")), 100, &discountOk);
            if (!discountOk) {
                addMissing(missing, QStringLiteral("serviceTier:batch"));
            } else {
                selectedDimensions.insert(QStringLiteral("batchDiscountPercent"), scaledToDouble(discount, 100));
            }
        }

        const QVariantMap additiveUsage = usage.value(QStringLiteral("additiveUsage")).toMap();
        const QVariantMap allowanceConsumed = usage.value(QStringLiteral("allowanceConsumed")).toMap();
        for (const QVariant &entry : price.value(QStringLiteral("additiveFees")).toList()) {
            const QVariantMap fee = entry.toMap();
            const QString kind = fee.value(QStringLiteral("kind")).toString();
            if (!additiveUsage.contains(kind)) continue;
            bool feeRateOk = false;
            const WideInt feeRate = rateValue(fee.value(QStringLiteral("amount")), &feeRateOk);
            bool requestedOk = false;
            const WideInt requested = decimalQuantityToRate(additiveUsage.value(kind), &requestedOk);
            if (!feeRateOk || !requestedOk) {
                addMissing(missing, QStringLiteral("additiveFee:") + kind);
                continue;
            }
            bool freeOk = false;
            const WideInt freeAllowance = decimalQuantityToRate(fee.value(QStringLiteral("freeAllowance"), 0), &freeOk);
            if (!freeOk) {
                addMissing(missing, QStringLiteral("freeAllowance:") + kind);
                continue;
            }
            WideInt consumed = 0;
            if (allowanceConsumed.contains(kind)) {
                bool consumedOk = false;
                consumed = decimalQuantityToRate(allowanceConsumed.value(kind), &consumedOk);
                if (!consumedOk) addMissing(missing, QStringLiteral("allowanceConsumed:") + kind);
            } else if (freeAllowance > 0) {
                addMissing(missing, QStringLiteral("allowanceConsumed:") + kind);
            }
            const WideInt remainingFree = qMax<WideInt>(0, freeAllowance - consumed);
            const WideInt billable = qMax<WideInt>(0, requested - remainingFree);
            const WideInt amount = roundDivide(billable * feeRate, kTokenScale);
            components.append(component(QStringLiteral("fee:") + kind, billable, amount,
                                        fee.value(QStringLiteral("amount")), kind));
        }
    }

    WideInt subtotal = 0;
    for (const QVariant &entry : components) {
        bool componentOk = false;
        const WideInt amount = rateValue(entry.toMap().value(QStringLiteral("amountText")), &componentOk);
        if (componentOk) subtotal += amount;
    }

    if (selectedDimensions.value(QStringLiteral("serviceTier")).toString() == QLatin1String("batch")
        && price.contains(QStringLiteral("batchDiscountPercent"))) {
        bool discountOk = false;
        const WideInt discount = parseScaled(price.value(QStringLiteral("batchDiscountPercent")), 100, &discountOk);
        if (discountOk) subtotal = roundDivide(subtotal * (100 * 100 - discount), 100 * 100);
    }

    result[QStringLiteral("currency")] = currency.isEmpty() ? QVariant() : QVariant(currency);
    result[QStringLiteral("components")] = components;
    result[QStringLiteral("selectedDimensions")] = selectedDimensions;
    result[QStringLiteral("missingDimensions")] = missing;
    if (!missing.isEmpty()) {
        result[QStringLiteral("reason")] = QStringLiteral("missing-or-invalid-pricing-dimensions");
        return result;
    }

    const int minorDigits = currencyMinorDigits(currency);
    qint64 minorScale = 1;
    for (int i = 0; i < minorDigits; ++i) {
        minorScale *= 10;
    }
    const WideInt divisor = minorDigits >= 0
        ? static_cast<WideInt>(kRateScale / minorScale)
        : 1;
    const WideInt amountMinor = roundDivide(subtotal, divisor);
    if (amountMinor > std::numeric_limits<qint64>::max() || amountMinor < std::numeric_limits<qint64>::min()) {
        result[QStringLiteral("missingDimensions")] = QStringList{QStringLiteral("amount:overflow")};
        result[QStringLiteral("reason")] = QStringLiteral("amount-overflow");
        return result;
    }
    const qint64 minor = static_cast<qint64>(amountMinor);
    const QString amountText = scaledToText(minor, minorScale);
    result[QStringLiteral("available")] = true;
    result[QStringLiteral("complete")] = true;
    result[QStringLiteral("estimateStatus")] = QStringLiteral("available");
    result[QStringLiteral("amountMinor")] = minor;
    result[QStringLiteral("amount")] = static_cast<double>(minor) / static_cast<double>(minorScale);
    result[QStringLiteral("amountText")] = amountText;
    result[QStringLiteral("unroundedAmountText")] = scaledToText(subtotal, kRateScale);
    return result;
}
