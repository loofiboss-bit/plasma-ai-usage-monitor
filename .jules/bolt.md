## 2026-03-31 - Static QRegularExpression in Hot Numeric and Session Parsers

**Learning:** Qt's `QRegularExpression` compiles patterns and allocates matcher state on instantiation. When used inside functions called repeatedly during numeric parsing (`CostEngine::parseScaled`), session record parsing (`LocalSessionParser::safeModel`), or policy validation (`BudgetPolicyRepository::normalizedPolicy`), temporary regex objects cause significant heap allocation and compilation overhead per token observation or log record.

**Action:** Always prefer `static const QRegularExpression` inside hot C++ utility functions in this plugin to ensure regex patterns are compiled once at initialization time.
