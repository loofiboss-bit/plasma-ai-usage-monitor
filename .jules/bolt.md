## 2026-03-30 - Avoid QRegularExpression in parseScaled hot loops

**Learning:** `QRegularExpression` constructed inline within numeric scaling and parsing functions (`parseScaled`) creates and compiles regex patterns on every single value evaluation. In cost estimation hot paths, replacing regex digit checks with `isAllDigits` (`QStringView`) and replacing `QString::number(scale)` with integer math eliminates dynamic allocations and regex compilation overhead.
**Action:** Use fast character range comparisons (`isAllDigits`) or `static const QRegularExpression` when validating formats in hot paths instead of constructing inline `QRegularExpression` instances.
