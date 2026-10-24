# Bolt's Performance Journal

## 2026-10-23 - Dynamic QRegularExpression re-compilation in C++ hot paths
**Learning:** In Qt/C++ parsing paths like `CostEngine::parseScaled`, `LocalSessionParser::safeModel`, and `LocalSessionModel::setProjectAlias`, constructing `QRegularExpression` on every function invocation allocates temporary `QRegularExpressionPrivate` instances on the heap and re-compiles regex patterns. For simple ASCII digit matching, `isAsciiDigits` eliminates regex overhead completely, while `static const QRegularExpression` compiles regex patterns once at initialization for complex patterns.
**Action:** When validating numbers or string identifiers in hot paths, use direct character inspection for simple digit/hex patterns, or `static const QRegularExpression` for complex patterns.
