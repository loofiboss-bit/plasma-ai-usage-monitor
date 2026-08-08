#include "currencyminorunits.h"

#include <QSet>

#include <cmath>
#include <limits>

namespace {
const QSet<QString> &twoDigitCurrencies() {
  static const QSet<QString> currencies = [] {
    const QString codes =
        QStringLiteral("AED AFN ALL AMD AOA ARS AUD AWG AZN BAM BBD BDT BMD "
                       "BND BOB BOV BRL BSD BTN BWP BYN BZD CAD "
                       "CDF CHE CHF CHW CNY COP COU CRC CUP CVE CZK DKK DOP "
                       "DZD EGP ERN ETB EUR FJD FKP GBP GEL GHS "
                       "GIP GMD GTQ GYD HKD HNL HTG HUF IDR ILS INR IRR JMD "
                       "KES KGS KHR KPW KYD KZT LAK LBP LKR "
                       "LRD LSL MAD MDL MGA MKD MMK MNT MOP MRU MUR MVR MWK "
                       "MXN MXV MYR MZN NAD NGN NIO NOK NPR "
                       "NZD PAB PEN PGK PHP PKR PLN QAR RON RSD RUB SAR SBD "
                       "SCR SDG SEK SGD SHP SLE SOS SRD SSP STN "
                       "SVC SYP SZL THB TJS TMT TOP TRY TTD TWD TZS UAH USD "
                       "USN UYU UZS VED VES WST XCD XCG YER ZAR ZMW ZWG");
    const QStringList entries = codes.split(QLatin1Char(' '));
    return QSet<QString>(entries.cbegin(), entries.cend());
  }();
  return currencies;
}
} // namespace

std::optional<int> CurrencyMinorUnits::digits(const QString &currency) {
  const QString code = currency.trimmed().toUpper();
  static const QSet<QString> zeroDigits = {
      QStringLiteral("BIF"), QStringLiteral("CLP"), QStringLiteral("DJF"),
      QStringLiteral("GNF"), QStringLiteral("ISK"), QStringLiteral("JPY"),
      QStringLiteral("KMF"), QStringLiteral("KRW"), QStringLiteral("PYG"),
      QStringLiteral("RWF"), QStringLiteral("UGX"), QStringLiteral("UYI"),
      QStringLiteral("VND"), QStringLiteral("VUV"), QStringLiteral("XAF"),
      QStringLiteral("XOF"), QStringLiteral("XPF")};
  static const QSet<QString> threeDigits = {
      QStringLiteral("BHD"), QStringLiteral("IQD"), QStringLiteral("JOD"),
      QStringLiteral("KWD"), QStringLiteral("LYD"), QStringLiteral("OMR"),
      QStringLiteral("TND")};
  static const QSet<QString> fourDigits = {QStringLiteral("CLF"),
                                           QStringLiteral("UYW")};
  if (zeroDigits.contains(code))
    return 0;
  if (threeDigits.contains(code))
    return 3;
  if (fourDigits.contains(code))
    return 4;
  if (twoDigitCurrencies().contains(code))
    return 2;
  return std::nullopt;
}

std::optional<qint64> CurrencyMinorUnits::fromMajor(double value,
                                                    const QString &currency) {
  const std::optional<int> precision = digits(currency);
  if (!precision || !std::isfinite(value))
    return std::nullopt;
  const double factor = std::pow(10.0, *precision);
  const double scaled = value * factor;
  if (scaled < static_cast<double>(std::numeric_limits<qint64>::min()) ||
      scaled > static_cast<double>(std::numeric_limits<qint64>::max()))
    return std::nullopt;
  return static_cast<qint64>(std::llround(scaled));
}

std::optional<double> CurrencyMinorUnits::toMajor(qint64 value,
                                                  const QString &currency) {
  const std::optional<int> precision = digits(currency);
  if (!precision)
    return std::nullopt;
  return static_cast<double>(value) / std::pow(10.0, *precision);
}
