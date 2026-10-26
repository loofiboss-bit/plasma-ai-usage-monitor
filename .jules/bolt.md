## 2026-10-26 - Eliminate Temporary QRegularExpression Instantiation on Hot Paths
**Learning:** Dynamic instantiation of `QRegularExpression` inside high-frequency functions like `CostEngine::parseScaled`, `normalizedPolicy`, and `setProjectAlias` causes significant CPU and heap allocation overhead due to repeated regex compilation on every evaluation.
**Action:** Replace temporary `QRegularExpression` matches with zero-allocation character inspection helpers (e.g. `isDigitsOnly`, `isIso4217CurrencyCode`, `isValidHex64`) or static constexpr instances when regex is necessary.
