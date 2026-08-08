#ifndef CURRENCYMINORUNITS_H
#define CURRENCYMINORUNITS_H

#include <QString>

#include <optional>

class CurrencyMinorUnits final {
public:
  static std::optional<int> digits(const QString &currency);
  static std::optional<qint64> fromMajor(double value, const QString &currency);
  static std::optional<double> toMajor(qint64 value, const QString &currency);
};

#endif
